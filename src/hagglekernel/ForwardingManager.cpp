/* Copyright 2008-2009 Uppsala University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#include <libcpphaggle/Platform.h>
#include "ForwardingManager.h"
#include "ForwarderProphet.h"

ForwardingManager::ForwardingManager(HaggleKernel * _kernel) :
	Manager("ForwardingManager", _kernel), 
	dataObjectQueryCallback(NULL), 
	nodeQueryCallback(NULL), 
	repositoryCallback(NULL),
	moduleEventType(-1),
	routingInfoEventType(-1),
	periodicDataObjectQueryEventType(-1),
	periodicDataObjectQueryEvent(NULL),
	periodicDataObjectQueryInterval(300),
	forwardingModule(NULL),
#if defined(ENABLE_RECURSIVE_ROUTING_UPDATES)
	recursiveRoutingUpdates(false),
#endif	
	doQueryOnNewDataObject(true)
{
}

bool ForwardingManager::init_derived()
{
	int ret;
#define __CLASS__ ForwardingManager

	ret = setEventHandler(EVENT_TYPE_NODE_UPDATED, onNodeUpdated);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}
	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_NEW, onNewDataObject);
	
	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}
	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_FORWARD, onDataObjectForward);
	
	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}
	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL, onSendDataObjectResult);
	
	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}
	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_SEND_FAILURE, onSendDataObjectResult);
	
	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}
	ret = setEventHandler(EVENT_TYPE_NODE_CONTACT_NEW, onNewNeighbor);
	
	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}
	ret = setEventHandler(EVENT_TYPE_NODE_CONTACT_END, onEndNeighbor);
	
	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}
	ret = setEventHandler(EVENT_TYPE_TARGET_NODES, onTargetNodes);
	
	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}
	ret = setEventHandler(EVENT_TYPE_DELEGATE_NODES, onDelegateNodes);
	
	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}

#if defined(DEBUG)
	setEventHandler(EVENT_TYPE_DEBUG_CMD, onDebugCmd);
#endif
	moduleEventType = registerEventType(getName(), onForwardingTaskComplete);
	
	if (moduleEventType < 0) {
		HAGGLE_ERR("Could not register module event type...");
		return false;
	}
	// Register filter for forwarding objects
	registerEventTypeForFilter(routingInfoEventType, "Forwarding", onRoutingInformation, "Forwarding=*");
	
	dataObjectQueryCallback = newEventCallback(onDataObjectQueryResult);
	delayedDataObjectQueryCallback = newEventCallback(onDelayedDataObjectQuery);
	nodeQueryCallback = newEventCallback(onNodeQueryResult);
	repositoryCallback = newEventCallback(onRepositoryData);
	periodicDataObjectQueryEventType = registerEventType("Periodic DataObject Query Event", 
				       onPeriodicDataObjectQuery);

	if (periodicDataObjectQueryEventType < 0) {
		HAGGLE_ERR("Could not register periodic data object query event type...\n");
		return false;
	}

	periodicDataObjectQueryEvent = new Event(periodicDataObjectQueryEventType);

	if (!periodicDataObjectQueryEvent)
		return false;

	periodicDataObjectQueryEvent->setAutoDelete(false);

	forwardingModule = new ForwarderProphet(this, moduleEventType);

	if (!forwardingModule) {
		HAGGLE_ERR("Could not create forwarding module\n");
		return false;
	}

	HAGGLE_DBG("Created forwarding module \'%s\'\n", forwardingModule->getName());

	return true;
}

ForwardingManager::~ForwardingManager()
{
	// Destroy the forwarding module:
	if (forwardingModule) {
		HAGGLE_ERR("ERROR: forwarding module detected in forwarding manager "
			"destructor. This shouldn't happen.");
		forwardingModule->quit();
		delete forwardingModule;
	}
	
	if (dataObjectQueryCallback)
		delete dataObjectQueryCallback;
	
	// Empty the list of forwarded data objects.
	while (!forwardedObjects.empty()) {
		HAGGLE_ERR("Clearing unsent data object.\n");
		forwardedObjects.pop_front();
	}
	if (nodeQueryCallback)
		delete nodeQueryCallback;
	
	if (repositoryCallback)
		delete repositoryCallback;
	
	if (delayedDataObjectQueryCallback)
		delete delayedDataObjectQueryCallback;
		
	Event::unregisterType(moduleEventType);

	if (periodicDataObjectQueryEvent) {
		if (periodicDataObjectQueryEvent->isScheduled())
			periodicDataObjectQueryEvent->setAutoDelete(true);
		else
			delete periodicDataObjectQueryEvent;
	}

	Event::unregisterType(periodicDataObjectQueryEventType);
}

void ForwardingManager::onPrepareStartup()
{
	/*
		If there is an active forwarding module, request its state from the repository.
		The state will be read in the callback, and the module's thread will be started
		after the state has been saved in the module.
	 */
	if (forwardingModule) {
		kernel->getDataStore()->readRepository(new RepositoryEntry(forwardingModule->getName()), repositoryCallback);
	}
}

