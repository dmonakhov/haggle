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
#include <string.h>

#include "NodeStore.h"
#include "Trace.h"

NodeStore::NodeStore()
{
}

NodeStore::~NodeStore() 
{
	int n = 0;

	while (!empty()) {
		NodeRecord *nr = front();
		nr->node->setStored(false);
		pop_front();
		delete nr;
		n++;
	}
	HAGGLE_DBG("Deleted %d node records in node store\n", n);
}

bool NodeStore::_stored(const NodeRef &node, bool mustBeNeighbor)
{
	for (NodeStore::iterator it = begin(); it != end(); it++) {
		NodeRecord *nr = *it;

		if (mustBeNeighbor && !nr->node->isNeighbor())
			continue;

		if (nr->node == node) {
			return true;
		}
	}
	return false;
}
bool NodeStore::_stored(const Node &node, bool mustBeNeighbor)
{

	for (NodeStore::iterator it = begin(); it != end(); it++) {
		NodeRecord *nr = *it;

		if (mustBeNeighbor && !nr->node->isNeighbor())
			continue;

		if (nr->node == node) {
			return true;
		}
	}
	return false;
}

bool NodeStore::_stored(const NodeId_t id, bool mustBeNeighbor)
{
	for (NodeStore::iterator it = begin(); it != end(); it++) {
		NodeRecord *nr = *it;

		if (mustBeNeighbor && !nr->node->isNeighbor())
			continue;

		if (memcmp(id, nr->node->getId(), NODE_ID_LEN) == 0) {
			return true;
		}
	}
	return false;
}

bool NodeStore::_stored(const string idStr, bool mustBeNeighbor)
{
	for (NodeStore::iterator it = begin(); it != end(); it++) {
		NodeRecord *nr = *it;

		if (mustBeNeighbor && !nr->node->isNeighbor())
			continue;

		if (strcmp(idStr.c_str(), nr->node->getIdStr()) == 0) {
			return true;
		}
	}
	return false;
}

bool NodeStore::stored(const NodeRef &node, bool mustBeNeighbor)
{
        Mutex::AutoLocker l(mutex);

	bool ret;
	
	if (!node)
		return false;

	node.lock();
	ret = _stored(node, mustBeNeighbor);
	node.unlock();

	return ret;
}

bool NodeStore::stored(const Node &node, bool mustBeNeighbor)
{
        Mutex::AutoLocker l(mutex);

	return _stored(node, mustBeNeighbor);
}

bool NodeStore::stored(const NodeId_t id, bool mustBeNeighbor)
{
        Mutex::AutoLocker l(mutex);

	return (id ? _stored(id, mustBeNeighbor) : false);
}

bool NodeStore::stored(const string idStr, bool mustBeNeighbor)
{
        Mutex::AutoLocker l(mutex);

	return _stored(idStr, mustBeNeighbor);
}

bool NodeStore::add(NodeRef &node)
{
        Mutex::AutoLocker l(mutex);

	if (!node)
		return false;;

	if (_stored(node)) {
		HAGGLE_DBG("Node %s is already in node store\n", node->getIdStr());
		return false;
	}

	HAGGLE_DBG("Adding new node to node store %s\n", node->getIdStr());
	node->setStored();
	push_back(new NodeRecord(node));

	return true;
}

NodeRef NodeStore::add(Node *node)
{
        Mutex::AutoLocker l(mutex);

	if (!node)
		return NULL;

	if (_stored(*node)) {
		HAGGLE_DBG("Node %s is already in node store\n", node->getIdStr());
		return false;
	}

	HAGGLE_DBG("Adding new node to node store %s\n", node->getIdStr());

	NodeRef nodeRef(node);
	nodeRef->setStored();
	push_back(new NodeRecord(nodeRef));

	return nodeRef;
}

