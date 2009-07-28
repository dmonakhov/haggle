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
	EventType dataTaskEvent;
	EventType agingEvent;
	DataHelper *helper;
	Bloomfilter localBF;
public:
        DataManager(HaggleKernel *_haggle = haggleKernel);
        ~DataManager();
        void onGetLocalBF(Event *e);
        void setThisNodeBF(void);
private:
	void handleVerifiedDataObject(DataObjectRef& dObj);
        void onVerifiedDataObject(Event *e);
	void onInsertedDataObject(Event *e);
        void onDeletedDataObject(Event *e);
        void onNewRelation(Event *e);
	void onDataTaskComplete(Event *e);
	void onAging(Event *e);
	void onShutdown();
#if defined(ENABLE_METADAPARSER)
        bool onParseMetadata(Metadata *m);
#endif
        class DMException : public ManagerException
        {
            public:
                DMException(const int err = 0, const char* data = "Data manager Error") : ManagerException(err, data) {}
        };
};


#endif /* _DATAMANAGER_H */
