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
#include "DataStore.h"
#include <libcpphaggle/String.h>
#include <haggleutils.h>

using namespace haggle;

DataStoreQueryResult::DataStoreQueryResult()
{
	queryInit.zero();
	querySqlStart.zero();
	querySqlEnd.zero();
	queryResult.zero();
}

DataStoreQueryResult::~DataStoreQueryResult()
{
}

int DataStoreQueryResult::addNode(NodeRef& n)
{
	nodes.add(n);

	return nodes.size();
}

int DataStoreQueryResult::addDataObject(DataObjectRef& dObj)
{
	dataObjects.add(dObj);

	return dataObjects.size();
}

NodeRef DataStoreQueryResult::detachFirstNode()
{
	if (nodes.empty())
		return NULL;

	return nodes.pop();
}

DataObjectRef DataStoreQueryResult::detachFirstDataObject()
{
	if (dataObjects.empty())
		return NULL;

	return dataObjects.pop();
}

const NodeRefList *DataStoreQueryResult::getNodeList() const
{
	return &nodes;
}

const DataObjectRefList *DataStoreQueryResult::getDataObjectList() const
{
	return &dataObjects;
}

int DataStoreQueryResult::countDataObjects()
{
	return dataObjects.size();
}

int DataStoreQueryResult::countNodes()
{
	return nodes.size();
}

int DataStoreQueryResult::addRepositoryEntry(RepositoryEntryRef& re) 
{
	repositoryEntries.add(re);
	
	return repositoryEntries.size();

}

RepositoryEntryRef DataStoreQueryResult::detachFirstRepositoryEntry() 
{
	if (repositoryEntries.empty())
		return NULL;
	
	return repositoryEntries.pop();
}

int DataStoreQueryResult::countRepositoryEntries()
{
	return repositoryEntries.size();
}

DataStoreRepositoryQuery::DataStoreRepositoryQuery(const RepositoryEntryRef& _q, EventCallback<EventHandler> *_callback) 
	: query(_q), callback(_callback)
{
}

const RepositoryEntryRef DataStoreRepositoryQuery::getQuery() const
{
	return query;
}

const EventCallback<EventHandler> *DataStoreRepositoryQuery::getCallback() const
{
	return callback;
}

const char *DataStoreTask::taskName[_TASK_MAX] = {
	"TASK_INSERT_DATAOBJECT",
	"TASK_DELETE_DATAOBJECT",
	"TASK_AGE_DATAOBJECTS",
	"TASK_INSERT_NODE",
	"TASK_DELETE_NODE",
	"TASK_RETRIEVE_NODE",
	"TASK_RETRIEVE_NODE_BY_TYPE",
	"TASK_RETRIEVE_NODE_BY_INTERFACE",
	"TASK_ADD_FILTER",
	"TASK_DELETE_FILTER",
	"TASK_FILTER_QUERY",
	"TASK_DATAOBJECT_QUERY",
	"TASK_DATAOBJECT_FOR_NODES_QUERY",
	"TASK_NODE_QUERY",
	"TASK_INSERT_REPOSITORY",
	"TASK_READ_REPOSITORY",
	"TASK_DELETE_REPOSITORY",
	"TASK_DUMP_DATASTORE",
	"TASK_DUMP_DATASTORE_TO_FILE",
#ifdef DEBUG_DATASTORE
	"TASK_DEBUG_PRINT",
#endif
	"TASK_EXIT"
};

unsigned long DataStoreTask::totNum = 0;