void ForwardingManager::onPrepareShutdown()
{	
	unregisterEventTypeForFilter(routingInfoEventType);

	// Save the forwarding module's state
	if (forwardingModule) {
		if (forwardingModule->isRunning())
			// Delay signaling we are ready for shutdown until the running
			// module tells us it is done
			setForwardingModule(NULL);
		else {
			// The forwarding module exists, but is not running,
			// delete it and signal that we are ready
			delete forwardingModule;
			signalIsReadyForShutdown();
		}
	} else {
		signalIsReadyForShutdown();
	}
}

void ForwardingManager::setForwardingModule(Forwarder *f, bool deRegisterEvents)
{
	// Is there a current forwarding module?
	if (forwardingModule) {
		if (f && strcmp(forwardingModule->getName(), f->getName()) == 0) {
			HAGGLE_DBG("Forwarder %s already set!\n", f->getName());
			delete f;
			f = NULL;
			return;
		}
		// Tell the module to quit
		forwardingModule->quit();

		// delete it:
		delete forwardingModule;
	}
	// Change forwarding module:
	forwardingModule = f;
	
	// Is there a new forwarding module?
	if (forwardingModule) {
		/*
		 Yes, request its state from the repository.
		 The state will be read in the callback, and the module's thread will be started
		 after the state has been saved in the module.
		 */
		kernel->getDataStore()->readRepository(new RepositoryEntry(forwardingModule->getName()), repositoryCallback);
		HAGGLE_DBG("Set new forwarding module to \'%s'\n", forwardingModule->getName());
		LOG_ADD("# %s: forwarding module is \'%s'\n", getName(), forwardingModule->getName());
		
		/* Register callbacks */
		if (!getEventInterest(EVENT_TYPE_DATAOBJECT_NEW)) {
			setEventHandler(EVENT_TYPE_DATAOBJECT_NEW, onNewDataObject);
		}
		if (!getEventInterest(EVENT_TYPE_NODE_UPDATED)) {
			setEventHandler(EVENT_TYPE_NODE_UPDATED, onNodeUpdated);
		}
		if (!getEventInterest(EVENT_TYPE_NODE_CONTACT_NEW)) {
			setEventHandler(EVENT_TYPE_NODE_CONTACT_NEW, onNewNeighbor);
		}
	} else {
		HAGGLE_DBG("Set new forwarding module to \'NULL'\n");
		LOG_ADD("# %s: forwarding module is \'NULL'\n", getName());
		
		if (deRegisterEvents) {
			
			HAGGLE_DBG("Deregistering events EVENT_TYPE_DATAOBJECT_NEW EVENT_TYPE_NODE_UPDATED EVENT_TYPE_NODE_CONTACT_NEW\n");
			// remove interest in new data objects to avoid resolution
			removeEventHandler(EVENT_TYPE_DATAOBJECT_NEW);
			removeEventHandler(EVENT_TYPE_NODE_UPDATED);
			removeEventHandler(EVENT_TYPE_NODE_CONTACT_NEW);
		}
	}
}

void ForwardingManager::onShutdown()
{
	// Set the current forwarding module to none. See setForwardingModule().
	unregisterWithKernel();
}

void ForwardingManager::onForwardingTaskComplete(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	ForwardingTask *task = static_cast < ForwardingTask * >(e->getData());
	
	if (!task)
		return;
	
	//HAGGLE_DBG("Got event type %u\n", task->getType());
	
	switch (task->getType()) {
		/*
		 QUIT: save the modules state which is returned in the task.
		 */
		case FWD_TASK_QUIT:
		{
			RepositoryEntryList *rel = task->getRepositoryEntryList();
			
			if (rel) {
				HAGGLE_DBG("Forwarding module QUIT, saving its state... (%lu entries)\n", rel->size());
				
				for (RepositoryEntryList::iterator it = rel->begin(); it != rel->end(); it++) {
					kernel->getDataStore()->insertRepository(*it);
				}
			}
			/*
				If we are preparing for shutdown, we should also signal
				that we are ready.
			 */
			if (getState() == MANAGER_STATE_PREPARE_SHUTDOWN) {
				signalIsReadyForShutdown();
			}
		}
			break;
		case FWD_TASK_GENERATE_ROUTING_INFO_DATA_OBJECT:
			if (isNeighbor(task->getNode())) {
				if (forwardingModule) {					
					// Send the neighbor our forwarding metric if we have one
					if (task->getDataObject()) {
						HAGGLE_DBG("Sending routing information to %s\n", 
							   task->getNode()->getName().c_str());
						
						if (shouldForward(task->getDataObject(), task->getNode())) {
							if (addToSendList(task->getDataObject(), task->getNode())) {
#if defined(ENABLE_RECURSIVE_ROUTING_UPDATES)
								// Check if this is a recursive update, and we have a list
								// of nodes that have received it already.
								if (task->getNodeList()) {
									// Append the list to the data object
									recurseListToMetadata(task->getDataObject()->getMetadata()->getMetadata(getName()), 
											      *task->getNodeList());
								}
#endif
								kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, 
											   task->getDataObject(), task->getNode()));
							}
						} else {
							HAGGLE_ERR("Could not send routing information to neighbor\n");
						}
					}
				}
			}
			default:
				break;
	}
	delete task;
}

