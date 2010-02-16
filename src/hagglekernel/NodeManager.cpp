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
#include <stdlib.h>
#include <time.h>

#include <libcpphaggle/Platform.h>
#include <libcpphaggle/String.h>
#include <haggleutils.h>

#include "EventQueue.h"
#include "NodeManager.h"
#include "DataObject.h"
#include "Node.h"
#include "Event.h"
#include "Attribute.h"
#include "Interface.h"
#include "Filter.h"

using namespace haggle;

#define MERGE_BLOOMFILTERS

#define FILTER_KEYWORD NODE_DESC_ATTR
#define FILTER_NODEDESCRIPTION FILTER_KEYWORD "=" ATTR_WILDCARD

#define DEFAULT_NODE_DESCRIPTION_RETRY_WAIT (10.0) // Seconds
#define DEFAULT_NODE_DESCRIPTION_RETRIES (3)

NodeManager::NodeManager(HaggleKernel * _haggle) : 
	Manager("NodeManager", _haggle), 
	thumbnail_size(0), thumbnail(NULL),
	nodeDescriptionRetries(DEFAULT_NODE_DESCRIPTION_RETRIES),
	nodeDescriptionRetryWait(DEFAULT_NODE_DESCRIPTION_RETRY_WAIT)
{
}

NodeManager::~NodeManager()
{
	if (onRetrieveNodeCallback)
		delete onRetrieveNodeCallback;
	
	if (onRetrieveThisNodeCallback)
		delete onRetrieveThisNodeCallback;

	if (onInsertedNodeCallback)
		delete onInsertedNodeCallback;
}

bool NodeManager::init_derived()
{
#define __CLASS__ NodeManager
	int ret;

	// Register filter for node descriptions
	registerEventTypeForFilter(nodeDescriptionEType, "NodeDescription", onReceiveNodeDescription, FILTER_NODEDESCRIPTION);

	ret = setEventHandler(EVENT_TYPE_LOCAL_INTERFACE_UP, onLocalInterfaceUp);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}

	ret = setEventHandler(EVENT_TYPE_LOCAL_INTERFACE_DOWN, onLocalInterfaceDown);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}

	ret = setEventHandler(EVENT_TYPE_NEIGHBOR_INTERFACE_UP, onNeighborInterfaceUp);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}

	ret = setEventHandler(EVENT_TYPE_NEIGHBOR_INTERFACE_DOWN, onNeighborInterfaceDown);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}

	ret = setEventHandler(EVENT_TYPE_NODE_CONTACT_NEW, onNewNodeContact);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}

	ret = setEventHandler(EVENT_TYPE_NODE_DESCRIPTION_SEND, onSendNodeDescription);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}
	
	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL, onSendResult);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}

	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_SEND_FAILURE, onSendResult);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}

	onRetrieveNodeCallback = newEventCallback(onRetrieveNode);
	onRetrieveThisNodeCallback = newEventCallback(onRetrieveThisNode);
	onInsertedNodeCallback = newEventCallback(onInsertedNode);

	kernel->getDataStore()->retrieveNode(kernel->getThisNode(), onRetrieveThisNodeCallback);
	
	/*
		We only search for a thumbnail at haggle startup time, to avoid 
		searching the disk every time we create a new node description.
	*/
	/*
		Search for and (if found) load the avatar image for this node.
	*/
#if defined(OS_ANDROID)
        // This is a ugly hack for Android in order to not store the
        // Avatar.jpg in /usr/bin
	string str = HAGGLE_DEFAULT_STORAGE_PATH;
#else
	string str = HAGGLE_FOLDER_PATH;