DataStoreTask::DataStoreTask(DataObjectRef& _dObj, TaskType _type, const EventCallback<EventHandler> *_callback) : 
	HeapItem(), type(_type), priority(TASK_PRIORITY_LOW), num(totNum++), timestamp(Timeval::now()), dObj(_dObj.copy()), 
	callback(_callback), boolParameter(false) 
{
	if (type == TASK_INSERT_DATAOBJECT) {
		priority = TASK_PRIORITY_HIGH;
	} else if (type == TASK_DELETE_DATAOBJECT) {
	} else {
		HAGGLE_ERR("Tried to create a data store task with the wrong task for the data. (task type = %s)\n", taskName[type]);
	}
}
DataStoreTask::DataStoreTask(const DataObjectId_t _id, TaskType _type, const EventCallback<EventHandler> *_callback) :
	HeapItem(), type(_type), priority(TASK_PRIORITY_LOW), num(totNum++), timestamp(Timeval::now()), callback(_callback), boolParameter(true) 
{
	if (type == TASK_DELETE_DATAOBJECT) {
		memcpy(id, _id, sizeof(DataObjectId_t));
	} else {
		HAGGLE_ERR("Tried to create a data store task with the wrong task for the data. (task type = %s)\n", taskName[type]);
	}
}
DataStoreTask::DataStoreTask(const NodeRef& _node, TaskType _type, const EventCallback<EventHandler> *_callback, bool _boolParameter) :
	HeapItem(), type(_type), priority(TASK_PRIORITY_LOW), num(totNum++), timestamp(Timeval::now()), node(_node.copy()), callback(_callback), boolParameter(_boolParameter) 
{
	if (type == TASK_INSERT_NODE ||
		type == TASK_RETRIEVE_NODE) {
			priority = TASK_PRIORITY_HIGH;
	} else if (
		type == TASK_DELETE_NODE) {
	} else {
		HAGGLE_ERR("Tried to create a data store task with the wrong task for the data. (task type = %s)\n", taskName[type]);
	}
}
DataStoreTask::DataStoreTask(NodeType_t _nodeType, TaskType _type, const EventCallback<EventHandler> *_callback) :
	HeapItem(), type(_type), priority(TASK_PRIORITY_MEDIUM), num(totNum++), timestamp(Timeval::now()), nodeType(_nodeType), callback(_callback), boolParameter(false) 
{
	if (type == TASK_RETRIEVE_NODE_BY_TYPE) {
	} else {
		HAGGLE_ERR("Tried to create a data store task with the wrong task for the data. (task type = %s)\n", taskName[type]);
	}
}
DataStoreTask::DataStoreTask(const InterfaceRef& _iface, TaskType _type, const EventCallback<EventHandler> *_callback, bool _boolParameter) :
HeapItem(), type(_type), priority(TASK_PRIORITY_HIGH), num(totNum++), timestamp(Timeval::now()), iface(_iface.copy()), callback(_callback), boolParameter(_boolParameter) 
{
	if (type != TASK_RETRIEVE_NODE_BY_INTERFACE) {
		priority = TASK_PRIORITY_LOW;
		HAGGLE_ERR("Tried to create a data store task with the wrong task for the data. (task type = %s)\n", taskName[type]);
	}
}
DataStoreTask::DataStoreTask(DataStoreFilterQuery *q, TaskType _type) :
	HeapItem(), type(_type), priority(TASK_PRIORITY_MEDIUM), num(totNum++), timestamp(Timeval::now()), query(q), callback(NULL), boolParameter(false) 
{
	if (type == TASK_FILTER_QUERY) {
	} else {
		HAGGLE_ERR("Tried to create a data store task with the wrong task for the data. (task type = %s)\n", taskName[type]);
	}
}

DataStoreTask::DataStoreTask(DataStoreDataObjectQuery *q, TaskType _type) :
	HeapItem(), type(_type), priority(TASK_PRIORITY_MEDIUM), num(totNum++), timestamp(Timeval::now()), DOQuery(q), callback(NULL), boolParameter(false) 
{
	if (type == TASK_DATAOBJECT_QUERY) {
	} else {
		HAGGLE_ERR("Tried to create a data store task with the wrong task for the data. (task type = %s)\n", taskName[type]);
	}
}

DataStoreTask::DataStoreTask(DataStoreDataObjectForNodesQuery *q, TaskType _type) :
	HeapItem(), type(_type), priority(TASK_PRIORITY_MEDIUM), num(totNum++), timestamp(Timeval::now()), DOForNodesQuery(q), callback(NULL), boolParameter(false) 
{
	if (type == TASK_DATAOBJECT_FOR_NODES_QUERY) {
	} else {
		HAGGLE_ERR("Tried to create a data store task with the wrong task for the data. (task type = %s)\n", taskName[type]);
	}
}

DataStoreTask::DataStoreTask(DataStoreNodeQuery *q, TaskType _type) :
	HeapItem(), type(_type), priority(TASK_PRIORITY_MEDIUM), num(totNum++), timestamp(Timeval::now()), NodeQuery(q), callback(NULL), boolParameter(false) 
{
	if (type == TASK_NODE_QUERY) {
	} else {
		HAGGLE_ERR("Tried to create a data store task with the wrong task for the data. (task type = %s)\n", taskName[type]);
	}
}

