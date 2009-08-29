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
#include "ForwarderEmpty.h"
#include "ForwarderProphet.h"

ForwardingManager::ForwardingManager(HaggleKernel * _kernel) :
	Manager("ForwardingManager", _kernel), 
	dataObjectQueryCallback(NULL), 
	nodeQueryCallback(NULL), 
	forwardDobjCallback(NULL), 
	forwardRepositoryCallback(NULL), 
	forwardQueryCallback(NULL),
	forwardingModule(NULL)
{
#define __CLASS__ ForwardingManager
	setEventHandler(EVENT_TYPE_NODE_UPDATED, onNodeUpdated);
	setEventHandler(EVENT_TYPE_DATAOBJECT_NEW, onNewDataObject);
	setEventHandler(EVENT_TYPE_DATAOBJECT_FORWARD, onDataObjectForward);
	setEventHandler(EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL, onSendDataObjectResult);
	setEventHandler(EVENT_TYPE_DATAOBJECT_SEND_FAILURE, onSendDataObjectResult);
	setEventHandler(EVENT_TYPE_NODE_CONTACT_NEW, onNewNeighbor);
	setEventHandler(EVENT_TYPE_NODE_CONTACT_END, onEndNeighbor);
	setEventHandler(EVENT_TYPE_TARGET_NODES, onTargetNodes);
	setEventHandler(EVENT_TYPE_DELEGATE_NODES, onDelegateNodes);
#ifdef DEBUG
	setEventHandler(EVENT_TYPE_DEBUG_CMD, onDebugCmd);
#endif
	
	dataObjectQueryCallback = newEventCallback(onDataObjectQueryResult);
	delayedDataObjectQueryCallback = newEventCallback(onDelayedDataObjectQuery);
	nodeQueryCallback = newEventCallback(onNodeQueryResult);
	forwardDobjCallback = newEventCallback(onForwardDobjsCallback);
	forwardRepositoryCallback = newEventCallback(onForwardRepositoryCallback);

	forwardingModule = new ForwarderProphet(this);
	kernel->getDataStore()->readRepository(new RepositoryEntry("ForwardingManager", "Forwarder state"), forwardRepositoryCallback);
	
	kernel->getDataStore()->doFilterQuery(new Filter("Forward=*"), forwardDobjCallback);
	
	forwardQueryCallback = newEventCallback(onForwardQueryResult);

#if HAVE_EXCEPTIONS
	if (!forwardQueryCallback)
		throw ForwardingException(-1, "Could not create forwardQueryCallback");
#endif

	sendMetricCallback = newEventCallback(onSendMetric);
}

ForwardingManager::~ForwardingManager()
{
	// Destroy the forwarding module:
	delete forwardingModule;
	
	if (dataObjectQueryCallback)
		delete dataObjectQueryCallback;
	
	// Empty the list of forwarded data objects.
	while (!forwardedObjects.empty()) {
		HAGGLE_ERR("Clearing unsent data object.\n");
		forwardedObjects.pop_front();
	}
	if (nodeQueryCallback)
		delete nodeQueryCallback;
	
	if (forwardDobjCallback)
		delete forwardDobjCallback;
	
	if (forwardRepositoryCallback)
		delete forwardRepositoryCallback;
	
	if (forwardQueryCallback)
		delete forwardQueryCallback;
	
	if (sendMetricCallback)
		delete sendMetricCallback;
	
	if (delayedDataObjectQueryCallback)
		delete delayedDataObjectQueryCallback;
}

void ForwardingManager::onShutdown()
{
	// Store the forwarding module's state in the data store:
	RepositoryEntryRef insertState(new RepositoryEntry("ForwardingManager", "Forwarder state",
				forwardingModule->getEncodedState().c_str()), "Forwarder state insertion entry");
	HAGGLE_DBG("Storing forwarder state: %s\n", insertState->getValue());
	kernel->getDataStore()->insertRepository(insertState);

	unregisterWithKernel();
}

