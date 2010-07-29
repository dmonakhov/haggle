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
#ifndef _DATAMANAGER_H
#define _DATAMANAGER_H


// default values for simple aging
#define DEFAULT_AGING_MAX_AGE 24*3600	// max age of data objects [s]
#define DEFAULT_AGING_PERIOD  3600	// period between aging processes [s]



/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/
class DataManager;

#include "Manager.h"
#include "Event.h"
#include "Bloomfilter.h"

typedef enum {
	DATA_TASK_VERIFY_DATA,
} DataTaskType_t;

class DataTask {
public:
	DataTaskType_t type;
	bool completed;
	DataObjectRef dObj;
	DataTask(const DataTaskType_t _type, DataObjectRef _dObj = NULL);
        ~DataTask();
};

class DataHelper : public ManagerModule<DataManager> {
	GenericQueue<DataTask *> taskQ;
	const EventType etype; // Private event used to communicate with data manager
	void doTask(DataTask *task);
	bool run();
        void cleanup();
public:
	DataHelper(DataManager *m, const EventType _etype);
	~DataHelper();

	bool addTask(DataTask *task) { return taskQ.insert(task); }
};

/** */
class DataManager : public Manager
{
	EventCallback <EventHandler> *onInsertedDataObjectCallback;
	EventCallback <EventHandler> *onGetLocalBFCallback;
	EventCallback <EventHandler> *onAgedDataObjectsCallback;
	EventType dataTaskEvent;
	EventType agingEvent;
	DataHelper *helper;
	/* We keep a local bloomfilter in the data manager in addition to the one in "this node". 
	 The reason for this is that the local bloomfilter here is a counting bloomfilter that
	 allows us to both add and remove data from it. When we update the local bloomfilter,
	 we also convert it into a non-counting bloomfilter that we set in "this node". The 
	 non-counting version is much smaller in size, and hence more suitable for sending out
	 in the node description.
	 */
	Bloomfilter *localBF;
	bool setCreateTimeOnBloomfilterUpdate;
	unsigned long agingMaxAge;
	unsigned long agingPeriod;
#if defined(DEBUG)
#define MAX_DATAOBJECTS_LISTED 10
	List<string> dataObjectsSent; // List of data objects sent.
	List<string> dataObjectsReceived; // List of data objects received.
	void onDebugCmd(Event *e);
#endif
public:
        DataManager(HaggleKernel *_haggle = haggleKernel, bool setCreateTimeOnBloomfilterUpdate = false);
        ~DataManager();
        void onGetLocalBF(Event *e);
private:
	void handleVerifiedDataObject(DataObjectRef& dObj);
        void onVerifiedDataObject(Event *e);
	void onInsertedDataObject(Event *e);
        void onDeletedDataObject(Event *e);
	void onSendResult(Event *e);
	void onIncomingDataObject(Event *e);
        void onNewRelation(Event *e);
	void onDataTaskComplete(Event *e);
	void onAgedDataObjects(Event *e);
	void onAging(Event *e);
	void onShutdown();
	void onConfig(Metadata *m);
#if defined(ENABLE_METADAPARSER)
        bool onParseMetadata(Metadata *m);
#endif
	bool init_derived();
};


#endif /* _DATAMANAGER_H */
