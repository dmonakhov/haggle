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
#ifndef _FORWARDER_H
#define _FORWARDER_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/
class Forwarder;

#include "ManagerModule.h"
#include "ForwardingManager.h"

/**
	Forwarding module base class.
	
	The forwarding manager will not actively call ->start() on its forwarding 
	object, so it is up to the forwarding module itself to start its thread if 
	it wants to run as a thread.
*/
class Forwarder : public ManagerModule<ForwardingManager> {
public:
	/**
		This metric data object should be initialized and managed by the module.
		
		The forwarding manager sends this data object to any and all new nodes 
		when they are encountered.
		
		This data object should be persistent, to avoid having to use an 
		additional storage format. The forwarding manager assumes that this is 
		so, and:
			1) asks the data store for all forwarding data objects when haggle 
			   starts up, and uses addMetricDO() to provide them to the 
			   forwarding module,
			2) deletes old data objects from the same node whenever a forwarding
			   data object is recieved from that node.
		
		FIXME: The name of this reference is bad. It should be renamed.
	*/
	DataObjectRef myMetricDO;
	
	Forwarder(
		ForwardingManager *m = NULL, 
		const string name = "Unknown forwarding module") :
		ManagerModule<ForwardingManager>(m, name) {}
	~Forwarder() {}
	
#ifdef DEBUG
	/**
		Used by the forwarding manager to print the routing table. Will print 
		a bit of text to show where the routing table starts and stops, and call
		printRoutingTable().
	*/
	void doPrintRoutingTable(void)
	{
		printf("========= Routing table ========\n");
		printRoutingTable();
		printf("================================\n");
	}
#endif
	
	/*
		The following functions are called by the forwarding manager as part of
		event processing in the kernel. They are therefore called from the 
		kernel thread, and multiprocessing issues need to be taken into account.
		
		The reason for these functions to all be declared virtual ... {} is so 
		that specific forwarding modules can override only those functions they
		actually need to do their job. This means that functions can be declared
		here (and called by the forwarding manager) that only one forwarding 
		algorithm actually uses.
	*/
	
	/**
		Called when a data object has come in that has a "Routing" attribute.
		
		Also called for each such data object that is in the data store on 
		startup.
		
		Since the format of the data in such a data object is unknown to the 
		forwarding manager, it is up to the forwarder to make sure the data is
		in the correct format.
		
		Also, the given metric data object may have been sent before, due to 
		limitations in the forwarding manager.
	*/
	virtual void addMetricDO(DataObjectRef &metricDO) {}
	
	/**
		Called when a neighbor node is discovered.
	*/
	virtual void newNeighbor(NodeRef &neighbor) {}


	/**
		Called when a node just ended being a neighbor.
	*/
	virtual void endNeighbor(NodeRef &neighbor) {}
	
	/**
		Generates an event (EVENT_TYPE_TARGET_NODES) providing all the target 
		nodes that the given node is a good delegate forwarder for.
		
		If no nodes are found, no event should be created.
	*/
	virtual void generateTargetsFor(NodeRef &neighbor) {}
	
	/**
		Generates an event (EVENT_TYPE_DELEGATE_NODES) providing all the nodes 
		that are good delegate forwarders for the given node.
		
		If no nodes are found, no event should be created.
	*/
	virtual void generateDelegatesFor(DataObjectRef &dObj, NodeRef &target) {}
	
	/**
		Returns a string to store in the data base that encodes whatever 
		information the forwarding algorithm needs to recreate it's internal
		state after shutdown.
		
		Called by the forwarding manager as part of the shutdown procedure. The
		string is stored in the database (as a repository entry), and given to
		the forwarder using setEncodedState() at haggle startup.
	*/
	virtual string getEncodedState(void) { return ""; }
	
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
	virtual void setEncodedState(string &state) {}
	
#ifdef DEBUG
	/**
		Prints the current routing table, without any enclosing text to show
		where it starts or stops.
	*/
	virtual void printRoutingTable(void) {}
#endif
};

#endif