NodeRef NodeStore::retrieve(const NodeRef &node, bool mustBeNeighbor)
{
        Mutex::AutoLocker l(mutex);

	if (!node)
		return NULL;

	for (NodeStore::iterator it = begin(); it != end(); it++) {
		NodeRecord *nr = *it;

		if (mustBeNeighbor && !nr->node->isNeighbor())
			continue;

		if (nr->node == node)
			return nr->node;
	}

	return NULL;
}

NodeRef NodeStore::retrieve(const Node& node, bool mustBeNeighbor)
{
        Mutex::AutoLocker l(mutex);

	for (NodeStore::iterator it = begin(); it != end(); it++) {
		NodeRecord *nr = *it;

		if (mustBeNeighbor && !nr->node->isNeighbor())
			continue;

		if (nr->node == node)
			return nr->node;
	}

	return NULL;
}

NodeRef NodeStore::retrieve(const NodeId_t id, bool mustBeNeighbor)
{
        Mutex::AutoLocker l(mutex);

	for (NodeStore::iterator it = begin(); it != end(); it++) {
		NodeRecord *nr = *it;
		NodeRef node = nr->node;

		if (mustBeNeighbor && !node->isNeighbor())
			continue;

		if (memcmp(id, node->getId(), NODE_ID_LEN) == 0) 
			return node;
	}

	return NULL;
}

NodeRef NodeStore::retrieve(const string &id, bool mustBeNeighbor)
{
        Mutex::AutoLocker l(mutex);

	for (NodeStore::iterator it = begin(); it != end(); it++) {
		NodeRecord *nr = *it;
		NodeRef node = nr->node;

		if (mustBeNeighbor && !node->isNeighbor())
			continue;

		if (memcmp(id.c_str(), node->getIdStr(), MAX_NODE_ID_STR_LEN) == 0) 
			return node;
	}

	return NULL;
}

NodeRef NodeStore::retrieve(const InterfaceRef &iface, bool mustBeNeighbor)
{
        Mutex::AutoLocker l(mutex);

        if (!iface)
	        return NULL;

	for (NodeStore::iterator it = begin(); it != end(); it++) {
		NodeRecord *nr = *it;
		NodeRef node = nr->node;

		if (mustBeNeighbor && !node->isNeighbor())
			continue;

		if (node->hasInterface(iface)) 
			return node;
	}

	return NULL;
}

NodeStore::size_type NodeStore::retrieve(const NodeType_t type, NodeRefList& nl)
{
        Mutex::AutoLocker l(mutex);
	size_type n = 0;
	
	for (NodeStore::iterator it = begin(); it != end(); it++) {
		const NodeRecord *nr = *it;

		if (nr->node->getType() == type) {
                        n++;
			nl.add(nr->node);
		}
	}

	return n;
}

NodeStore::size_type NodeStore::retrieve(const Criteria& crit, NodeRefList& nl)
{
        Mutex::AutoLocker l(mutex);
	size_type n = 0;

	for (NodeStore::iterator it = begin(); it != end(); it++) {
		const NodeRecord *nr = *it;

		if (crit(nr->node)) {
                        n++;
			nl.add(nr->node);
		}
	}

	return n;
}


NodeStore::size_type NodeStore::retrieveNeighbors(NodeRefList& nl)
{
        Mutex::AutoLocker l(mutex);
	size_type n = 0;

	for (NodeStore::iterator it = begin(); it != end(); it++) {
		const NodeRecord *nr = *it;

		if (nr->node->isNeighbor()) {
                        n++;
			nl.add(nr->node);
		}
	}

	return n;
}

NodeStore::size_type NodeStore::numNeighbors()
{
        Mutex::AutoLocker l(mutex);
	size_type n = 0;

	for (NodeStore::iterator it = begin(); it != end(); it++) {
		const NodeRecord *nr = *it;

		if (nr->node->isNeighbor()) {
                        n++;
		}
	}

	return n;
}

