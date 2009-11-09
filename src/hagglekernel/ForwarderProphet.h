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
typedef unsigned long prophet_forwarding_id;

// Symbolic constant for the local node.
#define this_node	((prophet_forwarding_id) 1)

typedef Map<prophet_forwarding_id, double> prophet_metric_table;

/**
	Proof-of-concept PRoPHET routing module.
*/
class ForwarderProphet : public ForwarderAsynchronous {
// Age every 10 minutes:
#define PROPHET_TIME_BETWEEN_AGING	(10*60)

// Aging constant:
#define PROPHET_AGING_CONSTANT	(0.1)

// Initialization constant:
#define PROPHET_INITIALIZATION_CONSTANT	(0.5)
        
        HaggleKernel *kernel;
        
	/**
		In order to reduce the amount of memory taken up by the forwarding 
		table, this mapping between node ids and forwarding manager id numbers
		is used. This means that this forwarding module cannot handle more than
		2^32 other nodes.
	*/
	Map<string, prophet_forwarding_id>	nodeid_to_id_number;
	/**
		In order to reduce the amount of memory taken up by the forwarding 
		table, this mapping between forwarding manager id numbers and node ids 
		is used. This means that this forwarding module cannot handle more than
		2^32 other nodes.
	*/
	Map<prophet_forwarding_id, string>	id_number_to_nodeid;
	/**
		The next id number to use. Since the default value for a 
		prophet_forwarding_id in a map is 0, this should never be 0, in order 
		to avoid confusion.
	*/
	prophet_forwarding_id				next_id_number;
	
	/**
		This is the local node's internal PRoPHET metrics.
	*/
	prophet_metric_table my_metrics;
	/**
		This is a mapping of id numbers (of other nodes) to those nodes' public
		metrics.
	*/
	Map<prophet_forwarding_id, prophet_metric_table> forwarding_table;
	
	/**
		This function returns the nonzero id number for the given node id 
		string. It ensures that the node id is in the nodeid_to_id_number map
		and that the returned id number is nonzero. If the node id wasn't in the
		map to begin with, it is inserted, along with a new id number.
	*/
	prophet_forwarding_id id_for_string(string nodeid);
	
	/**
		This function returns a metric string that can be parsed by 
		::parseMetricString().
	*/
	string getMetricString(prophet_metric_table &table);
	
	/**
		This function parses a metric string that was created by 
		::getMetricString().
	*/
	void parseMetricString(prophet_metric_table &table, string &metric);
	
	/**
		Does the actual work of addMetricDO.
	*/
	void _addMetricDO(DataObjectRef &dObj);
	
	/**
		Ages the local metrics:
	*/
	void _ageMetric(void);
	
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
	
	/**
		Does the actual work of getEncodedState.
	*/
	void _getEncodedState(string *state);
	
	/**
		Does the actual work of setEncodedState.
	*/
	void _setEncodedState(string *state);
#ifdef DEBUG
	/**
		Does the actual work or printRoutingTable().
	*/
	void _printRoutingTable(void);
#endif
	
	/**
		Creates a new metric data object.
	*/
	void updateMetricDO(void);
	
public:
	ForwarderProphet(
		ForwardingManager *m = NULL, 
		const string name = "PRoPHET forwarding module",
		const string _forwardAttributeName = "ForwarderProphetMetricForNodeID");
	~ForwarderProphet();
};

#endif
