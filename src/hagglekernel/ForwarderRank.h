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
	ForwarderRank(
		ForwardingManager *m = NULL, 
		const string name = "Rank forwarding module");
	~ForwarderRank();

	virtual void addMetricDO(DataObjectRef &metricDO);
	
	/**
		Called when a neighbor node is discovered.
	*/
	virtual void newNeighbor(NodeRef &neighbor);


	/**
		Called when a node just ended being a neighbor.
	*/
	virtual void endNeighbor(NodeRef &neighbor);
	
	/**
		Generates an event (EVENT_TYPE_TARGET_NODES) providing all the target 
		nodes that the given node is a good delegate forwarder for.
		
		If no nodes are found, no event should be created.
	*/
	virtual void generateTargetsFor(NodeRef &neighbor);
	
	/**
		Generates an event (EVENT_TYPE_DELEGATE_NODES) providing all the nodes 
		that are good delegate forwarders for the given node.
		
		If no nodes are found, no event should be created.
	*/
	virtual void generateDelegatesFor(DataObjectRef &dObj, NodeRef &target);
	
	/**
		Returns a string to store in the data base that encodes whatever 
		information the forwarding algorithm needs to recreate it's internal
		state after shutdown.
		
		Called by the forwarding manager as part of the shutdown procedure. The
		string is stored in the database (as a repository entry), and given to
		the forwarder using setEncodedState() at haggle startup.
	*/
	virtual string getEncodedState(void);
	
	/**
		Called by the forwarding manager as part of the startup procedure. The
		given string is either one returned by getEncodedState() for this kind
		of module, one returned by getEncodedState() for another kind of module,
		or the empty string.
		
		This function shall be called by the forwarding manager as soon as it
		retrieves the string from the data store. This does not mean it will be
		called before any other function, however. This must be taken into 
		consideration when writing the module.
	*/
	virtual void setEncodedState(string &state);
};

#endif