void ForwardingManager::onRepositoryData(Event *e)
{
	DataStoreQueryResult *qr = NULL;
	
	if (!e || !e->getData())
		goto out;
	
	HAGGLE_DBG("Got repository callback\n");
	
	// This event either reports the forwarding module's state
	qr = static_cast < DataStoreQueryResult * >(e->getData());
	
	if (forwardingModule) {		
		if (qr->countRepositoryEntries() > 0) {
			unsigned int n = 0;
			// Then this is most likely the forwarding module's state:
			RepositoryEntryRef re;
			
			while ((re = qr->detachFirstRepositoryEntry())) {
				// Was there a repository entry? => was this really what we expected?
				if (strcmp(re->getAuthority(), forwardingModule->getName()) == 0) {
					forwardingModule->setSaveState(re);
					n++;
				}
			}
			HAGGLE_DBG("Restored %u entries of saved state for module \'%s\'\n", 
				   n, forwardingModule->getName());
		}
	} else {
		HAGGLE_ERR("No forwarding module set for when retreiving saved state\n");
	}
	
	delete qr;
out:
	if (forwardingModule) {
		// It is now safe to start the module thread
		HAGGLE_DBG("Starting forwarding module \'%s\'\n", forwardingModule->getName());
		forwardingModule->start();
	}
	// Only send ready signal if this callback was made while still in startup
	if (!isStartupComplete())
		signalIsReadyForStartup();
}

#ifdef DEBUG
void ForwardingManager::onDebugCmd(Event *e)
{
	if (e) {
		if (e->getDebugCmd()->getType() == DBG_CMD_PRINT_ROUTING_TABLE) {
			if (forwardingModule)
				forwardingModule->printRoutingTable();
			else
				printf("No forwarding module");
		}
	}
}
#endif

/*
  A wrapper around node->isNeighbor() that handles application nodes
  and checks that a node retrieved from the data store is really a
  neighbor (i.e., a node from the data store may have inaccurate
  interface information and therefore has to be checked against a
  potential neighbor node in the node store).
 */
bool ForwardingManager::isNeighbor(const NodeRef& node)
{
	if (node->isNeighbor())
		return true;

	if (node && node->getType() == Node::TYPE_PEER) {
		/*
		 WARNING! Previously, kernel->getNodeStore()->retrieve(node,...) was
		 called on the same line as node->getType(), i.e.,
		 
		 if (node && node->getType() == Node::TYPE_PEER && kernel->getNodeStore()->retrieve(node, true))
			...
		 
		 but this could cause a potential deadlock in the NodeStore(). The first
		 call to getType() will lock the node (through the use of the reference auto
		 locking) and that lock will remain until the next line is executed. If
		 getNodeStore()->retrieve() is called on the same line, the node will hence
		 be in a locked state when accessing the node store, which is forbidden
		 due to the risk of deadlock (see separate note in NodeStore.{h,cpp}).
		 */
		if (kernel->getNodeStore()->retrieve(node, true))
			return true;
	}
        return false;
}
/*
   This function verifies that a node is a valid receiver for a
   specific data object, e.g., that the node has not already received
   the data object.
 */

bool ForwardingManager::addToSendList(DataObjectRef& dObj, const NodeRef& node, int repeatCount)
{
 	// Check if the data object/node pair is already in our send list:
        for (forwardingList::iterator it = forwardedObjects.begin();
             it != forwardedObjects.end(); it++) {
                // Is this it?
                if ((*it).first.first == dObj &&
                    (*it).first.second == node) {
                        // Yep. Do not forward this.
                        HAGGLE_DBG("Data object already in send list for node '%s'\n",
                                   node->getName().c_str());
                        return false;
                }
        }
	
        // Remember that we tried to send this:
        forwardedObjects.push_front(Pair< Pair<const DataObjectRef, const NodeRef>,int>(Pair<const DataObjectRef, const NodeRef>(dObj,node), repeatCount));
        
        return true;
}

bool ForwardingManager::shouldForward(const DataObjectRef& dObj, const NodeRef& node)
{
        NodeRef peer;
        
	if (!node) {
		HAGGLE_ERR("node is NULL\n");
		return false;
	}
	
	if (dObj->isNodeDescription()) {
		NodeRef descNode = Node::create(Node::TYPE_PEER, dObj);
		
		if (descNode) {
			if (descNode == node) {
				// Do not send the peer its own node description
				HAGGLE_DBG("Data object [%s] is peer %s's node description. - not sending!\n", 
					dObj->getIdStr(), node->getName().c_str());
				return false;
			}
		}
	}
	
	HAGGLE_DBG("%s Checking if data object %s should be forwarded to node %s (%s num=%lu)\n", 
		getName(), dObj->getIdStr(), node->getName().c_str(), 
		node->isStored() ? "stored" : "not stored", node->getNum());
	
        // Make sure we use the node in the node store
        peer = kernel->getNodeStore()->retrieve(node, false);

	if (!peer) {
		peer = node;
	} 

	if (peer->getBloomfilter()->has(dObj)) {
		HAGGLE_DBG("%s node %s [%s] already has data object [%s]\n", 
			getName(), peer->getName().c_str(), peer->getIdStr(), dObj->getIdStr());
	} else {
		return true;
	}

	return false;
}