#endif
	str += PLATFORM_PATH_DELIMITER;
	str += "Avatar.jpg";
	FILE *fp = fopen(str.c_str(), "r");
        
	if (fp != NULL) {
		fseek(fp, 0, SEEK_END);
		thumbnail_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		thumbnail = (char *) malloc(thumbnail_size);
	
                if (thumbnail == NULL) {
			thumbnail_size = 0;
		} else {
			if (fread(thumbnail, 1, thumbnail_size, fp) != (size_t) thumbnail_size) {
                                free(thumbnail);
                                thumbnail = NULL;
                        }
		}
		fclose(fp);
	}

	if (thumbnail == NULL){
		HAGGLE_DBG("No avatar image found.\n");
	} else {
		HAGGLE_DBG("Found avatar image. Will attach to all node descriptions\n");
	}
	
	return true;
}

void NodeManager::onPrepareShutdown()
{
	// Remove the node description filter from the data store:
	unregisterEventTypeForFilter(nodeDescriptionEType);
	// Save the "this node" node in the data store so it can be retrieved when 
	// we next start up.
	kernel->getDataStore()->insertNode(kernel->getThisNode());
	
	// We're done:
	signalIsReadyForShutdown();
}

#if defined(ENABLE_METADAPARSER)
bool NodeManager::onParseMetadata(Metadata *md)
{         
        HAGGLE_DBG("NodeManager onParseMetadata()\n");

        // FIXME: should check 'Node' section of metadata
        return true;
}
#endif

void NodeManager::onRetrieveThisNode(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	NodeRef node = e->getNode();
	
	// If we found a "this node" in the data store:
	if (node) {
		// try to update the node store with that node:
		if (kernel->getNodeStore()->update(node)) {
			// Success! update the hagglekernel's reference, too.
			kernel->setThisNode(node);
		}
	}
	// Update create time to mark the freshness of the thisNode node description
	kernel->getThisNode()->setNodeDescriptionCreateTime();
}

bool NodeManager::isInSendList(const NodeRef& node, const DataObjectRef& dObj)
{
	for (SendList_t::iterator it = sendList.begin(); it != sendList.end(); it++) {
		if ((*it).first == node && (*it).second.dObj == dObj) {
			return true;
		}
	}
	return false;
}

int NodeManager::sendNodeDescription(NodeRefList& neighList)
{
	NodeRefList targetList;

	HAGGLE_DBG("Pushing node description to %lu neighbors\n", neighList.size());

	DataObjectRef dObj = kernel->getThisNode()->getDataObject();

	if (thumbnail != NULL)
		dObj->setThumbnail(thumbnail, thumbnail_size);
	
	for (NodeRefList::iterator it = neighList.begin(); it != neighList.end(); it++) {
		NodeRef& neigh = *it;
	
		if (neigh->getBloomfilter()->has(dObj)) {
			HAGGLE_DBG("Neighbor %s already has our most recent node description\n", neigh->getName().c_str());
		} else if (!isInSendList(neigh, dObj)) {
			HAGGLE_DBG("Sending node description [%s] to \'%s\', bloomfilter #objs=%lu\n", 
				   dObj->getIdStr(), neigh->getName().c_str(), kernel->getThisNode()->getBloomfilter()->numObjects());
			targetList.push_back(neigh);

			SendEntry_t se = { dObj, 0 };
			// Remember that we tried to send our node description to this node:
			sendList.push_back(Pair<NodeRef, SendEntry_t>(neigh, se));
		} else {
			HAGGLE_DBG("Node description [%s] is already in send list for neighbor %s\n",
				dObj->getIdStr(), neigh->getName().c_str());
		}
	}
	
	if (targetList.size()) {
		HAGGLE_DBG("Pushing node description [%s] to %lu neighbors\n", dObj->getIdStr(), targetList.size());
		kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, dObj, targetList));
	} else {
		HAGGLE_DBG("All neighbors already had our most recent node description\n");
	}
	
	return 1;
}

