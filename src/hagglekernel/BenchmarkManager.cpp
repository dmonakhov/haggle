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

#if defined(BENCHMARK)

#include <libcpphaggle/Platform.h>
#include <libcpphaggle/String.h>

using namespace haggle;

#include <stdlib.h>
#include <stdio.h>

#include "EventQueue.h"
#include "BenchmarkManager.h"
#include "DataObject.h"
#include "Node.h"
#include "Interface.h"
#include "Event.h"
#include "Attribute.h"
#include "Filter.h"

#include <haggleutils.h>

#define FILTER_EVALUATE "benchmark=evaluate"

unsigned int Test_cnt = 0;

BenchmarkManager::BenchmarkManager(HaggleKernel * _kernel, unsigned int _DataObjects_Attr, unsigned int _Nodes_Attr, unsigned int _Attr_Num, unsigned int _DataObjects_Num, unsigned int _Test_Num) : 
	Manager("BenchmarkManager", _kernel)
{
	DataObjects_Attr = _DataObjects_Attr;
	Nodes_Attr = _Nodes_Attr;
	Attr_Num = _Attr_Num;
	DataObjects_Num = _DataObjects_Num;
	Test_Num = _Test_Num;
}

BenchmarkManager::~BenchmarkManager()
{
}

bool BenchmarkManager::init_derived()
{
#define __CLASS__ BenchmarkManager
	int ret;

	// private event: timeout
	EventType timeoutEType = registerEventType("BenchmarkManager Timeout Event", onTimeout);
	
	if (timeoutEType < 0) {
		HAGGLE_ERR("Could not register Timeout Event...\n");
		return false;
	}
	kernel->addEvent(new Event(timeoutEType, NULL, 1.0));
	
	/*
	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_SEND,onEvaluate);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event\n");
		return false;
	}
	*/
	queryCallback = newEventCallback(onQueryResult);

	if (!queryCallback) {
		HAGGLE_ERR("Could not create queryCallback\n");
		return false;
	}

	// private event: filter
	evaluateEType = registerEventType("BenchmarkManager EvalFilter Event", onEvaluate);

	if (evaluateEType < 0) {
		HAGGLE_ERR("Could not register EvalFilter Event...");
		return false;
	}

	Filter evaluateFilter(FILTER_EVALUATE, evaluateEType);

	kernel->getDataStore()->insertFilter(evaluateFilter);

	return true;
}

int BenchmarkManager::handleAttributes(Event *e)
{
	if (!e)
		return -1;

	return 1;
}

#define USE_PREGENERATED_DATABASE

void BenchmarkManager::onTimeout(Event *e)	// start evaluation
{
	HAGGLE_LOG("Starting benchmark\n");

#ifdef USE_PREGENERATED_DATABASE
	kernel->getDataStore()->retrieveNode(Node::TYPE_PEER, newEventCallback(onRetreiveNodes));
#else
	// Generate and insert nodes
	for (unsigned int n = 0; n < Test_Num; n++) {
		NodeRef node = createNode(Nodes_Attr);
		HAGGLE_LOG("Generating and inserting node %d\n", n);

		kernel->getDataStore()->insertNode(node, newEventCallback(insertDataobject));
		queryNodes.push_back(node);
	}

	// Generate and insert data objects
	// insertDataobject() is called once from here, then with asynchronous callbacks from Datastore::insertDataobject() until DataObjects_Num is reached
	insertDataobject(NULL);
#endif
}

void BenchmarkManager::onRetreiveNodes(Event *e)
{
	Watch w;
	Timeval timeout(3, 0);

	NodeRefList *nodes = static_cast < NodeRefList * >(e->getData());

	queryNodes = *nodes;

	printf("Retreived %lu nodes from the data store\n", queryNodes.size());

	delete nodes;

	printf("Starting benchmark in 3 seconds\n");

	w.wait(&timeout);

	onEvaluate(NULL);
}

NodeRef BenchmarkManager::createNode(unsigned int numAttr)
{
	char name[128];
	char value[128];
	char nodeid[128];
	char nodename[128];
	static unsigned long id = 0;
	unsigned long r;

	id++;
	
	char macaddr[6];
	macaddr[0] = (char) RANDOM_INT(255);
	macaddr[1] = (char) RANDOM_INT(255);
	macaddr[2] = (char) RANDOM_INT(255);
	macaddr[3] = (char) RANDOM_INT(255);
	macaddr[4] = (char) RANDOM_INT(255);
	macaddr[5] = (char) RANDOM_INT(255);
	
	Address addr(AddressType_EthMAC, (unsigned char *) macaddr);
	InterfaceRef iface = new Interface(IFTYPE_ETHERNET, macaddr, &addr);
	sprintf(nodeid, "%ld", id);
	sprintf(nodename, "node %ld", id);
	
	NodeRef node = Node::create_with_id(Node::TYPE_PEER, nodeid, nodename);

	if (!node)
		return NULL;

	node->addInterface(iface);

	for (unsigned int i = 0; i < numAttr; i++) {
		int tries = 0;
		do {
			r = RANDOM_INT(Attr_Num);
			sprintf(name, "name");
			sprintf(value, "value %lu", r);
			if (tries++ > 10) {
				HAGGLE_ERR("WARNING: Cannot generate unique attributes in data object... check attribute pool size!\n");
				break;
			}
		} while (node->getAttribute(name, value));

		int weight = RANDOM_INT(10);
		node->addAttribute(name, value, weight);
	}

	return node;
}


