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
	if (isRunning())
		stop();
	
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
	Manager("DataManager", _kernel), localBF(NULL), 
	setCreateTimeOnBloomfilterUpdate(_setCreateTimeOnBloomfilterUpdate), 
	keepInBloomfilterOnAging(true)
{	
	if (setCreateTimeOnBloomfilterUpdate) {
		HAGGLE_DBG("Will set create time in node description when updating bloomfilter\n");
	}
}

bool DataManager::init_derived()
{
#define __CLASS__ DataManager
	int ret;

	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_VERIFIED, onVerifiedDataObject);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event\n");
		return false;
	}

	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_DELETED, onDeletedDataObject);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event\n");
		return false;
	}
	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_INCOMING, onIncomingDataObject);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event\n");
		return false;
	}
	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL, onSendResult);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event\n");
		return false;
	}

#if defined(DEBUG)
	ret = setEventHandler(EVENT_TYPE_DEBUG_CMD, onDebugCmd);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event\n");
		return false;
	}
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
	
	if (!helper) {
		HAGGLE_ERR("Could not create data manager helper\n");
		return false;
	}
	
	localBF = Bloomfilter::create(Bloomfilter::BF_TYPE_COUNTING);
	
	if (!localBF) {
		HAGGLE_ERR("Could not create data manager bloomfilter\n");
		return false;
	}
	onGetLocalBFCallback = newEventCallback(onGetLocalBF);
	RepositoryEntryRef lbf = new RepositoryEntry(getName(), "Bloomfilter");
	kernel->getDataStore()->readRepository(lbf, onGetLocalBFCallback);
	
	HAGGLE_DBG("Starting data helper...\n");
	helper->start();

	return true;
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
	
	if (localBF)
		delete localBF;
}

void DataManager::onShutdown()
{
	if (helper) {
		HAGGLE_DBG("Stopping data helper...\n");
		helper->stop();
	}
	
	RepositoryEntryRef lbf = new RepositoryEntry(getName(), "Bloomfilter", localBF->getRaw(), localBF->getRawLen());
	
	kernel->getDataStore()->insertRepository(lbf);
	
	unregisterWithKernel();
}

#ifdef DEBUG
void DataManager::onDebugCmd(Event *e)
{
	if (e) {
		if (e->getDebugCmd()->getType() == DBG_CMD_PRINT_DATAOBJECTS) {
			unsigned int n = 0;
			
			printf("+++++++++++++++++++++++++++++++\n");
			printf("%u last data objects sent:\n", MAX_DATAOBJECTS_LISTED);
			printf("-------------------------------\n");

			for (List<string>::iterator it = dataObjectsSent.begin(); it != dataObjectsSent.end(); it++) {
				printf("%u %s\n", n++, (*it).c_str());
			}
			printf("%u last data objects received:\n", MAX_DATAOBJECTS_LISTED);
			printf("-------------------------------\n");

			n = 0;

			for (List<string>::iterator it = dataObjectsReceived.begin(); it != dataObjectsReceived.end(); it++) {
				printf("%u %s\n",  n++, (*it).c_str());
			}
			printf("+++++++++++++++++++++++++++++++\n");
		}
	}
}
#endif