void NodeManager::onSendResult(Event *e)
{
	NodeRef neigh = kernel->getNodeStore()->retrieve(e->getNode(), false);
	DataObjectRef &dObj = e->getDataObject();
	
	if (!neigh) {
		neigh = e->getNode();

		if (!neigh) {
			HAGGLE_ERR("No node in send result\n");
			return;
		}
	}
	// Go through all our data regarding current node exchanges:
	for (SendList_t::iterator it = sendList.begin(); it != sendList.end(); it++) {
		// Is this the one?
		if ((*it).first == neigh && (*it).second.dObj == dObj) {
			// Yes.
			
			// Was the exchange successful?
			if (e->getType() == EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL) {
				// Yes. Set the flag.
				neigh->setExchangedNodeDescription(true);
				sendList.erase(it);
				HAGGLE_DBG("Successfully sent node description [%s] to neighbor %s [%s], after %lu retries\n",
					dObj->getIdStr(), neigh->getName().c_str(), neigh->getIdStr(), (*it).second.retries);
				
				//dObj->print();
				
			} else if (e->getType() == EVENT_TYPE_DATAOBJECT_SEND_FAILURE) {
				// No. Unset the flag.
				neigh->setExchangedNodeDescription(false);

				(*it).second.retries++;

				// Retry
				if ((*it).second.retries <= nodeDescriptionRetries && neigh->isNeighbor()) {
					HAGGLE_DBG("Sending node description [%s] to neighbor %s [%s], retry=%u\n", 
						dObj->getIdStr(), neigh->getName().c_str(), neigh->getIdStr(), (*it).second.retries);
					// Retry, but delay for some seconds.
					kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, dObj, neigh, 0, nodeDescriptionRetryWait));
				} else {
					// Remove this entry from the list:
					HAGGLE_DBG("FAILED to send node description to neighbor %s [%s] after %u retries...\n",
						neigh->getName().c_str(), neigh->getIdStr(), (*it).second.retries);
					sendList.erase(it);
				}
			}
			// Done, no need to look further.
			return;
		}
	}
}

void NodeManager::onLocalInterfaceUp(Event * e)
{
	kernel->getThisNode()->addInterface(e->getInterface());
}

void NodeManager::onLocalInterfaceDown(Event *e)
{
	kernel->getThisNode()->setInterfaceDown(e->getInterface());
}

void NodeManager::onNeighborInterfaceUp(Event *e)
{
	NodeRef neigh = kernel->getNodeStore()->retrieve(e->getInterface(), true);

	if (!neigh) {
		// No one had that interface?

		// merge if node exists in datastore (asynchronous call), we force it to return
		// as we only generate a node up event when we know we have the best information
		// for the node.
		HAGGLE_DBG("No active neighbor with interface [%s], retrieving node from data store\n", 
			e->getInterface()->getIdentifierStr());

		kernel->getDataStore()->retrieveNode(e->getInterface(), onRetrieveNodeCallback, true);
	} else {
		HAGGLE_DBG("Neighbor %s has new active interface [%s], setting to UP\n", 
			neigh->getName().c_str(), e->getInterface()->getIdentifierStr());

		neigh->setInterfaceUp(e->getInterface());
	}
}

/* 
	callback on retrieve node from Datastore
 
	called in NodeManager::onNeighborInterfaceUp 
	to retrieve a previously known node that matches an interface from an interface up event.
*/
void NodeManager::onRetrieveNode(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	NodeRef& node = e->getNode();

	if (!node) {
		InterfaceRef& iface = e->getInterface();
		
		if (!iface) {
			HAGGLE_ERR("Neither node nor interface in callback\n");
			return;
		}
		
		node = Node::create();
		
		if (!node) 
			return;
		
		node->addInterface(iface);
		
	} else {
		HAGGLE_DBG("Neighbor node %s has %lu objects in bloomfilter\n", 
			   node->getName().c_str(), node->getBloomfilter()->numObjects());
	}
	
	// See if this node is already an active neighbor but in an uninitialized state
	if (kernel->getNodeStore()->update(node)) {
		HAGGLE_DBG("Node %s [%s] was updated in node store\n", node->getName().c_str(), node->getIdStr());
	} else {
		HAGGLE_DBG("Node %s [%s] not previously neighbor... Adding to node store\n", node->getName().c_str(), node->getIdStr());
		kernel->getNodeStore()->add(node);
	}
	
	kernel->addEvent(new Event(EVENT_TYPE_NODE_CONTACT_NEW, node));
}

