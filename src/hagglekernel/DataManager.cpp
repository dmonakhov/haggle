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
#include <string.h>

#include <libcpphaggle/Platform.h>
#include <haggleutils.h>

#include "EventQueue.h"
#include "DataStore.h"
#include "DataManager.h"
#include "DataObject.h"
#include "Node.h"
#include "Event.h"
#include "Interface.h"
#include "Attribute.h"
#include "Filter.h"

using namespace haggle;

DataTask::DataTask(const DataTaskType_t _type, DataObjectRef _dObj) : 
                        type(_type), completed(false), dObj(_dObj)
{
}

 DataTask::~DataTask()
 {
 }

DataHelper::DataHelper(DataManager *m, const EventType _etype) : 
	ManagerModule<DataManager>(m, "DataHelper"), taskQ("DataHelper"), etype(_etype)
{
}

DataHelper::~DataHelper()
{
	while (!taskQ.empty()) {
		DataTask *task = NULL;
		taskQ.retrieve(&task);
		delete task;
	}
}

void DataHelper::doTask(DataTask *task)
{
        DataObject::DataState_t state = DataObject::DATA_STATE_NOT_VERIFIED;

	switch (task->type) {
		case DATA_TASK_VERIFY_DATA:
			HAGGLE_DBG("DataHelper tries to verify the data in a data object\n");
                        state = task->dObj->verifyData();

			if (state == DataObject::DATA_STATE_VERIFIED_BAD) {
				HAGGLE_ERR("Could not verify the data hash of data object! - Discarding...\n");
				delete task;
				return;
			} else if (state == DataObject::DATA_STATE_NOT_VERIFIED) {
                                HAGGLE_DBG("Could not verify data... No hash?\n");
                        } else if (state == DataObject::DATA_STATE_VERIFIED_OK) {
                                HAGGLE_DBG("Data object's data is OK\n");
                        } else if (state == DataObject::DATA_STATE_NO_DATA) {
                                HAGGLE_ERR("Instructed to verify a data object without data\n");
                        }
			task->completed = true;
			break;
		default:
                        HAGGLE_ERR("Unknown data task\n");
			delete task;
			return;
	}
	// Return result if the private event is valid
	if (Event::isPrivate(etype))
		addEvent(new Event(etype, task));
	else
		delete task;
}

bool DataHelper::run()
{	
	HAGGLE_DBG("DataHelper running...\n");
	
	while (!shouldExit()) {
		QueueEvent_t qe;
		DataTask *task = NULL;
		
		qe = taskQ.retrieve(&task);
		
		switch (qe) {
		case QUEUE_ELEMENT:
			doTask(task);
			break;
		case QUEUE_WATCH_ABANDONED:
			HAGGLE_DBG("DataHelper instructed to exit...\n");
			return false;
		default:
			HAGGLE_ERR("Unknown data task queue return value\n");
		}
	}
	return false;
}

void DataHelper::cleanup()
{
	while (!taskQ.empty()) {
		DataTask *task = NULL;
		taskQ.retrieve(&task);
		delete task;
	}
}

DataManager::DataManager(HaggleKernel * _kernel, const bool _setCreateTimeOnBloomfilterUpdate) : 
	Manager("DataManager", _kernel), localBF((float) 0.01, MAX_RECV_DATAOBJECTS, true),
	setCreateTimeOnBloomfilterUpdate(_setCreateTimeOnBloomfilterUpdate)
{
#define __CLASS__ DataManager
	int ret;

	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_VERIFIED, onVerifiedDataObject);

#if HAVE_EXCEPTION
	if (ret < 0)
		throw DMException(ret, "Could not register event");
#endif
	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_DELETED, onDeletedDataObject);

#if HAVE_EXCEPTION
	if (ret < 0)
		throw DMException(ret, "Could not register event");
#endif
	onInsertedDataObjectCallback = newEventCallback(onInsertedDataObject);
	onAgedDataObjectsCallback = newEventCallback(onAgedDataObjects);

	// Insert time stamp for when haggle starts up into the data store:
	RepositoryEntryRef timestamp = RepositoryEntryRef(new RepositoryEntry("DataManager", "Startup timestamp", Timeval::now().getAsString().c_str()));
	kernel->getDataStore()->insertRepository(timestamp);

	agingMaxAge = DEFAULT_AGING_MAX_AGE;
	agingPeriod = DEFAULT_AGING_PERIOD;

	agingEvent = registerEventType("Aging Event", onAging);

	// Start aging:
	onAgedDataObjects(NULL);
	
	dataTaskEvent = registerEventType("DataTaskEvent", onDataTaskComplete);

	helper = new DataHelper(this, dataTaskEvent);
	
	onGetLocalBFCallback = newEventCallback(onGetLocalBF);
	RepositoryEntryRef lbf = new RepositoryEntry("DataManager", "Local Bloomfilter");
	kernel->getDataStore()->readRepository(lbf, onGetLocalBFCallback);
	
	if (helper) {
		HAGGLE_DBG("Starting data helper...\n");
		helper->start();
	}
	
	if (setCreateTimeOnBloomfilterUpdate) {
		HAGGLE_DBG("Will set create time in node description when updating bloomfilter\n");
	}
}