void ForwardingManager::forwardByDelegate(DataObjectRef &dObj, const NodeRef &target, const NodeRefList *other_targets)
{
	if (forwardingModule)
		forwardingModule->generateDelegatesFor(dObj, target, other_targets);
}

void ForwardingManager::onDataObjectForward(Event *e)
{
	// Get the data object:
	DataObjectRef dObj = e->getDataObject();

	if (!dObj) {
		HAGGLE_ERR("Someone posted an EVENT_TYPE_DATAOBJECT_FORWARD event without a data object.\n");
		return;
	}

	// Get the node:
	NodeRef& target = e->getNode();

	if (!target) {
		HAGGLE_ERR("Someone posted an EVENT_TYPE_DATAOBJECT_FORWARD event without a node.\n");
		return;
	}
	
	// Start forwarding the object:
	if (shouldForward(dObj, target)) {
		// Ok, the target wants the data object. Now check if
		// the target is a neighbor, in which case the data
		// object is sent directly to the target, otherwise
		// ask the forwarding module to generate delegates.
		if (isNeighbor(target)) {
			if (addToSendList(dObj, target)) {
				HAGGLE_DBG("Sending data object %s directly to target neighbor %s\n", 
					dObj->getIdStr(), target->getName().c_str());
				kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, dObj, target));
			}
		} else {
			HAGGLE_DBG("Trying to find delegates for data object %s bound for target %s\n", 
				dObj->getIdStr(), target->getName().c_str());
			forwardByDelegate(dObj, target);
		}
	}
}

void ForwardingManager::onSendDataObjectResult(Event *e)
{
	DataObjectRef& dObj = e->getDataObject();
	NodeRef& node = e->getNode();

        HAGGLE_DBG("Checking data object results\n");

	// Find the data object in our send list:
	for (forwardingList::iterator it = forwardedObjects.begin();
		it != forwardedObjects.end(); it++) {
		
		if ((*it).first.first == dObj && (*it).first.second == node) {
			if (e->getType() == EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL) {
				// Remove the data object - it has been forwarded.
				forwardedObjects.erase(it);
			} else if (e->getType() == EVENT_TYPE_DATAOBJECT_SEND_FAILURE) {
				int repeatCount;
				repeatCount = (*it).second + 1;
				// Remove this from the list. It may be reinserted later.
				forwardedObjects.erase(it);
				switch (repeatCount) {
					case 1:
						// This was the first attempt. Try resending the 
						// data object:
						if (isNeighbor(node) && shouldForward(dObj, node) && addToSendList(dObj, node, repeatCount)) {
							kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, dObj, node));
						}
					break;
					
					default:
						// Do nothing. This object has already failed once 
						// before - give up.
					break;
				}
			}
			// No need to search this list further (the "it" variable is 
			// garbage anyway...)
			goto found_it;
		}
	}

        HAGGLE_DBG("Data object result done\n");
found_it:;
	// Done.
}

void ForwardingManager::onDataObjectQueryResult(Event *e)
{
	if (!e || !e->hasData()) {
		HAGGLE_ERR("Error: No data in data store query result!\n");
		return;
	}

	DataStoreQueryResult *qr = static_cast<DataStoreQueryResult*>(e->getData());
	NodeRef target = qr->detachFirstNode();

	if (!target) {
		HAGGLE_DBG("No peer node in query result\n");
		delete qr;
		return;
	}
	
	HAGGLE_DBG("Got dataobject query result for target node %s\n", target->getIdStr());
	
	DataObjectRef dObj;

	while (dObj = qr->detachFirstDataObject()) {
		// Does this target already have this data object, or
		// is the data object its node description? shouldForward() tells us.
		if (shouldForward(dObj, target)) {
			// Is this node a currently available neighbor node?
			if (isNeighbor(target)) {
				// Yes: it is it's own best delegate,
				// so start "forwarding" the object:
				
				if (addToSendList(dObj, target)) {
					kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, dObj, target));
					HAGGLE_DBG("Sending data object %s directly to target neighbor %s\n", 
						   dObj->getIdStr(), target->getName().c_str());
				}
			} else { 
				HAGGLE_DBG("Trying to find delegates for data object %s bound for target %s\n", 
					   dObj->getIdStr(), target->getName().c_str());
				forwardByDelegate(dObj, target);
			}
		}
	}
        
	delete qr;
}