DataStoreTask::DataStoreTask(DataStoreRepositoryQuery *q, TaskType _type) :
	HeapItem(), type(_type), priority(TASK_PRIORITY_LOW), num(totNum++), timestamp(Timeval::now()), RepositoryQuery(q), callback(NULL) 
{
	if (type == TASK_INSERT_REPOSITORY ||
		type == TASK_READ_REPOSITORY ||
		type == TASK_DELETE_REPOSITORY) {
	} else {
		HAGGLE_ERR("Tried to create a data store task with the wrong task for the data. (task type = %s)\n", taskName[type]);
	}
}

DataStoreTask::DataStoreTask(const Filter& _f, TaskType _type, const EventCallback<EventHandler> *_callback, bool _boolParameter) :
	HeapItem(), type(_type), priority(TASK_PRIORITY_HIGH), num(totNum++), timestamp(Timeval::now()), f(_f.copy()), callback(_callback), boolParameter(_boolParameter) 
{
	if (type == TASK_ADD_FILTER) {
	} else {
		HAGGLE_ERR("Tried to create a data store task with the wrong task for the data. (task type = %s)\n", taskName[type]);
	}
}

DataStoreTask::DataStoreTask(TaskType _type, void *_data, const EventCallback<EventHandler> *_callback) : 
	HeapItem(), type(_type), priority(TASK_PRIORITY_LOW), num(totNum++), timestamp(Timeval::now()), data(_data), callback(_callback), boolParameter(false) 
{
	if (type == TASK_EXIT) {
		priority = TASK_PRIORITY_LOW;
	} else if (
#ifdef DEBUG_DATASTORE
		type == TASK_DEBUG_PRINT ||
#endif
		type == TASK_DUMP_DATASTORE) {
			if (data != NULL) {
				HAGGLE_ERR("Tried to create a data store task with the wrong task for the data. (task type = %s)\n", taskName[type]);
			}
		
		priority = TASK_PRIORITY_HIGH;
	} else if (type == TASK_DUMP_DATASTORE_TO_FILE ||
		type == TASK_DELETE_FILTER) {
		priority = TASK_PRIORITY_LOW;
	} else {
		HAGGLE_ERR("Tried to create a data store task with the wrong task for the data. (task type = %s)\n", taskName[type]);
	}
}

DataStoreTask::DataStoreTask(const Timeval &_age, TaskType _type, const EventCallback<EventHandler> *callback) :
	HeapItem(), type(_type), priority(TASK_PRIORITY_LOW), num(totNum++), timestamp(Timeval::now()), age(new Timeval(_age)), callback(callback), boolParameter(false) 
{
	if (type == TASK_AGE_DATAOBJECTS) {
	} else {
		HAGGLE_ERR("Tried to create a data store task with the wrong task for the data. (task type = %s)\n", taskName[type]);
	}
}


bool DataStoreTask::compare_less(const HeapItem& i) const
{
	if (priority < static_cast<const DataStoreTask&>(i).priority)
		return true;
	else if (priority == static_cast<const DataStoreTask&>(i).priority && 
		 timestamp < static_cast<const DataStoreTask&>(i).timestamp)
		return true;
	
	return false;
}
bool DataStoreTask::compare_greater(const HeapItem& i) const
{
	if (priority > static_cast<const DataStoreTask&>(i).priority)
		return true;
	else if (priority == static_cast<const DataStoreTask&>(i).priority && 
		 timestamp > static_cast<const DataStoreTask&>(i).timestamp) 
		return true;
	
	return false;
}