DataManager::~DataManager()
{
	if (helper)
		delete helper;
	
	Event::unregisterType(dataTaskEvent);
	Event::unregisterType(agingEvent);
	
	if (onInsertedDataObjectCallback)
		delete onInsertedDataObjectCallback;

	if (onAgedDataObjectsCallback)
		delete onAgedDataObjectsCallback;

	if (onGetLocalBFCallback)
		delete onGetLocalBFCallback;
}

void DataManager::onShutdown()
{
	if (helper) {
		HAGGLE_DBG("Stopping data helper...\n");
		helper->stop();
	}
	// FIXME: why would the following crash the data store?
	/*
	RepositoryEntryRef lbf = 
		new RepositoryEntry(
			"DataManager", 
			"Local Bloomfilter",
			localBF.toBase64().c_str());
	kernel->getDataStore()->insertRepository(lbf);*/
	
	unregisterWithKernel();
}

void DataManager::onGetLocalBF(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	DataStoreQueryResult *qr = static_cast < DataStoreQueryResult * >(e->getData());
	// Are there any repository entries?
	if (qr->countRepositoryEntries() != 0) {
		RepositoryEntryRef re;
		
		// Then this is most likely the local bloomfilter:
		
		re = qr->detachFirstRepositoryEntry();
		// Was there a repository entry? => was this really what we expected?
		if (re) {
			// Yes:
			string str = re->getValue();
			localBF.fromBase64(str);
		}
		RepositoryEntryRef lbf = new RepositoryEntry("DataManager", "Local Bloomfilter");
		kernel->getDataStore()->deleteRepository(lbf);
	} else {
		// Don't do anything... for now.
	}
	
	delete qr;
}

#if defined(ENABLE_METADAPARSER)
bool DataManager::onParseMetadata(Metadata *md)
{

        //printf("\nDataManager: data object has data field\n\n");

        return true;
}
#endif
/*
	public event handler on verified DataObject
 
	means that the DataObject is verified by the SecurityManager
*/ 
void DataManager::onVerifiedDataObject(Event *e)
{
	if (!e || !e->hasData())
		return;

	DataObjectRef dObj = e->getDataObject();
	
	if(dObj == (DataObject *) NULL) {
		HAGGLE_DBG("Verified data object event without data object!\n");
		return;
	}
	HAGGLE_DBG("%s Received DataObject\n", getName());

#ifdef DEBUG
	char *raw;
	size_t len;

	dObj->getRawMetadataAlloc(&raw, &len);

	if (raw) {
		printf("DataObject id=%s METADATA:\n%s\n", dObj->getIdStr(), raw);
		free(raw);
	}
#endif

	if (dObj->getDataState() == DataObject::DATA_STATE_VERIFIED_BAD) {
		HAGGLE_ERR("Data in data object flagged as bad! -- discarding\n");
		return;
	} else if (dObj->getDataState() == DataObject::DATA_STATE_NOT_VERIFIED && helper) {
		// Call our helper to verify the data in the data object.
                if (dObj->dataIsVerifiable()) {
                        helper->addTask(new DataTask(DATA_TASK_VERIFY_DATA, dObj));
                        return;
                }
	}
	
	handleVerifiedDataObject(dObj);
}

void DataManager::handleVerifiedDataObject(DataObjectRef& dObj)
{
	// Add the data object to the bloomfilter of the one who sent it:
	// Find the interface it came from:
	InterfaceRef iface = dObj->getRemoteInterface();

	// Was there one?
	if (iface) {
		// Find the node associated with that interface
		NodeRef inStore = kernel->getNodeStore()->retrieve(iface);
		// Was there one?
		if (inStore) {
			// Is the sending node an application?
			if (inStore->getType() != NODE_TYPE_APPLICATION) {
				// No. Add the data object to the node's bloomfilter:
				inStore->getBloomfilter()->add(dObj);
				// yes:
				// Don't add the data object to the bloomfilter of the application
				// that sent it, since the correct behaviour is to deliver it to
				// the application if it wants it.
			}
		}
	}

	// insert into database (including filering)
	if (dObj->isPersistent()) {
		kernel->getDataStore()->insertDataObject(dObj, onInsertedDataObjectCallback);
	} else {
		// do not expect a callback for non-persistent data objects
		kernel->getDataStore()->insertDataObject(dObj, NULL);
	}
}