void NodeManager::onNeighborInterfaceDown(Event *e)
{
	// Let the NodeStore know:
	NodeRef node = kernel->getNodeStore()->retrieve(e->getInterface(), false);
	
	if (node) {
		node->setInterfaceDown(e->getInterface());
		
		if (!node->isAvailable()) {
			kernel->getNodeStore()->remove(node);

			/*
				We need to update the node information in the data store 
				since the bloomfilter might have been updated during the 
				neighbor's co-location.
			*/
			kernel->getDataStore()->insertNode(node);

			// Report the node as down
			kernel->addEvent(new Event(EVENT_TYPE_NODE_CONTACT_END, node));
		}
	}
}

void NodeManager::onNewNodeContact(Event *e)
{
	NodeRefList neighList;

	if (!e)
		return;

	NodeRef neigh = e->getNode();

	switch (neigh->getType()) {
	case NODE_TYPE_UNDEF:
		HAGGLE_DBG("%s - New node contact. Have not yet received node description!\n", getName());
		break;
	case NODE_TYPE_PEER:
		HAGGLE_DBG("%s - New node contact %s [id=%s]\n", getName(), neigh->getName().c_str(), neigh->getIdStr());
		break;
	case NODE_TYPE_GATEWAY:
		HAGGLE_DBG("%s - New gateway contact %s\n", getName(), neigh->getIdStr());
		break;
	default:
		break;
	}

	neighList.push_back(neigh);

	sendNodeDescription(neighList);
}

// Push our node description to all neighbors
void NodeManager::onSendNodeDescription(Event *e)
{
	NodeRefList neighList;

	unsigned long num = kernel->getNodeStore()->retrieveNeighbors(neighList);

	if (num <= 0) {
		HAGGLE_DBG("No neighbors - not sending node description\n");
		return;
	}

	sendNodeDescription(neighList);
}


