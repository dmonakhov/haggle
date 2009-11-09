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
	/**
		This is the name of the attribute that says this is a forwarding metric
		data object. The point of not having it as a #define is to enable the 
		use of multiple forwarding modules. Each forwarding module should have 
		its own attribute name, which makes it receive only forwarding objects 
		that it can read. This also makes it possible to have different 
		forwarding modules running at different times, because the forwarding
		module will not delete old forwarding data objects in favor of new ones
		from a different forwarding module.
	*/
	string forwardAttributeName;
	
	Forwarder(
		ForwardingManager *m = NULL, 
		const string name = "Unknown forwarding module",
		const string _forwardAttributeName = "ForwarderMetricForNodeID") :
		ManagerModule<ForwardingManager>(m, name),
		forwardAttributeName(_forwardAttributeName)
	{}
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
	For the moment, the format of a metric data object is as follows:
	
	The data object has an attribute <string>=<Node ID> showing that it is the
	forwarding data object for the node with that node id. The string is used to
	identify which forwarding module generated the information.
	
	The data object also has an attribute Metric=<string> which encodes the 
	metrics. This is placed in a <Forwarding>...</Forwarding> section of the 
	data object.
*/

	/**
		This function creates a new forwarding data object. It takes a metric 
		string and replaces myMetricDO with a new data object.
		
		This function is placed here, because most forwarding modules will need
		to have this function, and doesn't need a different format.
		
		If the given string has any encoding that may break XML parsing, the 
		generated data object will most likely not be pareseable.
	*/
	void createMetricDataObject(string forwardingMetric);
	
	/**
		This function determines if the given data object is a metric data 
		object for this forwarding module. It checks to see if the data object
		has the format of a data object created by ::createMetricDataObject().
		
		Returns: true iff the data object conforms to the format used by 
		::createMetricDataObject() for the current forwarding module.
	*/
	virtual bool isMetricDO(const DataObjectRef dObj) const;
	
	/**
		This function returns a string with the node id for the node which 
		created the given metric data object.
		
		Returns: if isMetricDO() would return true, a string containing a node 
		id. Otherwise NULL.
	*/
	virtual const string getNodeIdFromMetricDataObject(DataObjectRef dObj) const;
	
	/**
		This function returns a string with the metric for the node which 
		created the given metric data object.
		
		Returns: if isMetricDO() would return true, a string containing a 
		metric. Otherwise NULL.
	*/
	virtual const string getMetricFromMetricDataObject(DataObjectRef dObj) const;
	
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
