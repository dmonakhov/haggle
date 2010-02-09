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

#define FILTER_KEYWORD NODE_DESC_ATTR
//#define FILTER_NODEDESCRIPTION FILTER_KEYWORD "=true"
#define FILTER_NODEDESCRIPTION FILTER_KEYWORD "=" ATTR_WILDCARD

NodeManager::NodeManager(HaggleKernel * _haggle) : 
	Manager("NodeManager", _haggle), 
	thumbnail_size(0), thumbnail(NULL), 
	sequence_number(0)
{
}

NodeManager::~NodeManager()
{
	if (filterQueryCallback)
		delete filterQueryCallback;
	
	if (onRetrieveNodeCallback)
		delete onRetrieveNodeCallback;
	
	if (onRetrieveThisNodeCallback)
		delete onRetrieveThisNodeCallback;
	
	if (onRetrieveNodeDescriptionCallback)
		delete onRetrieveNodeDescriptionCallback;
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
	
	/*
	ret = setEventHandler(EVENT_TYPE_NEIGHBOR_INTERFACE_DOWN, onNeighborInterfaceDown);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}
	*/
	filterQueryCallback = newEventCallback(onFilterQueryResult);

	onRetrieveNodeCallback = newEventCallback(onRetrieveNode);
	onRetrieveThisNodeCallback = newEventCallback(onRetrieveThisNode);
	onRetrieveNodeDescriptionCallback = newEventCallback(onRetrieveNodeDescription);

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
	// add node information to trace
//	Event nodeInfoEvent(onRetrieveThisNodeCallback, kernel->getThisNode());
//	LOG_ADD("%s: %s NodeManager thisNode information\n", Timeval::now().getAsString().c_str(), nodeInfoEvent.getDescription().c_str());

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
		if (kernel->getNodeStore()->update(node))
			// Success! update the hagglekernel's reference, too.
			kernel->setThisNode(node);
	}
	// FIXME: set these to better values.
	// FIXME: respond to (?) and set this accordingly.
	kernel->getThisNode()->setMatchingThreshold(0);
	// FIXME: respond to the resource manager, and set this value accordingly.
	kernel->getThisNode()->setMaxDataObjectsInMatch(10);
	// Update create time to mark the freshness of the thisNode node description
	kernel->getThisNode()->setCreateTime();
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
		} else {
			HAGGLE_DBG("Sending node description [id=%s] to \'%s\', bloomfilter #objs=%lu\n", 
				   dObj->getIdStr(), neigh->getName().c_str(), kernel->getThisNode()->getBloomfilter()->numObjects());
			targetList.push_back(neigh);
			// Remember that we tried to send our node description to this node:
			nodeExchangeList.push_back(Pair<NodeRef, DataObjectRef>(neigh, dObj));
		}
	}
	
	if (targetList.size()) {
		HAGGLE_DBG("Pushing node description to %lu neighbors\n", targetList.size());
		kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, dObj, targetList));
	} else {
		HAGGLE_DBG("All neighbors already had our most recent node description\n");
	}
	
	return 1;
}

void NodeManager::onSendResult(Event * e)
{
	NodeRef &node = e->getNode();
	NodeRef neigh = kernel->getNodeStore()->retrieve(node, false);
	DataObjectRef &dObj = e->getDataObject();
	
	// Go through all our data regarding current node exchanges:
	for (NodeExchangeList::iterator it = nodeExchangeList.begin();
             it != nodeExchangeList.end(); it++) {
		// Is this the one?
		if ((*it).first == node && (*it).second == dObj) {
			// Yes.
			
			// Was the exchange successful?
			if (e->getType() == EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL) {
				// Yes. Set the flag.
				if (neigh)
					neigh->setExchangedNodeDescription(true);
				else
					node->setExchangedNodeDescription(true);
			} else if (e->getType() == EVENT_TYPE_DATAOBJECT_SEND_FAILURE) {
				// No. Unset the flag.
				if (neigh)
					neigh->setExchangedNodeDescription(false);
				else
					node->setExchangedNodeDescription(false);
				// FIXME: retry?
			}
			// Remove this entry from the list:
			nodeExchangeList.erase(it);
			
			// Done, no need to look further.
			return;
		}
	}
}