#ifdef DEBUG
void ForwardingManager::onDebugCmd(Event *e)
{
	if (e) {
		if (e->getDebugCmd()->getType() == DBG_CMD_PRINT_ROUTING_TABLE)
			forwardingModule->doPrintRoutingTable();
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
bool ForwardingManager::isNeighbor(NodeRef& node)
{
	if (node && node->getType() == NODE_TYPE_PEER && kernel->getNodeStore()->retrieve(node, true))
                return true;

        return false;
}
/*
   This function verifies that a node is a valid receiver for a
   specific data object, e.g., that the node has not already received
   the data object.
 */

bool ForwardingManager::addToSendList(DataObjectRef& dObj, NodeRef& node, int repeatCount)
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
        forwardedObjects.push_front(Pair< Pair<DataObjectRef, NodeRef>,int>(Pair<DataObjectRef, NodeRef>(dObj,node), repeatCount));
        
        return true;
}

bool ForwardingManager::shouldForward(DataObjectRef dObj, NodeRef node)
{
        NodeRef nodeInStore;
        
	if (dObj->isNodeDescription()) {
		NodeRef descNode = NodeRef(new Node(NODE_TYPE_PEER, dObj));
		
		if (descNode) {
			if (descNode == node) {
				// Do not send the peer its own node description
				HAGGLE_DBG("Data object is peer's node description. - not sending!\n");
				return false;
			}
		}
	}
	
	HAGGLE_DBG("%s Checking if data object %s should be forwarded to node '%s'\n", 
                   getName(), dObj->getIdStr(), node->getName().c_str());
	
        // Check if the node is in the node store
        nodeInStore = kernel->getNodeStore()->retrieve(node, false);

        if (node->getType() == NODE_TYPE_PEER && !nodeInStore)
                nodeInStore = node;

	if (nodeInStore) {
		if (nodeInStore->getBloomfilter()->has(dObj)) {
			HAGGLE_DBG("%s node %s already has data object %s\n", 
				getName(), nodeInStore->getIdStr(), dObj->getIdStr());
		} else {
                        return true;
		}
	} else {
		HAGGLE_DBG("No active targets for data object\n");
	}
	return false;
}

void ForwardingManager::forwardByDelegate(DataObjectRef &dObj, NodeRef &target)
{
        forwardingModule->generateDelegatesFor(dObj, target);
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
	NodeRef node = e->getNode();

	if (!node) {
		HAGGLE_ERR("Someone posted an EVENT_TYPE_DATAOBJECT_FORWARD event without a node.\n");
		return;
	}
	
	// Start forwarding the object:
	if (shouldForward(dObj, node)) {
		// Ok, the target wants the data object. Now check if
		// the target is a neighbor, in which case the data
		// object is sent directly to the target, otherwise
		// ask the forwarding module to generate delegates.
		if (isNeighbor(node)) {
			if (addToSendList(dObj, node)) {
				HAGGLE_DBG("Sending data object %s directly to target neighbor %s\n", 
					dObj->getIdStr(), node->getName().c_str());
				kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, dObj, node));
			}
		} else {
			HAGGLE_DBG("Trying to find delegates for data object %s bound for target %s\n", 
				dObj->getIdStr(), node->getName().c_str());
			forwardByDelegate(dObj, node);
		}
	}
}