DataStoreTask::~DataStoreTask()
{
	switch (type) {
	case TASK_INSERT_DATAOBJECT:
		// HAGGLE_DBG("Destroying data object reference, refcount=%d\n", dObj->refcount());
		delete dObj;
		break;
	case TASK_DELETE_DATAOBJECT:
		// HAGGLE_DBG("Destroying data object reference, refcount=%d\n", dObj->refcount());
		/*
			The boolParameter indicates whether we deleted by reference or by ID
		*/
		if (!boolParameter)
			delete dObj;
		break;
	case TASK_AGE_DATAOBJECTS:
		delete age;
		break;
	case TASK_INSERT_NODE:
		// HAGGLE_DBG("Destroying node reference, refcount=%d\n", node->refcount());
		delete node;
		break;
	case TASK_DELETE_NODE:
		// HAGGLE_DBG("Destroying node reference, refcount=%d\n", node->refcount());
		delete node;
		break;
	case TASK_RETRIEVE_NODE:
		// HAGGLE_DBG("Destroying node reference, refcount=%d\n", node->refcount());
		delete node;
		break;
	case TASK_RETRIEVE_NODE_BY_TYPE:
		break;
	case TASK_RETRIEVE_NODE_BY_INTERFACE:
		delete iface;
		break;
	case TASK_ADD_FILTER:
		// HAGGLE_DBG("Destroying filter\n");
		delete f;
		break;
	case TASK_DELETE_FILTER:
		// HAGGLE_DBG("Destroying filter type (long)\n");
		delete static_cast<long *>(data);
		break;
	case TASK_FILTER_QUERY:
		// HAGGLE_DBG("Destroying filter query\n");
		delete query;
		break;
	case TASK_DATAOBJECT_QUERY:
		// HAGGLE_DBG("Destroying data object query\n");
		delete DOQuery;
		break;
	case TASK_DATAOBJECT_FOR_NODES_QUERY:
		delete DOForNodesQuery;
		break;
	case TASK_NODE_QUERY:
		// HAGGLE_DBG("Destroying node query\n");
		delete NodeQuery;
		break;
	case TASK_INSERT_REPOSITORY:
	case TASK_READ_REPOSITORY:
	case TASK_DELETE_REPOSITORY:
		// HAGGLE_DBG("Destroying repository query\n");
		delete RepositoryQuery;
		break;
#ifdef DEBUG_DATASTORE
	case TASK_DEBUG_PRINT:
		delete static_cast<long *>(data);
		break;
#endif
	case TASK_DUMP_DATASTORE:
		break;
	case TASK_DUMP_DATASTORE_TO_FILE:
		delete static_cast<string *>(data);
		break;
	default:
		// HAGGLE_DBG("Unknown task type (%d) in Task Queue!\n", type);
		break;
	}
}

DataStore::~DataStore()
{
	HAGGLE_DBG("Destroying task queue containing %lu tasks\n", taskQ.size());

	while (!taskQ.empty()) {
		DataStoreTask *task = static_cast<DataStoreTask *>(taskQ.front());

		taskQ.pop_front();
		
		delete task;
	}
}

void DataStore::insertNode(NodeRef& node, const EventCallback<EventHandler> *callback, bool mergeBloomfilters)
{
	Mutex::AutoLocker l(mutex);

	taskQ.insert(new DataStoreTask(node, TASK_INSERT_NODE, callback, mergeBloomfilters));

	cond.signal();
}

void DataStore::deleteNode(NodeRef& node)
{
	Mutex::AutoLocker l(mutex);
	
	taskQ.insert(new DataStoreTask(node, TASK_DELETE_NODE));
	
	cond.signal();
}

void DataStore::retrieveNode(const NodeRef& node, const EventCallback<EventHandler> *callback, bool forceCallback) 
{
	Mutex::AutoLocker l(mutex);
	
	taskQ.insert(new DataStoreTask(node, TASK_RETRIEVE_NODE, callback, forceCallback));
	
	cond.signal();
}

void DataStore::retrieveNode(NodeType_t type, const EventCallback<EventHandler> *callback)
{
	Mutex::AutoLocker l(mutex);
	
	taskQ.insert(new DataStoreTask(type, TASK_RETRIEVE_NODE_BY_TYPE, callback));
	
	cond.signal();
}

void DataStore::retrieveNode(const InterfaceRef& iface, const EventCallback<EventHandler> *callback, bool forceCallback)
{
	Mutex::AutoLocker l(mutex);
	
	taskQ.insert(new DataStoreTask(iface, TASK_RETRIEVE_NODE_BY_INTERFACE, callback, forceCallback));
	
	cond.signal();
}

void DataStore::insertDataObject(DataObjectRef& dObj, const EventCallback<EventHandler> *callback)
{
	Mutex::AutoLocker l(mutex);

	taskQ.insert(new DataStoreTask(dObj, TASK_INSERT_DATAOBJECT, callback));

	cond.signal();
}

void DataStore::deleteDataObject(const DataObjectId_t id)
{
	Mutex::AutoLocker l(mutex);
	
	taskQ.insert(new DataStoreTask(id, TASK_DELETE_DATAOBJECT));
	
	cond.signal();
}

void DataStore::deleteDataObject(DataObjectRef& dObj)
{
	Mutex::AutoLocker l(mutex);
	
	taskQ.insert(new DataStoreTask(dObj, TASK_DELETE_DATAOBJECT));
	
	cond.signal();
}

void DataStore::ageDataObjects(const Timeval& minimumAge, const EventCallback<EventHandler> *callback)
{
	Mutex::AutoLocker l(mutex);
	
	taskQ.insert(new DataStoreTask(minimumAge, TASK_AGE_DATAOBJECTS, callback));
	
	cond.signal();
}

