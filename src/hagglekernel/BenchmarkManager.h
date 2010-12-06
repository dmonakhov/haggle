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
#ifndef _BENCHMARKMANAGER_H
#define _BENCHMARKMANAGER_H

#include <libcpphaggle/Platform.h>

#if defined(BENCHMARK)

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/
class BenchmarkManager;

#include "Manager.h"
#include "Event.h"
#include "DataObject.h"
#include "Node.h"

/** */
class BenchmarkManager : public Manager
{
        unsigned int DataObjects_Attr;
        unsigned int Nodes_Attr;
        unsigned int Attr_Num;
        unsigned int DataObjects_Num;
        unsigned int Test_Num;
        EventCallback<EventHandler> *queryCallback;
        NodeRefList queryNodes;
        EventType evaluateEType;
	bool init_derived();
public:
        BenchmarkManager(HaggleKernel *_haggle = haggleKernel, unsigned int _DataObjects_Attr = 0, unsigned int _Nodess_Attr = 0, unsigned int _Attr_Num = 0, unsigned int _DataObjects_Num = 0, unsigned int _Test_Num = 0);
	~BenchmarkManager();

	int handleAttributes(Event* e);
	void onTimeout(Event* e);
	void insertNode(Event* e);
	void insertDataobject(Event* e);
	NodeRef createNode(unsigned int numAttr);
	DataObjectRef createDataObject(unsigned int numAttr);
	void onEvaluate(Event *e);
	void onQueryResult(Event* e);
	void onRetreiveNodes(Event *e);
};

#endif
#endif /* _BENCHMARKMANAGER_H */