void ForwardingManager::onNodeQueryResult(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	DataStoreQueryResult *qr = static_cast < DataStoreQueryResult * >(e->getData());
	DataObjectRef dObj = qr->detachFirstDataObject();
	const NodeRefList *targets = qr->getNodeList();

	if (!dObj) {
		HAGGLE_DBG("No dataobject in query result\n");
		delete qr;
		return;
	}
	if (!targets || targets->empty()) {
		HAGGLE_ERR("No nodes in query result\n");
		delete qr;
		return;
	}

	HAGGLE_DBG("Got node query result for dataobject %s with %lu nodes\n", dObj->getIdStr(), targets->size());
	
	NodeRefList target_neighbors;

	for (NodeRefList::const_iterator it = targets->begin(); it != targets->end(); it++) {
		const NodeRef& target = *it;
                 // Is this node a currently available neighbor node?
                if (shouldForward(dObj, target)) {
                        if (isNeighbor(target)) {
                                // Yes: it is it's own best delegate,
                                // so start "forwarding" the object:
                                if (addToSendList(dObj, target)) {
                                        HAGGLE_DBG("Sending data object %s directly to target neighbor %s\n", 
						dObj->getIdStr(), target->getName().c_str());
                                        target_neighbors.push_front(target);
                                }
                        } else if (target->getType() == Node::TYPE_PEER || 
				   target->getType() == Node::TYPE_GATEWAY) { 
                                HAGGLE_DBG("Trying to find delegates for data object %s bound for target %s\n", 
					dObj->getIdStr(), target->getName().c_str());
                                forwardByDelegate(dObj, target, targets);
                        }
                }
	}

	if (!target_neighbors.empty()) {
		kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, dObj, target_neighbors));
	}
	
	delete qr;
}

void ForwardingManager::onDelayedDataObjectQuery(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	NodeRef node = e->getNode();

	// If the node is still in the node query list, we know that
	// there has been no update for it since we generated this
	// delayed call.
	for (List<NodeRef>::iterator it = pendingQueryList.begin(); 
	     it != pendingQueryList.end(); it++) {
		if (node == *it) {
			pendingQueryList.erase(it);
			findMatchingDataObjectsAndTargets(node);
			break;
		}
	}
}

void ForwardingManager::onPeriodicDataObjectQuery(Event *e)
{
	NodeRefList neighbors;
	Timeval now = Timeval::now();
	double nextTimeout = periodicDataObjectQueryInterval;
	
	kernel->getNodeStore()->retrieveNeighbors(neighbors);

	for (NodeRefList::iterator it = neighbors.begin(); it != neighbors.end(); it++) {
		NodeRef& neigh = *it;
		
		if ((now - neigh->getLastDataObjectQueryTime()) > 
		    periodicDataObjectQueryInterval) {
			HAGGLE_DBG("Periodic data object query for neighbor %s\n",
				   neigh->getName().c_str());
			findMatchingDataObjectsAndTargets(neigh);
		} else if ((now - neigh->getLastDataObjectQueryTime()) < nextTimeout) {
			nextTimeout = (now - neigh->getLastDataObjectQueryTime()).getSeconds();
		}
	}
	
	if (kernel->getNodeStore()->numNeighbors() > 0 &&
	    !periodicDataObjectQueryEvent->isScheduled() &&
	    periodicDataObjectQueryInterval > 0) {
		periodicDataObjectQueryEvent->setTimeout(nextTimeout);
		kernel->addEvent(periodicDataObjectQueryEvent);
	}
}

void ForwardingManager::onNewNeighbor(Event *e)
{
	if (getState() > MANAGER_STATE_RUNNING) {
		HAGGLE_DBG("In shutdown, ignoring new neighbor\n");
		return;
	}
	
	NodeRef node = e->getNode();
	
	if (node->getType() == Node::TYPE_UNDEFINED)
		return;
	
	// Tell the forwarding module that we've got a new neighbor:
	if (forwardingModule) {
		forwardingModule->newNeighbor(node);		
		forwardingModule->generateRoutingInformationDataObject(node);
	}
	// Find matching data objects for the node and figure out whether it is a good
	// delegate. But delay these operations in case we get a node update event for the
	// same node due to receiving a new node description for it. If we get the
	// the update, we should only do the query once using the updated information.
	pendingQueryList.push_back(node);
	kernel->addEvent(new Event(delayedDataObjectQueryCallback, node, 5));
	
	HAGGLE_DBG("%s - new node contact with %s [id=%s]. Delaying data object query in case there is an incoming node description for the node\n", 
		   getName(), node->getName().c_str(), node->getIdStr());


	/* Start periodic data object query if configuration allows. */
	if (kernel->getNodeStore()->numNeighbors() == 1 &&
	    !periodicDataObjectQueryEvent->isScheduled() &&
	    periodicDataObjectQueryInterval > 0) {
		periodicDataObjectQueryEvent->setTimeout(periodicDataObjectQueryInterval);
		kernel->addEvent(periodicDataObjectQueryEvent);
	}
}