bool NodeStore::update(NodeRef &node, NodeRefList *nl)
{
        Mutex::AutoLocker l(mutex);
	bool found = false;
	
	if (!node)
		return false;

	// There may be undefined nodes in the node store that should
	// be removed/merged with a 'defined' node that we create from a 
	// node description. We loop through all nodes in the store and
	// compare their interface lists with the one in the 'defined' node.
	// If any interfaces match, we remove the matching nodes in the store 
	// and eventually replace them with the new one.
	for (NodeStore::iterator it = begin(); it != end();) {
		NodeRecord *nr = *it;
		bool found_now = false;
		
		nr->node.lock();

		const InterfaceRefList *ifaces = nr->node->getInterfaces();

		for (InterfaceRefList::const_iterator it2 = ifaces->begin(); it2 != ifaces->end(); it2++) {
			InterfaceRef iface = *it2;

			if (node->hasInterface(iface)) {
				// Transfer all the "up" interface states to the updated node
				if (iface->isUp())
					node->setInterfaceUp(iface);

				found_now = true;
			}
		}
		nr->node.unlock();

		if (found_now) {
			if (nl)
				nl->push_back(nr->node);
			
			nr->node->setStored(false);

			node->setExchangedNodeDescription(nr->node->hasExchangedNodeDescription());
				
			it = erase(it);
			delete nr;
			found = true;
		} else {
			it++;
		}
	}
	if (found) {
		node->setStored(true);
		push_back(new NodeRecord(node));
	}

	return found;
}


// Remove neighbor with a specified interface
NodeRef NodeStore::remove(const InterfaceRef &iface)
{
        Mutex::AutoLocker l(mutex);

	if (!iface)
		return NULL;

	for (NodeStore::iterator it = begin(); it != end(); it++) {
		NodeRecord *nr = *it;

		if (nr->node->hasInterface(iface)) {
			NodeRef node = nr->node;
			erase(it);
			node->setStored(false);
			delete nr;
			return node;
		}
	}

	return NULL;
}

// Remove all nodes of a specific type
int NodeStore::remove(const NodeType_t type)
{
        Mutex::AutoLocker l(mutex);
	int n = 0;

	NodeStore::iterator it = begin();

	while (it != end()) {
		NodeRecord *nr = *it;

		if (nr->node->getType() == type) {
			it = erase(it);
			n++;
			nr->node->setStored(false);
			delete nr;
			continue;
		}
		it++;
	}

	return n;
}

// Remove neighbor with a specified interface
bool NodeStore::remove(const NodeRef &node)
{
        Mutex::AutoLocker l(mutex);

	if (!node)
		return false;

	for (NodeStore::iterator it = begin(); it != end(); it++) {
		NodeRecord *nr = *it;

		if (nr->node == node) {
			erase(it);
			nr->node->setStored(false);
			delete nr;
			return true;
		}
	}

	return false;
}

#ifdef DEBUG
void NodeStore::print()
{
        Mutex::AutoLocker l(mutex);
	int n = 0;

	printf("======== Node store list ========\n\n");

	if (empty()) {
		printf("No nodes in store\n");
		return;
	}
	for (NodeStore::iterator it = begin(); it != end(); it++) {
		const NodeRecord *nr = *it;

                printf("Node: %d type=\'%s\' name=\'%s\' - %s stored=%s\n", 
                       n++, nr->node->getTypeStr(),
                       nr->node->getName().c_str(),
                       (nr->node->isAvailable() && (nr->node->getType() == NODE_TYPE_PEER || nr->node->getType() == NODE_TYPE_UNDEF)) ? "Neighbor" : "Unconfirmed neighbor",
                       nr->node->isStored() ? "Yes" : "No");
                printf("id=%s\n", nr->node->getIdStr());
                printf("");
		nr->node->printInterfaces();

		const Attributes *attrs = nr->node->getAttributes();

		if (attrs->size()) {
			printf("Interests:\n");
			for (Attributes::const_iterator itt = attrs->begin(); itt != attrs->end(); itt++) {
				printf("\t\t%s\n", (*itt).second.getString().c_str());
			}
		}
	}

	printf("===============================\n\n");
}
#endif