void DataStore::insertFilter(const Filter& f, bool matchFilter, const EventCallback<EventHandler> *callback)
{
	Mutex::AutoLocker l(mutex);

	taskQ.insert(new DataStoreTask(f, TASK_ADD_FILTER, callback, matchFilter));

	cond.signal();
}


void DataStore::deleteFilter(long eventtype)
{
	Mutex::AutoLocker l(mutex);

	taskQ.insert(new DataStoreTask(TASK_DELETE_FILTER, new long(eventtype)));

	cond.signal();
}

/* NOTE: The filter will be deleted, but not the callback. */
void DataStore::doFilterQuery(const Filter *f, EventCallback < EventHandler > *callback)
{
	Mutex::AutoLocker l(mutex);

	taskQ.insert(new DataStoreTask(new DataStoreFilterQuery(f, callback), TASK_FILTER_QUERY));

	cond.signal();
}

void DataStore::doDataObjectQuery(NodeRef& n, const unsigned int match, EventCallback < EventHandler > *callback)
{
	Mutex::AutoLocker l(mutex);

	taskQ.insert(new DataStoreTask(new DataStoreDataObjectQuery(n, match, callback), TASK_DATAOBJECT_QUERY));

	cond.signal();
}

void DataStore::doDataObjectForNodesQuery(const NodeRef &n, const NodeRefList &ns, const unsigned int match, const EventCallback < EventHandler > *callback)
{
	Mutex::AutoLocker l(mutex);

	taskQ.insert(new DataStoreTask(new DataStoreDataObjectForNodesQuery(n, ns, match, callback), TASK_DATAOBJECT_FOR_NODES_QUERY));

	cond.signal();
}

/* It is not possible to do lookups in the datastore simultaneously
 with the datastore thread without getting "library routine called
 out of sequence" errors. Possible solutions:

 1) do not do lookups directly, only pass via insertqueue and let the
 datastore thread do all the work asynchronously

 2) stop the datastore thread during lookup or move the mutex.unlock
 in the run() function to the end of the function. However, this would
 effectively remove the point of a thread since it might lock the main
 thread during a lookup because the datastore thread is working.

 3) Open a separate handle to the datastore that we use for reading
 from the main thread. This will let sqlite take care of thread
 safeness, but might have the same drawbacks as 2.

/Erik

*/


void DataStore::doNodeQuery(DataObjectRef& d, const unsigned int maxResp, const unsigned int match, EventCallback < EventHandler > *callback)
{
	Mutex::AutoLocker l(mutex);
	
	taskQ.insert(new DataStoreTask(new DataStoreNodeQuery(d, maxResp, match, callback), TASK_NODE_QUERY));
	
	cond.signal();
}
#ifdef DEBUG_DATASTORE
void DataStore::print() 
{
	Mutex::AutoLocker l(mutex);

	taskQ.insert(new DataStoreTask(TASK_DEBUG_PRINT));

	cond.signal();
}
#endif



/*
	Repository Functions
	for the specification of the uri, see DataStore.h
*/

void DataStore::insertRepository(RepositoryEntryRef re)
{
	Mutex::AutoLocker l(mutex);

	if (!re)
		return;
	
	taskQ.insert(new DataStoreTask(new DataStoreRepositoryQuery(re), TASK_INSERT_REPOSITORY));
	
	cond.signal();
}

void DataStore::readRepository(RepositoryEntryRef re, EventCallback < EventHandler > *callback)
{
	Mutex::AutoLocker l(mutex);
	
	if (!re)
		return;
	
	taskQ.insert(new DataStoreTask(new DataStoreRepositoryQuery(re, callback), TASK_READ_REPOSITORY));
	
	cond.signal();
}

void DataStore::deleteRepository(RepositoryEntryRef re)
{
	Mutex::AutoLocker l(mutex);
	
	if (!re)
		return;
	
	taskQ.insert(new DataStoreTask(new DataStoreRepositoryQuery(re), TASK_DELETE_REPOSITORY));
	
	cond.signal();
}

void DataStore::dump(const EventCallback<EventHandler> *callback)
{
        Mutex::AutoLocker l(mutex);
                
	taskQ.insert(new DataStoreTask(TASK_DUMP_DATASTORE, NULL, callback));
	
	cond.signal();
}