void NodeManager::onReceiveNodeDescription(Event *e)
{
	if (!e || !e->hasData())
		return;

	DataObjectRefList& dObjs = e->getDataObjectList();

	while (dObjs.size()) {
		bool fromThirdParty = false;

		DataObjectRef dObj = dObjs.pop();

		NodeRef node = Node::create(NODE_TYPE_PEER, dObj);

		if (!node) {
			HAGGLE_DBG("Could not create node from metadata!\n");
			continue;
		}

                HAGGLE_DBG("Node description [%s] of node %s [%s] received\n", 
			dObj->getIdStr(), node->getName().c_str(), node->getIdStr());

		if (node == kernel->getThisNode()) {
			HAGGLE_ERR("Node description is my own. Ignoring and deleting from data store\n");
			// Remove the data object from the data store:
			kernel->getDataStore()->deleteDataObject(dObj);
			continue;
		}
		
		// Retrieve any existing neighbor nodes that might match the newly received 
		// node description.
		NodeRef neighbor = kernel->getNodeStore()->retrieve(node, true);

		// Make sure at least the interface of the remote node is set to up
		// this 
		if (dObj->getRemoteInterface()) {
			
			if (node->hasInterface(dObj->getRemoteInterface())) {
				// Node description was received directly from
				// the node it belongs to
				
				// Mark the interface as up in the node.
				node->setInterfaceUp(dObj->getRemoteInterface());				
			} else {
				// Node description was received from third party.
				
				fromThirdParty = true;

				NodeRef peer = kernel->getNodeStore()->retrieve(dObj->getRemoteInterface(), true);
				
				if  (peer) {
					HAGGLE_DBG("Received %s's node description from third party node %s [%s]\n",
						   node->getName().c_str(), peer->getName().c_str(), peer->getIdStr());
				} else {
					HAGGLE_DBG("Received %s's node description from third party node with interface %s\n",
						   node->getName().c_str(), dObj->getRemoteInterface()->getIdentifierStr());
				}

				// Ignore the node description if the node it describes
				// is a current neighbor. We trust such a neighbor to give
				// us its latest node description when necessary.
				if (neighbor) {
					HAGGLE_DBG("Node description of %s received from third party describes a neighbor -- ignoring!\n",
						   node->getName().c_str());
					continue;
				}
			}
		} else {
			HAGGLE_DBG("Node description of %s [%s] has no remote interface\n",
				   node->getName().c_str(), node->getIdStr());
		}
	
		// If the existing neighbor node was undefined, we merge the bloomfilters
		// of the undefined node and the node created from the node description
		if (neighbor) {
			if (neighbor->getType() == NODE_TYPE_UNDEF) {
				HAGGLE_DBG("Merging bloomfilter of node %s with its previously undefined node\n", 
					node->getName().c_str());
				node->getBloomfilter()->merge(*neighbor->getBloomfilter());
			} else {
				if (node->getNodeDescriptionCreateTime() <= neighbor->getNodeDescriptionCreateTime()) {
					HAGGLE_DBG("Node description create time is NOT greater than on existing neighbor node. IGNORING node description\n");
					// Delete old node descriptions from data store
					if (node->getNodeDescriptionCreateTime() < neighbor->getNodeDescriptionCreateTime())
						kernel->getDataStore()->deleteDataObject(dObj);

					continue;
				}
			}
		} 

		HAGGLE_DBG("New node description from node %s -- createTime %s receiveTime %s, bloomfilter #objs=%lu\n", 
			node->getName().c_str(), 
			dObj->getCreateTime().getAsString().c_str(), 
			dObj->getReceiveTime().getAsString().c_str(),
			node->getBloomfilter()->numObjects());

		// Here we have a fast path and a slow path depending on whether the node description
		// was received directly from the node it describes or not. In the case the node description 
		// was received directly from the node it describes, then we trust it to contain the latest 
		// information, and an updated bloomfilter. In this case we take the fast path. In the case we received
		// the node description from a third party node, we cannot trust the bloomfilter to be up-to-date
		// with all the data objects we have previously sent the node. Therefore, we should merge
		// the received bloomfilter with the one we have in the data store. This requires first a 
		// call to the data store to retrieve the 
		if (fromThirdParty) {
#if defined(MERGE_BLOOMFILTERS)
			// Slow path, wait for callback from data store before proceeding
			kernel->getDataStore()->insertNode(node, onInsertedNodeCallback, true);
#else
			// Do fast path also here if we do not want to merge bloomfilters
			kernel->getDataStore()->insertNode(node);
			nodeUpdate(node);
#endif
		} else {
			// Fast path, without callback
			kernel->getDataStore()->insertNode(node);
			nodeUpdate(node);
		}
	}
}