void DataManager::onGetLocalBF(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	DataStoreQueryResult *qr = static_cast < DataStoreQueryResult * >(e->getData());
	
	HAGGLE_DBG("Got repository callback\n");
	
	// Are there any repository entries?
	if (qr->countRepositoryEntries() != 0) {
		RepositoryEntryRef re;
		
		// Then this is most likely the local bloomfilter:
		
		re = qr->detachFirstRepositoryEntry();
		// Was there a repository entry? => was this really what we expected?
		if (re) {
			HAGGLE_DBG("Retrieved bloomfilter from data store\n");
			// Yes:
			
			Bloomfilter *tmpBF = Bloomfilter::create(re->getValueBlob(), re->getValueLen());

			if (tmpBF) {
				if (localBF)
					delete localBF;
				
				localBF = tmpBF;
				kernel->getThisNode()->setBloomfilter(*localBF, setCreateTimeOnBloomfilterUpdate);
			}
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
	
	if (!dObj) {
		HAGGLE_DBG("Verified data object event without data object!\n");
		return;
	}

#ifdef DEBUG
	if (dataObjectsReceived.size() >= MAX_DATAOBJECTS_LISTED) {
		dataObjectsReceived.pop_front();
	}
	dataObjectsReceived.push_back(dObj->getIdStr());
#endif

	HAGGLE_DBG("%s Received data object [%s]\n", getName(), dObj->getIdStr());

#ifdef DEBUG
	dObj->print();
#endif
	if (dObj->getSignatureStatus() == DataObject::SIGNATURE_INVALID) {
		// This data object had a bad signature, we should remove
		// it from the bloomfilter
		HAGGLE_DBG("Data object [%s] had bad signature, removing from bloomfilter\n", dObj->getIdStr());
		localBF->remove(dObj);
		kernel->getThisNode()->setBloomfilter(*localBF, setCreateTimeOnBloomfilterUpdate);
		return;
	}

	if (dObj->getDataState() == DataObject::DATA_STATE_VERIFIED_BAD) {
		HAGGLE_ERR("Data in data object flagged as bad! -- discarding\n");
		if (localBF->has(dObj)) {
			// Remove the data object from the bloomfilter since it was bad.
			localBF->remove(dObj);
			kernel->getThisNode()->setBloomfilter(*localBF, setCreateTimeOnBloomfilterUpdate);
		}
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

void DataManager::onSendResult(Event *e)
{
	DataObjectRef& dObj = e->getDataObject();
	NodeRef node = e->getNode();

	if (!dObj) {
		HAGGLE_ERR("No data object in send result\n");	
		return;
	}

	if (!node) {
		HAGGLE_ERR("No node in send result\n");	
		return;
	}

	if (!node->isStored()) {
		HAGGLE_DBG("Send result node %s is not in node store, trying to retrieve\n", node->getName().c_str());
		NodeRef peer = kernel->getNodeStore()->retrieve(node);

		if (peer) {
			HAGGLE_DBG("Found node %s in node store, using the one in store\n", node->getName().c_str());
			node = peer;
		}
	}
	if (e->getType() == EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL) {
		// Add data object to node's bloomfilter.
		HAGGLE_DBG("Adding data object [%s] to node %s's bloomfilter\n", dObj->getIdStr(), node->getName().c_str());
		node->getBloomfilter()->add(dObj);

#ifdef DEBUG
	if (dataObjectsSent.size() >= MAX_DATAOBJECTS_LISTED) {
		dataObjectsSent.pop_front();
	}
	dataObjectsSent.push_back(dObj->getIdStr());
#endif

	}
}

void DataManager::onIncomingDataObject(Event *e)
{
	if (!e || !e->hasData())
		return;

	DataObjectRef& dObj = e->getDataObject();
	
	if (!dObj) {
		HAGGLE_DBG("Incoming data object event without data object!\n");
		return;
	}
	// Add the data object to the bloomfilter of the one who sent it:
	NodeRef peer = e->getNode();

	if (!peer || peer->getType() == Node::TYPE_UNDEF) {
		// No valid node in event, try to figure out from interface

		// Find the interface it came from:
		const InterfaceRef& iface = dObj->getRemoteInterface();

		if (iface) {
			peer = kernel->getNodeStore()->retrieve(iface);
		} else {
			HAGGLE_DBG("No valid peer interface in data object, cannot figure out peer node\n");
		}
	}
	
	if (peer) {
		if (peer->getType() != Node::TYPE_APPLICATION && peer->getType() != Node::TYPE_UNDEF) {
			// Add the data object to the peer's bloomfilter so that
			// we do not send the data object back.
			HAGGLE_DBG("Adding data object [%s] to peer node %s's (%s num=%lu) bloomfilter\n", 
				dObj->getIdStr(), peer->getName().c_str(), peer->isStored() ? "stored" : "not stored", peer->getNum());
			/*
			LOG_ADD("%s: BLOOMFILTER:ADD %s\t%s:%s\n", 
				Timeval::now().getAsString().c_str(), dObj->getIdStr(), 
				peer->getTypeStr(), peer->getIdStr());
			*/
			peer->getBloomfilter()->add(dObj);

		}
	} else {
		HAGGLE_DBG("No valid peer node for incoming data object [%s]\n", dObj->getIdStr());
	}

	// Check if this is a control message from an application. We do not want 
	// to bloat our bloomfilter with such messages, because they are sent
	// everytime an application connects.
	if (!dObj->isControlMessage()) {
		// Add the incoming data object also to our own bloomfilter
		// We do this early in order to avoid receiving duplicates in case
		// the same object is received at nearly the same time from multiple neighbors
		if (localBF->has(dObj)) {
			HAGGLE_DBG("Data object [%s] already in our bloomfilter, marking as duplicate...\n", dObj->getIdStr());
			dObj->setDuplicate();
		} else {
			HAGGLE_DBG("Adding data object [%s] to our bloomfilter\n", dObj->getIdStr());
			localBF->add(dObj);
			kernel->getThisNode()->setBloomfilter(*localBF, setCreateTimeOnBloomfilterUpdate);
		}
	}
}

void DataManager::handleVerifiedDataObject(DataObjectRef& dObj)
{
	// insert into database (including filtering)
	if (dObj->isPersistent()) {
		kernel->getDataStore()->insertDataObject(dObj, onInsertedDataObjectCallback);
	} else {
		// do not expect a callback for a non-persistent data object,
		// but we still call insertDataObject in order to filter the data object.
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
	unsigned int n_removed = 0;

	for (DataObjectRefList::iterator it = dObjs.begin(); it != dObjs.end(); it++) {
		/* 
		  Do not remove Node descriptions from the bloomfilter. We do not
		  want to receive old node descriptions again.
		  If the flag in the event is set, it means we should keep the data object
		  in the bloomfilter.
		*/
		if (!(*it)->isNodeDescription() && !e->getFlags()) {
			HAGGLE_DBG("Removing deleted data object [id=%s] from bloomfilter\n", (*it)->getIdStr());
			localBF->remove(*it);
			n_removed++;
		} else {
			HAGGLE_DBG("Keeping deleted data object [id=%s] in bloomfilter\n", (*it)->getIdStr());
		}
	}
	
	if (n_removed > 0)
		kernel->getThisNode()->setBloomfilter(*localBF, setCreateTimeOnBloomfilterUpdate);
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
	
	DataObjectRef& dObj = e->getDataObject();
	
	/*
		The DATAOBJECT_NEW event signals that a new data object has been 
		received and is inserted into the data store. Other managers can now be
		sure that this data object exists in the data store - as long as they
		query it through the data store task queue (since the query will be 
		processed after the insertion task).
	*/
	if (dObj->isDuplicate()) {
		HAGGLE_DBG("Data object %s is a duplicate! Not generating DATAOBJECT_NEW event\n", dObj->getIdStr());
	} else {
		kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_NEW, dObj));
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
			kernel->addEvent(new Event(agingEvent, NULL, (double)agingPeriod));
		}
	} else {
		// No data in event -> means this is the first time the function
		// is called and we should start the aging timer.
		kernel->addEvent(new Event(agingEvent, NULL, (double)agingPeriod));
	}
}

void DataManager::onAging(Event *e)
{
	if (kernel->isShuttingDown())
		return;

	// Delete from the data store any data objects we're not interested
	// in and are too old.
	// FIXME: find a better way to deal with the age parameter. 
	kernel->getDataStore()->ageDataObjects(Timeval(agingMaxAge, 0), onAgedDataObjectsCallback, keepInBloomfilterOnAging);
}

void DataManager::onConfig(Metadata *m)
{
	bool agingHasChanged = false;
	
	const char *param = m->getParameter("set_createtime_on_bloomfilter_update");
	
	if (param) {
		if (strcmp(param, "true") == 0) {
			setCreateTimeOnBloomfilterUpdate = true;
			HAGGLE_DBG("setCreateTimeOnBloomfilterUpdate set to 'true'\n");
		}
		else if (strcmp(param, "false") == 0) {
			setCreateTimeOnBloomfilterUpdate = false;
			HAGGLE_DBG("setCreateTimeOnBloomfilterUpdate set to 'false'\n");
		}
	}

	Metadata *dm = m->getMetadata("Bloomfilter");

	if (dm) {
		const char *param = dm->getParameter("default_error_rate");

		if (param) {
			char *endptr = NULL;
#if defined(OS_WINDOWS)
			double error_rate = atof(param);
			endptr = (char *)(param + strlen(param));
#else
			double error_rate = strtof(param, &endptr);
#endif			
			if (endptr && endptr != param) {
				Bloomfilter::setDefaultErrorRate(error_rate);
				HAGGLE_DBG("config default bloomfilter error rate %.3f\n", 
					   Bloomfilter::getDefaultErrorRate());
			}
		}
		
		param = dm->getParameter("default_capacity");

		if (param) {
			char *endptr = NULL;
			unsigned int capacity = (unsigned int)strtoul(param, &endptr, 10);
			
			if (endptr && endptr != param) {
				Bloomfilter::setDefaultCapacity(capacity);
				HAGGLE_DBG("config default bloomfilter capacity %u\n", 
					   Bloomfilter::getDefaultCapacity());
			}
		}
	}

	dm = m->getMetadata("Aging");

	if (dm) {
		const char *param = dm->getParameter("period");
		
		if (param) {
			char *endptr = NULL;
			unsigned long period = strtoul(param, &endptr, 10);
			
			if (endptr && endptr != param) {
				agingPeriod = period;
				HAGGLE_DBG("config agingPeriod=%lu\n", agingPeriod);
				LOG_ADD("# %s: agingPeriod=%lu\n", getName(), agingPeriod);
				agingHasChanged = true;
			}
		}
		
		param = dm->getParameter("max_age");
		
		if (param) {
			char *endptr = NULL;
			unsigned long period = strtoul(param, &endptr, 10);
			
			if (endptr && endptr != param) {
				agingMaxAge = period;
				HAGGLE_DBG("config agingMaxAge=%lu\n", agingMaxAge);
				LOG_ADD("# %s: agingMaxAge=%lu\n", getName(), agingPeriod);
				agingHasChanged = true;
			}
		}
		
		param = dm->getParameter("keep_in_bloomfilter");
		
		if (param) {
			if (strcmp(param, "true") == 0) {
				HAGGLE_DBG("config aging: keep_in_bloomfilter=true\n");
				LOG_ADD("# %s: aging: keep_in_bloomfilter=true\n", getName());
				keepInBloomfilterOnAging = true;
			} else if (strcmp(param, "false") == 0) {
				HAGGLE_DBG("config aging: keep_in_bloomfilter=false\n");
				LOG_ADD("# %s: aging: keep_in_bloomfilter=false\n", getName());
				keepInBloomfilterOnAging = false;
			}
		}
	}
	
	if (agingHasChanged)
		onAging(NULL);
}
