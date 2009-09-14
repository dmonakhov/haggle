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
	HAGGLE_DBG("Destroying task queue containing %d tasks\n", taskQ.size());

	while (!taskQ.empty()) {
		DataStoreTask *task = taskQ.front();

		taskQ.pop_front();
		
		delete task;
	}
}

int DataStore::insertNode(NodeRef& node, const EventCallback<EventHandler> *callback)
{
	Mutex::AutoLocker l(mutex);

	taskQ.push_back(new DataStoreTask(node, TASK_INSERT_NODE, callback));

	cond.signal();

	return 0;
}

int DataStore::deleteNode(NodeRef& node)
{
	Mutex::AutoLocker l(mutex);
	
	taskQ.push_back(new DataStoreTask(node, TASK_DELETE_NODE));
	
	cond.signal();
	
	return 0;
}

int DataStore::retrieveNode(NodeRef& node, const EventCallback<EventHandler> *callback, bool forceCallback) 
{
	Mutex::AutoLocker l(mutex);
	
	taskQ.push_back(new DataStoreTask(node, TASK_RETRIEVE_NODE, callback, forceCallback));
	
	cond.signal();
	
	return 0;
}

int DataStore::retrieveNodeByType(NodeType_t type, const EventCallback<EventHandler> *callback)
{
	Mutex::AutoLocker l(mutex);
	
	taskQ.push_back(new DataStoreTask(type, TASK_RETRIEVE_NODE_BY_TYPE, callback));
	
	cond.signal();
	
	return 0;
}

int DataStore::insertDataObject(DataObjectRef& dObj, const EventCallback<EventHandler> *callback)
{
	Mutex::AutoLocker l(mutex);

	taskQ.push_back(new DataStoreTask(dObj, TASK_INSERT_DATAOBJECT, callback));

	cond.signal();

	return 0;
}

int DataStore::deleteDataObject(const DataObjectId_t id)
{
	Mutex::AutoLocker l(mutex);
	
	taskQ.push_back(new DataStoreTask(id, TASK_DELETE_DATAOBJECT));
	
	cond.signal();
	
	return 0;
}

int DataStore::deleteDataObject(DataObjectRef& dObj)
{
	Mutex::AutoLocker l(mutex);
	
	taskQ.push_back(new DataStoreTask(dObj, TASK_DELETE_DATAOBJECT));
	
	cond.signal();
	
	return 0;
}

int DataStore::ageDataObjects(Timeval minimumAge)
{
	Mutex::AutoLocker l(mutex);
	
	taskQ.push_back(new DataStoreTask(minimumAge, TASK_AGE_DATAOBJECTS));
	
	cond.signal();
	
	return 0;
}

int DataStore::insertFilter(const Filter& f, bool matchFilter, const EventCallback<EventHandler> *callback)
{
	Mutex::AutoLocker l(mutex);

	taskQ.push_back(new DataStoreTask(f, TASK_ADD_FILTER, callback, matchFilter));

	cond.signal();

	return 0;
}


int DataStore::deleteFilter(long eventtype)
{
	Mutex::AutoLocker l(mutex);

	taskQ.push_back(new DataStoreTask(TASK_DELETE_FILTER, new long(eventtype)));

	cond.signal();

	return 0;
}

/* NOTE: The filter will be deleted, but not the callback. */
int DataStore::doFilterQuery(const Filter *f, EventCallback < EventHandler > *callback)
{
	Mutex::AutoLocker l(mutex);

	taskQ.push_back(new DataStoreTask(new DataStoreFilterQuery(f, callback), TASK_FILTER_QUERY));

	cond.signal();

	return 0;
}

int DataStore::doDataObjectQuery(NodeRef& n, const unsigned int match, EventCallback < EventHandler > *callback)
{
	Mutex::AutoLocker l(mutex);

	taskQ.push_back(new DataStoreTask(new DataStoreDataObjectQuery(n, match, callback), TASK_DATAOBJECT_QUERY));

	cond.signal();

	return 0;
}

