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
#ifndef _FORWARDERASYNCHRONOUS_H
#define _FORWARDERASYNCHRONOUS_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/
class ForwarderAsynchronous;

#include "Forwarder.h"
#include "DataObject.h"
#include "Node.h"

#include <libcpphaggle/String.h>
#include <libcpphaggle/Mutex.h>
#include <libcpphaggle/GenericQueue.h>
#include <haggleutils.h>

/**
	This enum is used in the actions to tell the run loop what to do.
*/
typedef enum {
	// Add newMetricDO's metric data to the routing table
	FP_add_metric,
	// This neighbor was just seen in the neighborhood
	FP_new_neighbor,
	// This neighbor just left the neighborhood
	FP_end_neighbor,
	// Get the nodes that delegateNode is a good delegate forwarder for
	FP_generate_targets_for,
	// Get the nodes that are good delegate forwarders for this node
	FP_generate_delegates_for,
	// Get an encoded version of the internal state
	FP_get_encoded_state,
	// Set the internal state using an encoded version
	FP_set_encoded_state,
#ifdef DEBUG
	// Print the routing table:
	FP_print_table,
#endif
	// Terminate the run loop
	FP_quit
} FP_action_type;

/**
	These action elements are used to send data to the run loop, in order to
	make processing asynchronous.
*/
class FP_Action {
public:
	FP_action_type	action;
	DataObjectRef	theDO;
	NodeRef			theNode;
	string			*theString;
	// If this is set to a non-NULL value, then after the action is perfomed, 
	// this mutex is unlocked to simulate synchronous operation.
	Mutex			*toBeUnlocked;
	FP_Action(
		FP_action_type _action,
		DataObjectRef _theDO,
		NodeRef _theNode,
		string *_theString,
		Mutex *_toBeUnlocked) :
			action(_action), 
			theDO(_theDO), 
			theNode(_theNode), 
			theString(_theString), 
			toBeUnlocked(_toBeUnlocked)
		{}
	~FP_Action() {}
};

/**
	Asynchronous forwarding module. A forwarding module should inherit from this
	module if it is doing too much processing to be executing in the kernel 
	thread.
*/
class ForwarderAsynchronous : public Forwarder {
	/**
		This is maintained as the next time (roughly) that the algorithm should
		age its own internal metric.
	*/
	Timeval	next_aging_time;
	/**
		This is the difference in time between calls to _ageMetric.
	*/
	Timeval aging_time_delta;
	
	GenericQueue<FP_Action *> actionQueue;
	
	/**
		Main run loop for the prophet forwarder.
	*/
	bool run(void);
protected:
	
	/**
		True if the metric data object is out-of-date.
	*/
	bool should_recalculate_metric_do;
	
	/**
		Does the actual work of addMetricDO.
	*/
	virtual void _addMetricDO(DataObjectRef &dObj) {}
	
	/**
		Ages the local metrics. This function will be called once every 
		aging_time_delta seconds.
	*/
	virtual void _ageMetric(void) {}
	
	/**
		Does the actual work of newNeighbor.
	*/
	virtual void _newNeighbor(NodeRef &neighbor) {}
	
	/**
		Does the actual work of endNeighbor.
	*/
	virtual void _endNeighbor(NodeRef &neighbor) {}
	
	/**
		Does the actual work of getTargetsFor.
	*/
	virtual void _generateTargetsFor(NodeRef &neighbor) {}
	
	/**
		Does the actual work of getDelegatesFor.
	*/
	virtual void _generateDelegatesFor(DataObjectRef &dObj, NodeRef &target) {}
	
	/**
		Does the actual work of getEncodedState.
	*/
	virtual void _getEncodedState(string *state) {}
	
	/**
		Does the actual work of setEncodedState.
	*/
	virtual void _setEncodedState(string *state) {}
	
#ifdef DEBUG
	/**
		Does the actual work or printRoutingTable().
	*/
	virtual void _printRoutingTable(void) {}
#endif
	
	/**
		Creates a new metric data object.
	*/
	virtual void updateMetricDO(void) {}
	
public:
	ForwarderAsynchronous(
		Timeval _aging_time_delta,
		ForwardingManager *m = NULL, 
		const string name = "Asynchronous forwarding module");
	~ForwarderAsynchronous();
	
	/** See the parent class function with the same name. */
	void addMetricDO(DataObjectRef &metricDO);
	/** See the parent class function with the same name. */
	void newNeighbor(NodeRef &neighbor);
	/** See the parent class function with the same name. */
	void endNeighbor(NodeRef &neighbor);
	/** See the parent class function with the same name. */
	void generateTargetsFor(NodeRef &neighbor);
	/** See the parent class function with the same name. */
	void generateDelegatesFor(DataObjectRef &dObj, NodeRef &target);
	/** See the parent class function with the same name. */
	string getEncodedState(void);
	/** See the parent class function with the same name. */
	void setEncodedState(string &state);
#ifdef DEBUG
	/** See the parent class function with the same name. */
	void printRoutingTable(void);
#endif
};

#endif
