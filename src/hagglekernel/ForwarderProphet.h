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
#ifndef _FORWARDERPROPHET_H
#define _FORWARDERPROPHET_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/
class ForwarderProphet;

#include "ForwarderAsynchronous.h"

#include <libcpphaggle/Map.h>

/**
	Prophet forwarding ids are only used internally in the forwarderprophet 
	manager module, but for technical reasons it needs to be defined here.
*/
typedef unsigned long prophet_node_id_t;

// Symbolic constant for the local node.
#define this_node_id ((prophet_node_id_t) 1)

typedef Pair<double, Timeval> prophet_metric_t;
typedef Map<prophet_node_id_t, prophet_metric_t> prophet_rib_t;

/**
	Proof-of-concept PRoPHET routing module.
*/
class ForwarderProphet : public ForwarderAsynchronous {
	
// Prophet constants (as per draft v4):
#define PROPHET_P_ENCOUNTER (0.75)
#define PROPHET_ALPHA (0.5)
#define PROPHET_BETA (0.25)
#define PROPHET_GAMMA (0.999)
#define PROPHET_AGING_TIME_UNIT (10) // 10 minutes
#define PROPHET_AGING_CONSTANT (0.1)
        
        HaggleKernel *kernel;
        
	/**
		In order to reduce the amount of memory taken up by the forwarding 
		table, this mapping between node ids and forwarding manager id numbers
		is used. This means that this forwarding module cannot handle more than
		2^32 other nodes.
	*/
	Map<string, prophet_node_id_t> nodeid_to_id_number;
	/**
		In order to reduce the amount of memory taken up by the forwarding 
		table, this mapping between forwarding manager id numbers and node ids 
		is used. This means that this forwarding module cannot handle more than
		2^32 other nodes.
	*/
	Map<prophet_node_id_t, string> id_number_to_nodeid;
	/**
		The next id number to use. Since the default value for a 
		prophet_node_id_t in a map is 0, this should never be 0, in order 
		to avoid confusion.
	*/
	prophet_node_id_t next_id_number;
	
	/**
		This is the local node's internal PRoPHET metrics.
	*/
	prophet_rib_t rib;
	Timeval rib_timestamp;
	/**
		This is a mapping of id numbers (of other nodes) to those nodes' public
		metrics.
	*/
	Map<prophet_node_id_t, prophet_rib_t> neighbor_ribs;
	
	size_t getSaveState(RepositoryEntryList& rel);
	bool setSaveState(RepositoryEntryRef& e);
	
	/**
	 This function returns the nonzero id number for the given node id 
	 string. It ensures that the node id is in the nodeid_to_id_number map
	 and that the returned id number is nonzero. If the node id wasn't in the
	 map to begin with, it is inserted, along with a new id number.
	 */
	prophet_node_id_t id_for_string(const string& nodeid);
	
	prophet_metric_t& age_metric(prophet_metric_t& metric, bool force = false);
	
	bool newRoutingInformation(const Metadata *m);
	
	bool addRoutingInformation(DataObjectRef& dObj, Metadata *parent);
	/**
		Does the actual work of newNeighbor.
	*/
	void _newNeighbor(NodeRef &neighbor);

	/**
		Does the actual work of endNeighbor.
	*/
	void _endNeighbor(NodeRef &neighbor);
	
	/**
		Does the actual work of getTargetsFor.
	*/
	void _generateTargetsFor(NodeRef &neighbor);
	
	/**
		Does the actual work of getDelegatesFor.
	*/
	void _generateDelegatesFor(DataObjectRef &dObj, NodeRef &target);
#ifdef DEBUG
	/**
		Does the actual work or printRoutingTable().
	*/
	void _printRoutingTable(void);
#endif
		
public:
	ForwarderProphet(ForwardingManager *m = NULL, const EventType type = -1);
	~ForwarderProphet();
};

#endif