void BenchmarkManager::insertDataobject(Event* e) 
{
	static unsigned int n = 0;

	n++;

	DataObjectRef dObj = createDataObject(DataObjects_Attr);
	HAGGLE_LOG("Generating and inserting dataobject %d\n", n);
	
	if (n == DataObjects_Num - 1) {
		// mark last node to get evaluation starte
		// after its insert
		dObj->addAttribute("benchmark", "evaluate");
		HAGGLE_LOG("Inserted final data object... "
			  "waiting for the data store to finish before starting test. "
			   "This may take a while.\n");
		kernel->getDataStore()->insertDataObject(dObj);
	} else {
		kernel->getDataStore()->insertDataObject(dObj, newEventCallback(insertDataobject));
	}
}


DataObjectRef BenchmarkManager::createDataObject(unsigned int numAttr)
{
	char name[128];
	char value[128];
	unsigned int r;

	char macaddr[6];
	macaddr[0] = (char) RANDOM_INT(255);
	macaddr[1] = (char) RANDOM_INT(255);
	macaddr[2] = (char) RANDOM_INT(255);
	macaddr[3] = (char) RANDOM_INT(255);
	macaddr[4] = (char) RANDOM_INT(255);
	macaddr[5] = (char) RANDOM_INT(255);
	
	char macaddr2[6];
	macaddr2[0] = (char) RANDOM_INT(255);
	macaddr2[1] = (char) RANDOM_INT(255);
	macaddr2[2] = (char) RANDOM_INT(255);
	macaddr2[3] = (char) RANDOM_INT(255);
	macaddr2[4] = (char) RANDOM_INT(255);
	macaddr2[5] = (char) RANDOM_INT(255);
	
	Address addr(AddressType_EthMAC, (unsigned char *) macaddr);
	Address addr2(AddressType_EthMAC, (unsigned char *) macaddr2);
	InterfaceRef localIface = new Interface(IFTYPE_ETHERNET, macaddr, &addr);		
	InterfaceRef remoteIface = new Interface(IFTYPE_ETHERNET, macaddr2, &addr2);		
	DataObjectRef dObj = DataObject::create(NULL, 0, localIface, remoteIface);

	for (unsigned int i = 0; i < numAttr; i++) {
		int tries = 0;
		do {
			r = RANDOM_INT(Attr_Num);
			sprintf(name, "name");
			sprintf(value, "value %d", r);
			if (tries++ > 10) {
				HAGGLE_ERR("WARNING: Cannot generate unique attributes in data object... check attribute pool size!\n");
				break;
			}
		} while (dObj->getAttribute(name, value));

		dObj->addAttribute(name, value, r);
	}

	return dObj;
}


void BenchmarkManager::onEvaluate(Event *e)
{
	HAGGLE_LOG("Got filter event: Starting evaluation in 3 secs...\n");

	kernel->addEvent(new Event(queryCallback, NULL, 3.0));
//	kernel->addEvent(new Event(queryCallback));
}

void BenchmarkManager::onQueryResult(Event *e)
{
	DataStoreQueryResult *qr = static_cast < DataStoreQueryResult * >(e->getData());
	int n = 0;
	static int queries = 1;

	if (!qr)
		goto do_query;

	printf("Got query result\n");

	while (true) {
		DataObjectRef dObj = qr->detachFirstDataObject();

		if (!dObj)
			break;
		n++;
	}

	BENCH_TRACE(BENCH_TYPE_INIT, qr->getQueryInitTime(), 0);
	BENCH_TRACE(BENCH_TYPE_QUERYSTART, qr->getQuerySqlStartTime(), 0);
	BENCH_TRACE(BENCH_TYPE_QUERYEND, qr->getQuerySqlEndTime(), 0);
	BENCH_TRACE(BENCH_TYPE_RESULT, qr->getQueryResultTime(), n);
	BENCH_TRACE(BENCH_TYPE_END, Timeval::now(), 0);

	delete qr;

	HAGGLE_LOG("%d data objects in query response %d\n", n, queries++);

      do_query:

	HAGGLE_LOG("Doing query %d\n", queries);

	NodeRef node = queryNodes.pop();

	if (!node) {
		HAGGLE_LOG("finished\n");
		BENCH_TRACE_DUMP(DataObjects_Attr, Nodes_Attr, Attr_Num, DataObjects_Num);
		exit(1);
	}

	kernel->getDataStore()->doDataObjectQuery(node, 1, queryCallback);
}
#endif