void DataStore::dumpToFile(const char *filename)
{
        Mutex::AutoLocker l(mutex);
                
	taskQ.insert(new DataStoreTask(TASK_DUMP_DATASTORE_TO_FILE, new string(filename)));
	
	cond.signal();
}

void DataStore::hookCancel()
{
	Mutex::AutoLocker l(mutex);
	
	taskQ.insert(new DataStoreTask(TASK_EXIT));

	cond.signal();
}

// This function is the thread
bool DataStore::run()
{
#if defined (DEBUG)
	static unsigned short count = 0;
#endif
	while (true) {
		mutex.lock();

		// Check if we should quit. 
                if (taskQ.empty()) {
			if (shouldExit()) {
				// yep.
				mutex.unlock();
				// Done:
				HAGGLE_DBG("DataStore exits due to exit condition and empty queue\n");
				return false;
			}
			HAGGLE_DBG("Waiting for task\n");
			cond.wait(&mutex);
		}
                
		DataStoreTask *task = static_cast<DataStoreTask *>(taskQ.front());

		taskQ.pop_front();
#if defined(DEBUG)
		// Log the queue length every tenth time we
		// execute a task
		if (++count > 10) {
			count = 0;
			LOG_ADD("%s: DataStore task queue length=%lu\n", Timeval::now().getAsString().c_str(), taskQ.size());
		}
#endif
		mutex.unlock();

		//HAGGLE_DBG("Executing task with priority=%u timestamp=%s\n", 
		//	   task->getPriority(), task->getTimestamp().getAsString().c_str());
		
		switch (task->getType()) {
		case TASK_INSERT_DATAOBJECT:
			_insertDataObject(*task->dObj, task->callback);
			break;
		case TASK_DELETE_DATAOBJECT:
			if (task->boolParameter)
				_deleteDataObject(task->id);
			else
				_deleteDataObject(*task->dObj);
			break;
		case TASK_AGE_DATAOBJECTS:
			_ageDataObjects(*task->age, task->callback);
			break;
		case TASK_INSERT_NODE:
			_insertNode(*task->node, task->callback, task->boolParameter);
			break;
		case TASK_DELETE_NODE:
			_deleteNode(*task->node);
			break;
		case TASK_RETRIEVE_NODE:
			_retrieveNode(*task->node, task->callback, task->boolParameter);
			break;
		case TASK_RETRIEVE_NODE_BY_TYPE:
			_retrieveNode(task->nodeType, task->callback);
			break;
		case TASK_RETRIEVE_NODE_BY_INTERFACE:
			_retrieveNode(*task->iface, task->callback, task->boolParameter);
			break;
		case TASK_ADD_FILTER:
			_insertFilter(task->f, task->boolParameter, task->callback);
			break;
		case TASK_DELETE_FILTER:
			_deleteFilter(*static_cast<long *>(task->data));
			break;
		case TASK_FILTER_QUERY:
			_doFilterQuery(task->query);
			break;
		case TASK_DATAOBJECT_QUERY:
			_doDataObjectQuery(task->DOQuery);
			break;
		case TASK_DATAOBJECT_FOR_NODES_QUERY:
			_doDataObjectForNodesQuery(task->DOForNodesQuery);
			break;
		case TASK_NODE_QUERY:
			_doNodeQuery(task->NodeQuery);
			break;
		case TASK_INSERT_REPOSITORY:
			_insertRepository(task->RepositoryQuery);
			break;
		case TASK_READ_REPOSITORY:
			_readRepository(task->RepositoryQuery);
			break;
		case TASK_DELETE_REPOSITORY:
			_deleteRepository(task->RepositoryQuery);
			break;
                case TASK_DUMP_DATASTORE:
			_dump(task->callback);
			break;
                case TASK_DUMP_DATASTORE_TO_FILE:
			_dumpToFile(static_cast<string *>(task->data)->c_str());
			break;
#ifdef DEBUG_DATASTORE
		case TASK_DEBUG_PRINT:
			HAGGLE_DBG("Printing data store\n");
			_print();
			HAGGLE_DBG("Done printing data store\n");
			break;
#endif
		case TASK_EXIT:
			// Do not execute anymore tasks after this one.
			HAGGLE_DBG("DataStore exit task\n");
			/* 
			 delete task;
			 return false;
			 */
			break;
		default:
			HAGGLE_DBG("Undefined data store task\n");
			break;
		}
		delete task;
	}
	HAGGLE_DBG("DataStore exits\n");
	return false;
}

void DataStore::cleanup()
{
	HAGGLE_DBG("DataStore thread cleanup\n");
}
