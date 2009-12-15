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
#ifndef _NODESTORE_H
#define _NODESTORE_H

#include <libcpphaggle/List.h>
#include <libcpphaggle/Mutex.h>

#include "Node.h"

/**
	The node record only holds a node now, but it may be expanded in the future.
*/
class NodeRecord {
public:
	NodeRef node;
	NodeRecord(const NodeRef &_node) : node(_node) {}
	~NodeRecord() {}
	bool operator==(const NodeRef &n) { return node == n; }
};

/** */
class NodeStore : protected List<NodeRecord *>
{
	Mutex mutex;
	/*
	  Internal, unlocked versions of functions below.
	 */
        bool _stored(const NodeRef &node, bool mustBeNeighbor = false);
        bool _stored(const Node &node, bool mustBeNeighbor = false);
        bool _stored(const char *id, bool mustBeNeighbor = false);
	bool _stored(const string idStr, bool mustBeNeighbor = false);
public:
	/**
		The Critiera class is a matching functor that can be
		used as a generic way to retrieve nodes from the node
		store. The basic criteria matches all nodes. When retrieving
		nodes, the caller should inherit this class and implement
		their own matching criteria.
	*/
	class Criteria {
	public:
		virtual bool operator() (const NodeRef& n) const
		{
			return true;
		}
		virtual ~Criteria() {}
	};
	NodeStore();
	~NodeStore();

	// Locking for the store
	void lock() { mutex.lock(); }
	void unlock() { mutex.unlock(); }
	bool trylock() { return mutex.trylock(); }
	/**
		Check if a node is currently stored in the store. The caller
		may optionally specify whether the given node must be marked
		a neighbor in the store.
		Returns: boolean indicating whether the node was found or not.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
        bool stored(const NodeRef &node, bool mustBeNeighbor = false);
	/**
		Check if a node is currently stored in the store. The caller
		may optionally specify whether the given node must be marked
		a neighbor in the store.
		Returns: boolean indicating whether the node was found or not.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
        bool stored(const Node &node, bool mustBeNeighbor = false);
	/**
		Check if a node is currently stored in the store. The caller
		may optionally specify whether the given node must be marked
		a neighbor in the store.
		Returns: boolean indicating whether the node was found or not.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
        bool stored(const char *id, bool mustBeNeighbor = false);
	/**
		Check if a node is currently stored in the store. The caller
		may optionally specify whether the given node must be marked
		a neighbor in the store.
		Returns: boolean indicating whether the node was found or not.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
	bool stored(const string idStr, bool mustBeNeighbor = false);
	/**
		Add a new node to the node store.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
        bool add(NodeRef &node);
	/**
		Add a new node to the node store.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
        NodeRef add(Node *node);

	/**
		Retrieve a node from the node store. The caller may optionally 
		specify to only retrieve current neighbors.
		Returns: A node reference to the specified node or a null-reference
		if it was not found in the store.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
        NodeRef retrieve(const NodeRef &node, bool mustBeNeighbor = false);
	/**
		Retrieve a node from the node store. The caller may optionally 
		specify to only retrieve current neighbors.
		Returns: A node reference to the specified node or a null-reference
		if it was not found in the store.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
        NodeRef retrieve(const Node &node, bool mustBeNeighbor = false);
	/**
		Retrieve a node from the node store. The caller may optionally 
		specify to only retrieve current neighbors.
		Returns: A node reference to the specified node or a null-reference
		if it was not found in the store.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
        NodeRef retrieve(const char *id, bool mustBeNeighbor = false);
	/**
		Retrieve a node from the node store. The caller may optionally 
		specify to only retrieve current neighbors.
		Returns: A node reference to the specified node or a null-reference
		if it was not found in the store.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
		NodeRef retrieve(const string &id, bool mustBeNeighbor = false);
	/**
		Retrieve a node from the node store. The caller may optionally 
		specify to only retrieve current neighbors.
		Returns: A node reference to the specified node or a null-reference
		if it was not found in the store.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
        NodeRef retrieve(const InterfaceRef &iface, bool mustBeNeighbor = false);

	/**
		Retrieve a list of all nodes matching a specific criteria.
		Returns NULL if there are no nodes that match the given criteria.
		The returned list has to be freed by the caller.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
	size_type retrieve(const Criteria& crit, NodeRefList& nl);
	/**
		Retrieve a list of all nodes of a specific type.
		Returns NULL if there are no nodes that match the given type.
		The returned list has to be freed by the caller.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
	size_type retrieve(const NodeType_t type, NodeRefList& nl);
	/**
		Retrieve a list of all current and confirmed neighbors.
		Returns NULL if there are no neighbors. The list has to be freed 
		by the caller.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
	size_type retrieveNeighbors(NodeRefList& nl);

	/**
		Returns the number of current neighbors in the store.
	*/
	size_type numNeighbors();
	/**
		Remove a node from the node store.

		Returns true if the node was removed, and false if the node
		was not located in the store.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
	bool remove(const NodeRef &node);
	/**
		Remove all nodes in the store of a specific type.

		Returns: The number of nodes removed, or -1 on error.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
        int remove(const NodeType_t type);

	/**
		Remove a node that have a specific interface.

		Returns: A reference to the removed node.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
        NodeRef remove(const InterfaceRef &iface);
	/**
		Update a node in the node store given a "fresher" version
		of the node. The interfaces of the node given as in-argument is matched
		against the nodes in the store. If at least one interface given node matches 
		a node in the store, the stored node will be updated.
		
		DEADLOCK WARNING: the calling thread may not hold the lock on a node 
		reference or an interface reference while calling this function.
	*/
        bool update(NodeRef &inNode, NodeRefList *nl = NULL);

	
#ifdef DEBUG
        void print();
#endif
};

#endif /* _NODESTORE_H */