void ForwardingManager::onEndNeighbor(Event *e)
{	
	if (e->getNode()->getType() == Node::TYPE_UNDEFINED)
		return;
	
	NodeRef node = e->getNode();

	// Tell the forwarding module that the neighbor went away
	if (forwardingModule)
		forwardingModule->endNeighbor(node);

	// Cancel any queries for this node in the data store since they are no longer
	// needed
	kernel->getDataStore()->cancelDataObjectQueries(node);

	// Also remove from pending query list so that
	// onDelayedDataObjectQuery won't generate a delayed query
	// after the node went away
	for (List<NodeRef>::iterator it = pendingQueryList.begin(); it != pendingQueryList.end(); it++) {
		if (node == *it) {
			pendingQueryList.erase(it);
			break;
		}
	}
#if defined(ENABLE_RECURSIVE_ROUTING_UPDATES)
	if (recursiveRoutingUpdates) {
		// Trigger a new routing update to inform our other neighbors about our new metrics
		recursiveRoutingUpdate(node, NULL);
	}
#endif
}

void ForwardingManager::onNodeUpdated(Event *e)
{
	if (!e || !e->hasData())
		return;

	NodeRef node = e->getNode();
	NodeRefList &replaced = e->getNodeList();
	
	if (node->getType() == Node::TYPE_UNDEFINED) {
		HAGGLE_DBG("%s Node is undefined, deferring dataObjectQuery\n", getName());
		return;
	} 
	
	HAGGLE_DBG("%s - got node update for %s [id=%s]\n", 
		   getName(), node->getName().c_str(), node->getIdStr());

	// Did this updated node replace an undefined node?
	// Go through the replaced nodes to find out...
	NodeRefList::iterator it = replaced.begin();
	
	while (it != replaced.end()) {
		// Was this undefined?
		if ((*it)->getType() == Node::TYPE_UNDEFINED && node->isNeighbor()) {
			// Yep. Tell the forwarding module that we've got a new neighbor:
			if (forwardingModule) {
				forwardingModule->newNeighbor(node);
				forwardingModule->generateRoutingInformationDataObject(node);
			}
			
			break;
		}
		it++;
	}
	
	// Check if there are any pending node queries that have been initiated by a previous
	// new node contact event (in onNewNeighbor). In that case, remove the node from the
	// pendingQueryList so that we do not generate the query twice.
	for (List<NodeRef>::iterator it = pendingQueryList.begin(); it != pendingQueryList.end(); it++) {
		if (node == *it) {
			pendingQueryList.erase(it);
			break;
		}
	}
	findMatchingDataObjectsAndTargets(node);
}

void ForwardingManager::findMatchingDataObjectsAndTargets(NodeRef& node)
{
	if (!node || node->getType() == Node::TYPE_UNDEFINED)
		return;
	
	// Check that this is an active neighbor node we can send to:
	if (node->isNeighbor()) {
		// Ask the forwarding module for additional target nodes for which 
		// this neighbor can act as delegate.
		
		if (forwardingModule) {
			HAGGLE_DBG("%s trying to find targets for which neighbor %s [id=%s] is a good delegate\n", 
				 getName(), node->getName().c_str(), node->getIdStr());
			forwardingModule->generateTargetsFor(node);
		}
	}
	
	HAGGLE_DBG("%s doing data object query for node %s [id=%s]\n", 
		   getName(), node->getName().c_str(), node->getIdStr());
	
	// Ask the data store for data objects bound for the node.
	// The node can be a valid target, even if it is not a current
	// neighbor -- we might find a delegate for it.
	
	node->setLastDataObjectQueryTime(Timeval::now());
	kernel->getDataStore()->doDataObjectQuery(node, 1, dataObjectQueryCallback);
}

#if defined(ENABLE_RECURSIVE_ROUTING_UPDATES)
/*
 
 The recurse list is a list of node that a "triggered" update has traversed.
 
 The general idea is that nodes, in the event of a change in a neighbor's status, should
 be able to tell all their other neighbors about this change.
 
 However, this leads to a risk of circular and never ending routing updates. The idea of the 
 recurse list is to mitigate such updates by appending a list to the update that indicates 
 the nodes that have already processed an update. Hence, when a node receives a recursive
 routing update, it appends all its  neighbors not already in the list and sends forth its
 updated metrics with this list attached. If a node finds that all its neighbors have
 processed this update, then the recursive update ends.
 */
size_t ForwardingManager::metadataToRecurseList(Metadata *m, NodeRefList& recurse_list)
{	
	if (!m || m->getName() != "RecurseList")
		return 0;
		
	Metadata *tm = m->getMetadata("Node");
	
	while (tm) {
		const char *id = tm->getParameter("node_id");
		
		if (id) {
			NodeRef n = Node::create_with_id(Node::TYPE_PEER, id);
			
			if (n) {
				recurse_list.push_back(n);
			}
		}
		
		tm = m->getNextMetadata();
	}
	
	return recurse_list.size();
}

Metadata *ForwardingManager::recurseListToMetadata(Metadata *m, const NodeRefList& recurse_list)
{
	if (!m)
		return NULL;
	
	Metadata *tm = m->addMetadata("RecurseList");
	
	if (!tm)
		return NULL;
	
	for (NodeRefList::const_iterator it = recurse_list.begin(); it != recurse_list.end(); it++) {
		Metadata *nm = tm->addMetadata("Node");
		
		if (nm) {
			nm->setParameter("node_id", (*it)->getIdStr());
		}
	}
	
	return tm;
}