void DataManager::onDataTaskComplete(Event *e)
{
	if (!e || !e->hasData())
		return;

	DataTask *task = static_cast<DataTask *>(e->getData());

	if (task->type == DATA_TASK_VERIFY_DATA && task->completed) {
		handleVerifiedDataObject(task->dObj);
	}
	delete task;
}

void DataManager::onDeletedDataObject(Event * e)
{
	if (!e || !e->hasData())
		return;
	
	DataObjectRefList dObjs = e->getDataObjectList();
	
	for (DataObjectRefList::iterator it = dObjs.begin(); it != dObjs.end(); it++)
		localBF.remove(*it);
	
	if (dObjs.size() > 0)
		kernel->getThisNode()->setBloomfilter(localBF, setCreateTimeOnBloomfilterUpdate);
}

/*
	callback on successful insert of a DataObject into the DataStore,
        or if the data object was a duplicate, in which case the data object
        is marked as such
*/
void DataManager::onInsertedDataObject(Event * e)
{
	if (!e || !e->hasData())
		return;
	
	DataObjectRef dObj = e->getDataObject();
	
	/*
		The DATAOBJECT_NEW event signals that a new data object has been 
		received and is inserted into the data store. Other managers can now be
		sure that this data object exists in the data store - as long as they
		query it through the data store task queue (since the query will be 
		processed after the insertion task).
	*/
	if (dObj->isDuplicate()) {
		HAGGLE_DBG("Data object %s is a duplicate, but adding to bloomfilter to be sure\n", dObj->getIdStr());
	} else {
		kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_NEW, dObj));
	}

	if (dObj->isPersistent() && !localBF.has(dObj)) {
		localBF.add(dObj);
		kernel->getThisNode()->setBloomfilter(localBF, setCreateTimeOnBloomfilterUpdate);
	}
}

void DataManager::onAgedDataObjects(Event *e)
{
	if (!e || kernel->isShuttingDown())
		return;

	if (e->hasData()) {
		DataObjectRefList dObjs = e->getDataObjectList();

		HAGGLE_DBG("Aged %lu data objects\n", dObjs.size());

		if (dObjs.size() >= DATASTORE_MAX_DATAOBJECTS_AGED_AT_ONCE) {
			// Call onAging() immediately in case there are more data
			// objects to age.
			onAging(NULL);
		} else {
			// Delay the aging for one period
			kernel->addEvent(new Event(agingEvent, NULL, agingPeriod));
		}
	} else {
		// No data in event -> means this is the first time the function
		// is called and we should start the aging timer.
		kernel->addEvent(new Event(agingEvent, NULL, agingPeriod));
	}
}

void DataManager::onAging(Event *e)
{
	if (kernel->isShuttingDown())
		return;

	// Delete from the data store any data objects we're not interested
	// in and are too old.
	// FIXME: find a better way to deal with the age parameter. 
	kernel->getDataStore()->ageDataObjects(Timeval(agingMaxAge, 0), onAgedDataObjectsCallback);
}

void DataManager::onConfig(Event * e)
{
	DataObjectRef dObj = e->getDataObject();
	
	if (!dObj) 
		return;
	
	// extract metadata
	Metadata *m = dObj->getMetadata();
	
	if (!m) 
		return;
	
	// extract metadata relevant for ForwardingManager
	m = m->getMetadata(this->getName());
	
	if (!m) 
		return;
	
	bool agingHasChanged = false;
	
	Metadata *tmp = NULL;

	if ((tmp = m->getMetadata("AgingPeriod"))) {
		agingPeriod = atoi(tmp->getContent().c_str());
		HAGGLE_DBG("config agingPeriod=%d\n", agingPeriod);
		agingHasChanged = true;
	}
	if ((tmp = m->getMetadata("AgingMaxAge"))) {
		agingMaxAge = atoi(tmp->getContent().c_str());
		HAGGLE_DBG("config agingMaxAge=%d\n", agingMaxAge);
		agingHasChanged = true;
	}

	if (agingHasChanged)
		onAging(NULL);
}