void NodeManager::nodeUpdate(NodeRef& node)
{

	NodeRefList nl;
	
	// See if this node is already an active neighbor but in an uninitialized state
	if (kernel->getNodeStore()->update(node, &nl)) {
		HAGGLE_DBG("Neighbor node %s [id=%s] was updated in node store\n", 
			   node->getName().c_str(), node->getIdStr());
	} else {
		// This is the path for node descriptions received via a third party, i.e.,
		// the node description does not belong to the neighbor node we received it
		// from.
		
		// NOTE: in reality, we should never get here for node descriptions belonging to neighbor nodes. 
		// This is because a node (probably marked as 'undefined') should have been 
		// added to the node store when we got an interface up event just before the node description
		// was received (the interface should have been discovered or 'snooped').
		// Thus, the getNodeStore()->update() above should have succeeded. However,
		// in case the interface discovery somehow failed, there might not be a
		// neighbor node in the node store that matches the received node description.
		// Therefore, we might get here for neighbor nodes as well, but in that case
		// there is not much more to do until we discover the node properly
		
		HAGGLE_DBG("Got a node description [%s] for node %s [id=%s], which is not a previously discovered neighbor.\n", 
			node->getDataObject()->getIdStr(), node->getName().c_str(), node->getIdStr());
		
		// Sync the node's interfaces with those in the interface store. This
		// makes sure all active interfaces are marked as 'up'.
		node.lock();

		const InterfaceRefList *ifl = node->getInterfaces();
		
		for (InterfaceRefList::const_iterator it = ifl->begin(); it != ifl->end(); it++) {
			if (kernel->getInterfaceStore()->stored(*it)) {
				node->setInterfaceUp(*it);
			}
		}
		node.unlock();
		
		if (node->isAvailable()) {
			// Add node to node store
			HAGGLE_DBG("Node %s [id=%s] was a neighbor -- adding to node store\n", 
				   node->getName().c_str(), node->getIdStr());
			
			kernel->getNodeStore()->add(node);
		
			// We generate both a new contact event and an updated event because we
			// both 'discovered' the node and updated its node description at once.
			// In most cases, this should never really happen (see not above).
			kernel->addEvent(new Event(EVENT_TYPE_NODE_CONTACT_NEW, node));
		} else {
			HAGGLE_DBG("Node %s [id=%s] had no active interfaces, not adding to store\n", 
				   node->getName().c_str(), node->getIdStr());
		}
	}
	
	// We send the update event for all nodes that we have received a new node description from, even
	// if they are not neighbors. This is because we want to match data objects against the node although
	// we might not have direct connectivity to it.
	kernel->addEvent(new Event(EVENT_TYPE_NODE_UPDATED, node, nl));
}

void NodeManager::onInsertedNode(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	NodeRef& node = e->getNode();

	if (!node) {
		HAGGLE_ERR("No node in callback\n");
		return;
	}

	HAGGLE_DBG("Node %s [%s] was successfully inserted into data store\n", 
		node->getName().c_str(), node->getIdStr());

	nodeUpdate(node);
}

void NodeManager::onConfig(Metadata *m)
{
	Metadata *nm = m->getMetadata("MatchingThreshold");

	if (nm) {
		char *endptr = NULL;
		unsigned long matchingThreshold = strtoul(nm->getContent().c_str(), &endptr, 10);

		if (endptr && endptr != nm->getContent().c_str()) {
			HAGGLE_DBG("Setting matching threshold to %lu\n", matchingThreshold);
			kernel->getThisNode()->setMatchingThreshold(matchingThreshold);
			LOG_ADD("# NodeManager: matching threshold=%lu\n", matchingThreshold);
		}
	}

	nm = m->getMetadata("MaxDataObjectsInMatch");

	if (nm) {
		char *endptr = NULL;
		unsigned long maxDataObjectsInMatch = strtoul(nm->getContent().c_str(), &endptr, 10);

		if (endptr && endptr != nm->getContent().c_str()) {
			HAGGLE_DBG("Setting max data objects in match to %lu\n", maxDataObjectsInMatch);
			kernel->getThisNode()->setMaxDataObjectsInMatch(maxDataObjectsInMatch);
			LOG_ADD("# NodeManager: max data objects in match=%lu\n", maxDataObjectsInMatch);
		}
	}

	nm = m->getMetadata("NodeDescriptionRetry");

	if (nm) {
		char *endptr = NULL;
		const char *param = nm->getParameter("retries");

		if (param) {
			unsigned long retries = strtoul(param, &endptr, 10);

			if (endptr && endptr != param) {
				HAGGLE_DBG("Setting node description retries to %lu\n", retries);
				nodeDescriptionRetries = retries;
				LOG_ADD("# NodeManager: node description retries=%lu\n", nodeDescriptionRetries);
			}
		}

		param = nm->getParameter("retry_wait");

		if (param) {
			char *endptr = NULL;
			double retry_wait = strtod(param, &endptr);

			if (endptr && endptr != param) {
				HAGGLE_DBG("Setting node description retry wait to %lf\n", retry_wait);
				nodeDescriptionRetryWait = retry_wait;
				LOG_ADD("# NodeManager: node description retry wait=%lf\n", nodeDescriptionRetryWait);
			}
		}
	}
}