void ForwardingManager::onSendDataObjectResult(Event *e)
{
	DataObjectRef dObj = e->getDataObject();
	NodeRef	node = e->getNode();

        HAGGLE_DBG("Checking data object results\n");

	// Find the data object in our send list:
	for (forwardingList::iterator it = forwardedObjects.begin();
		it != forwardedObjects.end(); it++) {
		
		if ((*it).first.first == dObj && (*it).first.second == node) {
			if (e->getType() == EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL) {
				// Remove the data object - it has been forwarded.
				forwardedObjects.erase(it);
				
				// Add data object to neighbor's bloomfilter.
				node->getBloomfilter()->add(dObj);
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
		// Is this node a currently available neighbor node?
		if (shouldForward(dObj, target)) {
			if (isNeighbor(target)) {
				// Yes: it is it's own best delegate, so start "forwarding" the object:
				
				if (addToSendList(dObj, target)) {
					kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, dObj, target));
					HAGGLE_DBG("Sending data object %s directly to target neighbor %s\n", dObj->getIdStr(), target->getName().c_str());
				}
			} else { 
				HAGGLE_DBG("Trying to find delegates for data object %s bound for target %s\n", dObj->getIdStr(), target->getName().c_str());
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
	
	if (!dObj) {
		HAGGLE_DBG("No dataobject in query result\n");
		delete qr;
		return;
	}
	
	HAGGLE_DBG("Got node query result for dataobject %s\n", dObj->getIdStr());
	
	NodeRef node;
	NodeRefList ns;

	while (node = qr->detachFirstNode()) {
                 // Is this node a currently available neighbor node?
                if (shouldForward(dObj, node)) {
                        if (isNeighbor(node)) {
                                // Yes: it is it's own best delegate, so start "forwarding" the object:
                                if (addToSendList(dObj, node)) {
                                        HAGGLE_DBG("Sending data object %s directly to target neighbor %s\n", dObj->getIdStr(), node->getName().c_str());
                                        ns.push_front(node);
                                }
                        } else { 
                                HAGGLE_DBG("Trying to find delegates for data object %s bound for target %s\n", dObj->getIdStr(), node->getName().c_str());
                                forwardByDelegate(dObj, node);
                        }
                }
	}

	if (!ns.empty()) {
		kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, dObj, ns));
	}
	
	delete qr;
}

void ForwardingManager::onForwardDobjsCallback(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	// This event either reports the forwarding module's state, or all the
	// forwarding objects.
	
	DataStoreQueryResult *qr = static_cast < DataStoreQueryResult * >(e->getData());
	
	// Are there any objects?
	if (qr->countDataObjects() != 0) {
		// Yes. Tell the forwarding module about them:
		DataObjectRef dObj;
		do {
			dObj = qr->detachFirstDataObject();
			if (dObj) {
				forwardingModule->addMetricDO(dObj);
			}
		} while (dObj);
	}
	
	delete qr;
}

void ForwardingManager::onForwardRepositoryCallback(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	// This event either reports the forwarding module's state, or all the
	// forwarding objects.
	
	DataStoreQueryResult *qr = static_cast < DataStoreQueryResult * >(e->getData());
	
	// Are there any repository entries?
	if (qr->countRepositoryEntries() != 0) {
		RepositoryEntryRef re;
		
		// Then this is most likely the forwarding module's state:
		
		re = qr->detachFirstRepositoryEntry();
		// Was there a repository entry? => was this really what we expected?
		if (re) {
			// Yes: give it to the forwarding module:
			string str = re->getValue();
			forwardingModule->setEncodedState(str);
			
			// Remove any and all state data from the repository, to avoid 
			// having old data in the data store to mess things up:
			RepositoryEntryRef copy = new RepositoryEntry(re->getAuthority(), re->getKey());
			kernel->getDataStore()->deleteRepository(copy);
		} else {
			// No: tell the forwarding module there was no data:
			string str = "";
			forwardingModule->setEncodedState(str);
		}
	} else {
		// No: tell the forwarding module there was no data:
		string str = "";
		forwardingModule->setEncodedState(str);
	}
	
	delete qr;
}

void ForwardingManager::onForwardQueryResult(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	DataStoreQueryResult *qr = static_cast < DataStoreQueryResult * >(e->getData());
	
	DataObjectRef current, newest;
	/*
          Figure out which is actually the newest data object, and delete all 
          others.
	*/
	do {
		current = qr->detachFirstDataObject();
		if (current) {
			if (newest) {
				HAGGLE_DBG("Deleting old forwarding objects from data store\n");
				if (current->getCreateTime() > newest->getCreateTime()) {
					kernel->getDataStore()->deleteDataObject(newest);
					newest = current;
				} else {
					kernel->getDataStore()->deleteDataObject(current);
				}
			} else {
				newest = current;
			}
		}
	} while (current);
	
	delete qr;
	
	/*
		Now tell the forwarding module the forwarding data has changed for that 
		node.
	*/
	forwardingModule->addMetricDO(newest);
}

void ForwardingManager::onDelayedDataObjectQuery(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	NodeRef node = e->getNode();

	// If the node is still in the node query list, we know that there has been no update for it
	// since we generated this delayed call.
	for (List<NodeRef>::iterator it = pendingQueryList.begin(); it != pendingQueryList.end(); it++) {
		if (node == *it) {
			pendingQueryList.erase(it);
			findMatchingDataObjectsAndTargets(node);
			break;
		}
	}
}

void ForwardingManager::onNewNeighbor(Event *e)
{
	NodeRef node = e->getNode();
	
	if (node->getType() == NODE_TYPE_UNDEF)
		return;
	
	// Tell the forwarding module that we've got a new neighbor:
	forwardingModule->newNeighbor(node);
	
	// Find matching data objects for the node and figure out whether it is a good
	// delegate. But delay these operations in case we get a node update event for the
	// same node due to receiving a new node description for it. If we get the
	// the update, we should only do the query once using the updated information.
	
	pendingQueryList.push_back(node);
	kernel->addEvent(new Event(delayedDataObjectQueryCallback, node, 3));
	
	HAGGLE_DBG("%s - new node contact with %s [id=%s]. Delaying data object query in case there is an incoming node description for the node\n", 
		   getName(), node->getName().c_str(), node->getIdStr());
}

void ForwardingManager::onEndNeighbor(Event *e)
{	
	if (e->getNode()->getType() == NODE_TYPE_UNDEF)
		return;
	
	// Tell the forwarding module that we've got a new neighbor:
	forwardingModule->endNeighbor(e->getNode());
}

void ForwardingManager::onNodeUpdated(Event *e)
{
	if (!e || !e->hasData())
		return;

	NodeRef node = e->getNode();
	NodeRefList &replaced = e->getNodeList();
	
	if (node->getType() == NODE_TYPE_UNDEF) {
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
		if ((*it)->getType() == NODE_TYPE_UNDEF && node->isNeighbor()) {
			// Yep. Tell the forwarding module that we've got a new neighbor:
			forwardingModule->newNeighbor(node);
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
	if (!node)
		return;
	
	HAGGLE_DBG("%s doing data object query for node %s [id=%s]\n", 
		   getName(), node->getName().c_str(), node->getIdStr());
	
	// Check that this is an active neighbor node we can send to:
	if (node->isNeighbor()) {
		
		// Send the neighbor our forwarding metric if we have one
		if (forwardingModule->myMetricDO) {
			HAGGLE_DBG("Sending forwarding data object to %\n", node->getName().c_str());
			if (shouldForward(forwardingModule->myMetricDO, node)) {
				if (addToSendList(forwardingModule->myMetricDO, node)) {
					kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, forwardingModule->myMetricDO, node));
				}
			} else {
				HAGGLE_ERR("Could not forward MetricDo to neighbor\n");
			}
		}
		// Ask the forwarding module for additional target nodes for which this neighbor can act as delegate.
		forwardingModule->generateTargetsFor(node);
	} else {
                HAGGLE_ERR("Neighbor is not available, cannot send forwarding information\n");
        }
	
	// Ask the data store for data objects bound for the node:
	kernel->getDataStore()->doDataObjectQuery(node, 1, dataObjectQueryCallback);
}


void ForwardingManager::onNewDataObject(Event * e)
{
	if (!e || !e->hasData())
		return;
	
	DataObjectRef dObj = e->getDataObject();

	// Is this is a forwarding metric data object?
	const Attribute *forwardingAttr = dObj->getAttribute("Forward");

	if (forwardingAttr) {
		if (forwardingAttr->getValue() != "*") {
			// Figure out which forwarding objects are from this node:
			kernel->getDataStore()->doFilterQuery(new Filter(*forwardingAttr), forwardQueryCallback);
		}
	}
	
	HAGGLE_DBG("%s - new data object, doing node query\n", getName());
	
	kernel->getDataStore()->doNodeQuery(dObj, 10, 1, 0, nodeQueryCallback);
}


void ForwardingManager::onTargetNodes(Event * e)
{
	const NodeRef	&node = e->getNode();
	const NodeRefList &nodes = e->getNodeList();
	
	// Ask the data store for data objects bound for the nodes for which the 
	// node is a good delegate forwarder:
	kernel->getDataStore()->doDataObjectForNodesQuery(node, nodes, 1, dataObjectQueryCallback);
}

void ForwardingManager::onDelegateNodes(Event * e)
{
	NodeRefList *delegates = e->getNodeList().copy();
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

void ForwardingManager::onSendMetric(Event * e)
{
	// Do we have a metric DO?
	if (forwardingModule->myMetricDO) {
		// Yes. Send it to all neighbors that don't already have it:
		NodeRefList nodes;
		
		// Get all neighbors:
		kernel->getNodeStore()->retrieveNeighbors(nodes);
		
		NodeRefList ns;
		// For each neighbor:
		for (NodeRefList::iterator it = nodes.begin(); it != nodes.end(); it++) {
			// Send to that neighbor:
			if (isNeighbor(*it) && shouldForward(forwardingModule->myMetricDO, *it)) {
                                if (addToSendList(forwardingModule->myMetricDO, *it))
                                        ns.push_front(*it);
                        }
                }
                if (!ns.empty()) {
			/*
				Here, we don't send a _WILL_SEND event, because these data 
				objects are not meant to be modified, and should eventually be 
				removed entirely, in favor of using the node description to 
				transmit the forwarding metric.
			*/
			kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, 
                                                   forwardingModule->myMetricDO, ns));
		}
	}
}

void ForwardingManager::sendMetric(void)
{
	// Post an event to the event queue, so that the main thread does the 
	// sending. Prevents threading issues.
	kernel->addEvent(new Event(sendMetricCallback, NULL, 0.0));
}