/* 
 
 This function triggers the node to send its current metrics to all its neighbors
 modulo the ones already in a recurse list, which is given as a metadata object 
 from a just received routing update.
 
 If the recursive routing update is triggered by the loss of a neighbor, then
 the metadata is NULL.
 */
void ForwardingManager::recursiveRoutingUpdate(NodeRef peer, Metadata *m)
{
	// Send out our updated routing information to all neighbors
	NodeRefList neighbors;
	NodeRefList notify_list;
	NodeRefList recurse_list;
	size_t n = 0;
	
	// Fill in any existing nodes that have been notified by this recursive update
	if (m) {
		n = metadataToRecurseList(m->getMetadata("RecurseList"), recurse_list);
	}
	
	if (n == 0)
		recurse_list.add(peer);
	
	kernel->getNodeStore()->retrieveNeighbors(neighbors);
	
	// Figure out which peers have not already received this recursive update
	for (NodeRefList::iterator it = neighbors.begin(); it != neighbors.end(); it++) {
		bool should_notify = true;
		NodeRef neighbor = *it;
		
		// Do not notify nodes that are already in the list
		for (NodeRefList::iterator jt = recurse_list.begin(); jt != recurse_list.end(); jt++) {
			if (neighbor == *jt) {
				/*
				 HAGGLE_DBG("Neighbor %s [%s] has already received the update\n", 
					   neighbor->getName().c_str(), neighbor->getIdStr());
				 */
				should_notify = false;
				break;
			}
		}
		// Generate a routing update for this neighbor
		if (should_notify) {
			notify_list.push_back(neighbor);
			recurse_list.push_back(neighbor);
		}
	}
	// We have the complete list of neighbors that haven't received the update.
	// Now send our recursive update to them, append the current nodes that have
	// been part of this recursive routing update
	while (notify_list.size()) {
		NodeRef neighbor = notify_list.pop();
		forwardingModule->generateRoutingInformationDataObject(neighbor, &recurse_list);
	}	
}

#endif

void ForwardingManager::onRoutingInformation(Event *e)
{
	if (!e || !e->hasData())
		return;

	if (getState() > MANAGER_STATE_RUNNING) {
		HAGGLE_DBG("In shutdown, ignoring routing information\n");
		return;
	}
	
	DataObjectRefList& dObjs = e->getDataObjectList();

	while (dObjs.size()) {
		DataObjectRef dObj = dObjs.pop();
		
		InterfaceRef iface = dObj->getRemoteInterface();
		NodeRef peer = kernel->getNodeStore()->retrieve(iface);
		
		if (!peer || peer == kernel->getThisNode()) {
			HAGGLE_DBG("Routing information is from ourselves -- ignoring\n");
			return;
		}
		
		// Check if there is a module, and that the
		// information received data object makes sense to it.
		if (forwardingModule && forwardingModule->hasRoutingInformation(dObj)) {
			
			// Tell our module that it has new routing information
			forwardingModule->newRoutingInformation(dObj);

#if defined(ENABLE_RECURSIVE_ROUTING_UPDATES)
			if (recursiveRoutingUpdates && peer) {
				recursiveRoutingUpdate(peer, dObj->getMetadata()->getMetadata(getName()));
			}
#endif
		}
	}
}

void ForwardingManager::onNewDataObject(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	if (getState() > MANAGER_STATE_RUNNING) {
		HAGGLE_DBG("In shutdown, ignoring data object\n");
		return;
	}
	
	if (!doQueryOnNewDataObject)
		return;

	DataObjectRef& dObj = e->getDataObject();
	
	// No point in doing node queries if we have no neighbors to
	// forward the data object to
	if (dObj->isPersistent()) {
		if (kernel->getNodeStore()->numNeighbors() > 0) {
			HAGGLE_DBG("%s - new data object %s, doing node query\n", getName(), dObj->getIdStr());
			kernel->getDataStore()->doNodeQuery(dObj, MAX_NODES_TO_FIND_FOR_NEW_DATAOBJECTS, 1, nodeQueryCallback);
		} else {
			HAGGLE_DBG("%s - new data object %s, but deferring node query due to 0 neighbors\n", 
				   getName(), dObj->getIdStr());
		}
	} 
}

void ForwardingManager::onTargetNodes(Event * e)
{
	const NodeRef &delegate_node = e->getNode();
	const NodeRefList &target_nodes = e->getNodeList();
	
	// Ask the data store for data objects bound for the nodes for which the 
	// node is a good delegate forwarder:
	if (isNeighbor(delegate_node)) {
		// No point in asking for data objects if the delegate is not a current neighbor
		kernel->getDataStore()->doDataObjectForNodesQuery(delegate_node, target_nodes, 1, dataObjectQueryCallback);
	}
}

