/* Copyright 2009 Uppsala University
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
#ifndef _FORWARDERRANK_H
#define _FORWARDERRANK_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/
class ForwarderRank;

#include "Forwarder.h"
#include <libcpphaggle/Map.h>

/**
*/
class ForwarderRank : public Forwarder {
	long myRank;
	Map< String, long > ranks;
public:
	ForwarderRank(ForwardingManager *m = NULL);
	~ForwarderRank();

	bool addRoutingInformation(DataObjectRef& dObj, Metadata *parent);
	/**
		Called when a neighbor node is discovered.
	*/
	void newNeighbor(const NodeRef &neighbor);


	/**
		Called when a node just ended being a neighbor.
	*/
	void endNeighbor(const NodeRef &neighbor);
	
	/**
		Generates an event (EVENT_TYPE_TARGET_NODES) providing all the target 
		nodes that the given node is a good delegate forwarder for.
		
		If no nodes are found, no event should be created.
	*/
	void generateTargetsFor(const NodeRef &neighbor);
	
	/**
		Generates an event (EVENT_TYPE_DELEGATE_NODES) providing all the nodes 
		that are good delegate forwarders for the given node.
		
		If no nodes are found, no event should be created.
	*/
	void generateDelegatesFor(const DataObjectRef &dObj, const NodeRef &target, const NodeRefList *other_targets);
};

#endif