void NodeManager::onFilterQueryResult(Event * e)
{
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
		kernel->getDataStore()->retrieveNode(e->getInterface(), onRetrieveNodeCallback, true);
	} else {
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

		DataObjectRef dObj = dObjs.pop();

		NodeRef node = Node::create(NODE_TYPE_PEER, dObj);

		if (!node) {
			HAGGLE_DBG("Could not create node from metadata!\n");
			return;
		}

                HAGGLE_DBG("Node description from node %s [%s]\n", node->getName().c_str(), node->getIdStr());

		if (node == kernel->getThisNode()) {
			HAGGLE_ERR("Node description is my own. Ignoring and deleting from data store\n");
			// Remove the data object from the data store:
			kernel->getDataStore()->deleteDataObject(dObj);
			return;
		}
		
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
				// Ignore the node description if the node it describes
				// is a current neighbor. We trust such a neighbor to give
				// us the its latest node description when necessary.
				NodeRef peer = kernel->getNodeStore()->retrieve(dObj->getRemoteInterface(), true);
				
				if  (peer) {
					HAGGLE_DBG("Received %s's node description from third party node %s [%s]\n",
						   node->getName().c_str(), peer->getName().c_str(), peer->getIdStr());
				} else {
					HAGGLE_DBG("Received %s's node description from third party node with interface %s\n",
						   node->getName().c_str(), dObj->getRemoteInterface()->getIdentifierStr());
				}
				NodeRef neighbor = kernel->getNodeStore()->retrieve(node, true);
				
				if (neighbor) {
					HAGGLE_DBG("Node description of %s received from third party describes a neighbor -- ignoring!\n",
						   node->getName().c_str());
					return;
				}
			}
		} else {
			HAGGLE_DBG("Node description of %s [%s] has no remote interface\n",
				   node->getName().c_str(), node->getIdStr());
		}
		
		// The received node description may be older than one that we already have stored. Therefore, we
		// need to retrieve any stored node descriptions before we accept this one.
		char filterString[255];
		sprintf(filterString, "%s=%s", NODE_DESC_ATTR, node->getIdStr());

		kernel->getDataStore()->doFilterQuery(new Filter(filterString, 0), onRetrieveNodeDescriptionCallback);

	}
}
/* 
	callback to clean-up outdated nodedescriptions in the DataStore

	call in NodeManager::onReceiveNodeDescription through filterQuery
*/
void NodeManager::onRetrieveNodeDescription(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	DataStoreQueryResult *qr = static_cast < DataStoreQueryResult * >(e->getData());

	DataObjectRef dObj_tmp;
	DataObjectRef dObj = qr->detachFirstDataObject();
	Timeval receiveTime = dObj->getReceiveTime();
	/*
	HAGGLE_DBG("Node description id=%s\n", dObj->getIdStr());

	HAGGLE_DBG("Node description createTime %lf receiveTime %lf\n",
		   dObj->getCreateTime().getTimeAsSecondsDouble(),
		   receiveTime.getTimeAsSecondsDouble());
	*/

	while ((dObj_tmp = qr->detachFirstDataObject())) {

		HAGGLE_DBG("Node description createTime %lf receiveTime %lf\n",
			   dObj_tmp->getCreateTime().getTimeAsSecondsDouble(),
			   dObj_tmp->getReceiveTime().getTimeAsSecondsDouble());
		/*   
		HAGGLE_DBG("Time difference between node descriptions: receiveTime: %lf, createTime: %lf\n",
			   receiveTime.getTimeAsSecondsDouble() -
			   dObj_tmp->getReceiveTime().getTimeAsSecondsDouble(),
			   dObj->getCreateTime().getTimeAsSecondsDouble() -
			   dObj_tmp->getCreateTime().getTimeAsSecondsDouble());
		*/
		if (dObj_tmp->getReceiveTime() > dObj->getReceiveTime()) {
			receiveTime = dObj_tmp->getReceiveTime();
		}
		if (dObj_tmp->getCreateTime() > dObj->getCreateTime()) {
			// This node description was newer than the last "newest":
			
			HAGGLE_DBG("Found newer node description, deleting id=%s with createTime %lf\n",
				   dObj->getIdStr(), dObj->getCreateTime().getTimeAsSecondsDouble());
			// Delete the old "newest" data object:
			kernel->getDataStore()->deleteDataObject(dObj);
			// FIXME: we should really remove the data object from the 
			// "this" node's bloomfilter here.
			
			dObj = dObj_tmp;
		} else {
			// This node description was not newer than the last "newest":
			/*
			HAGGLE_DBG("Found older node description, deleting id=%s with createTime %lf\n",
				   dObj_tmp->getIdStr(), dObj_tmp->getCreateTime().getTimeAsSecondsDouble());
			*/
			// Delete it:
			kernel->getDataStore()->deleteDataObject(dObj_tmp);
			// FIXME: we should really remove the data object from the 
			// "this" node's bloomfilter here.
		}
	}

	/*
		If the greatest receive time is not equal to the one in the
		latest created node description, then we received an old node description 
		(i.e., we had a more recent one in the data store).
	*/
	if (receiveTime != dObj->getReceiveTime()) {
		HAGGLE_DBG("Received node description is not the latest, ignoring... latest: %s - dObj: %s\n", 
			receiveTime.getAsString().c_str(), dObj->getReceiveTime().getAsString().c_str());

		// The most recently received node description must have been received from a third party,
		// so we ignore it.
		delete qr;
		return;
	} 
	
	NodeRef node = Node::create(NODE_TYPE_PEER, dObj);
	
	if (!node) {
		HAGGLE_DBG("Could not create node from node description\n");
		delete qr;
		return;
	}

	// Add the node description to the node's bloomfilter
	node->getBloomfilter()->add(dObj);
	
	HAGGLE_DBG("New node description from node %s -- creating node: createTime %s receiveTime %s, bloomfilter #objs=%lu\n", 
		   node->getName().c_str(), 
		   dObj->getCreateTime().getAsString().c_str(), 
		   receiveTime.getAsString().c_str(),
		   node->getBloomfilter()->numObjects());
		
	// insert node into DataStore
	kernel->getDataStore()->insertNode(node);
	
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
		
		HAGGLE_DBG("Got a node description for node %s [id=%s], which is not a previously discovered neighbor.\n", 
			   node->getName().c_str(), node->getIdStr());
		
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

	delete qr;
}