void ForwardingManager::onDelegateNodes(Event * e)
{
	NodeRefList *delegates = e->getNodeList().copy();

	if (!delegates)
		return;

	DataObjectRef dObj = e->getDataObject();
	
	// Go through the list of delegates:
	NodeRef delegate;
	NodeRefList ns;
	do {
		delegate = delegates->pop();
		if (delegate) {
			NodeRef actual_delegate;
			// Find the actual node reference to the node (if it is a 
			// neighbor):
			actual_delegate = kernel->getNodeStore()->retrieve(delegate, true);
                        
			if (actual_delegate) {
				// Forward the message via this neighbor:
				if (isNeighbor(actual_delegate) && shouldForward(dObj, actual_delegate)) {
					if (addToSendList(dObj, actual_delegate))
                                                ns.push_front(actual_delegate);
                                }
				/*
					There is no checking for if this delegate has the data
					object or the data object is being sent. This is because
					shouldForward() does that check.
				*/
			}
		}
	} while (delegate);

	if (!ns.empty()) {
		kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, dObj, ns));
	}
	
	delete delegates;
}

#if 0
void ForwardingManager::onSendRoutingInformation(Event * e)
{
	// Do we have a forwarding module?
	if (!forwardingModule) 
		return;
	
	// Do we have a metric DO?
	DataObjectRef dObj = forwardingModule->createForwardingDataObject();
	
	if (dObj) {
		// Yes. Send it to all neighbors that don't already have it:
		NodeRefList nodes;
		
		// Get all neighbors:
		kernel->getNodeStore()->retrieveNeighbors(nodes);
		
		NodeRefList ns;
		// For each neighbor:
		for (NodeRefList::iterator it = nodes.begin(); it != nodes.end(); it++) {
			// Send to that neighbor:
			if (isNeighbor(*it) && shouldForward(dObj, *it)) {
				if (addToSendList(dObj, *it))
					ns.push_front(*it);
			}
		}
		if (!ns.empty()) {
			/*
			 Here, we don't send a _WILL_SEND event, because these data 
			 objects are not meant to be modified, and should eventually 
			 be removed entirely, in favor of using the node description 
			 to transmit the forwarding metric.
			 */
			kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, dObj, ns));
		}
	}
}
#endif

/*
 handled configurations:
 - <ForwardingModule>noForward</ForwardingModule>	(no resolution, nothing!)
 - <ForwardingModule>Prophet</ForwardingModule>		(Prophet)
 - <ForwardingModule>...</ForwardingModule>		(else: no forwarding)
 */ 
void ForwardingManager::onConfig(Metadata *m)
{
	Metadata *fm = NULL;
	const char *param;
	
#if defined(ENABLE_RECURSIVE_ROUTING_UPDATES)
	param = m->getParameter("recursive_routing_updates");
	
	if (param) {
		if (strcmp(param, "true") == 0) {
			HAGGLE_DBG("Enabling recursive routing updates\n");
			recursiveRoutingUpdates = true;
		} else if (strcmp(param, "false") == 0) {
			HAGGLE_DBG("Disabling recursive routing updates\n");
			recursiveRoutingUpdates = false;
		}
	}
#endif
	param = m->getParameter("query_on_new_dataobject");

	if (param) {
		if (strcmp(param, "true") == 0) {
			HAGGLE_DBG("Enabling query on new data object\n");
			doQueryOnNewDataObject = true;
		} else if (strcmp(param, "false") == 0) {
			HAGGLE_DBG("Disabling query on new data object\n");
			doQueryOnNewDataObject = false;
		}
		
		LOG_ADD("# %s: query_on_new_dataobject=%s\n", 
			getName(), doQueryOnNewDataObject ? "true" : "false");
	}

	param = m->getParameter("periodic_dataobject_query_interval");

	if (param) {		
		char *endptr = NULL;
		unsigned long period = strtoul(param, &endptr, 10);
		
		if (endptr && endptr != param) {
			if (periodicDataObjectQueryInterval == 0 && 
			    period > 0 && 
			    kernel->getNodeStore()->numNeighbors() > 0 &&
			    !periodicDataObjectQueryEvent->isScheduled()) {
				kernel->addEvent(new Event(periodicDataObjectQueryCallback, 
							   NULL, 
							   periodicDataObjectQueryInterval));
			} 
			periodicDataObjectQueryInterval = period;
			HAGGLE_DBG("periodic_dataobject_query_interval=%lu\n", 
				   periodicDataObjectQueryInterval);
			LOG_ADD("# %s: periodic_dataobject_query_interval=%lu\n", 
				getName(), periodicDataObjectQueryInterval);
		}
	}

	fm = m->getMetadata("Forwarder");
	
	if (fm) {
		param = fm->getParameter("protocol");
		
		if (param) {
			string protocol = param;
			
			HAGGLE_DBG("Forwarding module \'%s\'\n", protocol.c_str());

			if (forwardingModule && protocol.compare(forwardingModule->getName()) == 0) {
				HAGGLE_DBG("Forwarder %s already set!\n", protocol.c_str());
			} else {
				// handle new configuration
				if (protocol.compare("none") == 0) {
					// clean up current forwardingModule
					setForwardingModule(NULL, true);
				} else if (protocol.compare(PROPHET_NAME) == 0) {
					setForwardingModule(new ForwarderProphet(this));
				} else {
					setForwardingModule(NULL);
				}
			}			
			if (forwardingModule) {
				forwardingModule->onConfig(*fm);
			}	
		}
	}
}