int DataStore::doDataObjectForNodesQuery(const NodeRef &n, const NodeRefList &ns, const unsigned int match, const EventCallback < EventHandler > *callback)
{
	Mutex::AutoLocker l(mutex);

	taskQ.push_back(new DataStoreTask(new DataStoreDataObjectForNodesQuery(n, ns, match, callback), TASK_DATAOBJECT_FOR_NODES_QUERY));

	cond.signal();

	return 0;
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


int DataStore::doNodeQuery(DataObjectRef& d, const unsigned int max, const unsigned int match, const unsigned int ratio, EventCallback < EventHandler > *callback)
{
	Mutex::AutoLocker l(mutex);
	
	taskQ.push_back(new DataStoreTask(new DataStoreNodeQuery(d, max, match, ratio, callback), TASK_NODE_QUERY));
	
	cond.signal();

	return 0;
}
#ifdef DEBUG_DATASTORE
void DataStore::print() 
{
	Mutex::AutoLocker l(mutex);

	taskQ.push_back(new DataStoreTask(TASK_DEBUG_PRINT));

	cond.signal();
}
#endif



/*
	Repository Functions
	for the specification of the uri, see DataStore.h
*/

int DataStore::insertRepository(RepositoryEntryRef re)
{
	Mutex::AutoLocker l(mutex);

	if (!re)
		return -1;
	
	taskQ.push_back(new DataStoreTask(new DataStoreRepositoryQuery(re), TASK_INSERT_REPOSITORY));
	
	cond.signal();
	
	return 0;
}

int DataStore::readRepository(RepositoryEntryRef re, EventCallback < EventHandler > *callback)
{
	Mutex::AutoLocker l(mutex);
	
	if (!re)
		return -1;
	
	taskQ.push_back(new DataStoreTask(new DataStoreRepositoryQuery(re, callback), TASK_READ_REPOSITORY));
	
	cond.signal();
		
	return 0;
}

int DataStore::deleteRepository(RepositoryEntryRef re)
{
	Mutex::AutoLocker l(mutex);
	
	if (!re)
		return -1;
	
	taskQ.push_back(new DataStoreTask(new DataStoreRepositoryQuery(re), TASK_DELETE_REPOSITORY));
	
	cond.signal();
	
	return 0;
}

int DataStore::dump(const EventCallback<EventHandler> *callback)
{
        Mutex::AutoLocker l(mutex);
                
	taskQ.push_back(new DataStoreTask(TASK_DUMP_DATASTORE, NULL, callback));
	
	cond.signal();
	
	return 0;
}

int DataStore::dumpToFile(const char *filename)
{
        Mutex::AutoLocker l(mutex);
                
	taskQ.push_back(new DataStoreTask(TASK_DUMP_DATASTORE_TO_FILE, new string(filename)));
	
	cond.signal();
	
	return 0;
}

void DataStore::hookCancel()
{
	Mutex::AutoLocker l(mutex);
	
	taskQ.push_back(new DataStoreTask(TASK_EXIT));

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

		// Check if we should quit. Don't quit until all tasks have been 
		// completed.
                if (taskQ.empty()) {
			if (shouldExit()) {
				// yep.
				mutex.unlock();
				// Done:
				return false;
			}
			cond.wait(&mutex);
		}
                
		DataStoreTask *task = taskQ.front();

		taskQ.pop_front();
#if defined(DEBUG)
		// Log the queue length every tenth time we
		// execute a task
		if (++count > 10) {
			count = 0;
			LOG_ADD("DataStore task queue length=%lu\n", taskQ.size());
		}
#endif
		mutex.unlock();

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
			_ageDataObjects(*task->age);
			break;
		case TASK_INSERT_NODE:
			_insertNode(*task->node, task->callback);
			break;
		case TASK_DELETE_NODE:
			_deleteNode(*task->node);
			break;
		case TASK_RETRIEVE_NODE:
			_retrieveNode(*task->node, task->callback, task->boolParameter);
			break;
		case TASK_RETRIEVE_NODE_BY_TYPE:
			_retrieveNodeByType(task->nodeType, task->callback);
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
			break;
		default:
			HAGGLE_DBG("Undefined data store task\n");
			break;
		}
		delete task;
	}
	return false;
}

void DataStore::cleanup()
{
	HAGGLE_DBG("DataStore thread cleanup\n");
}
