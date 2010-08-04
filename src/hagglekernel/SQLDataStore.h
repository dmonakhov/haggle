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
#ifndef _SQLDATASTORE_H
#define _SQLDATASTORE_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/
class SQLDataStore;

#include <libcpphaggle/Platform.h>
#include "DataStore.h"
#include "Node.h"

#define DEFAULT_DATASTORE_FILENAME "haggle.db"
#define INMEMORY_DATASTORE_FILENAME ":memory:"
#define DEFAULT_DATASTORE_FILEPATH DEFAULT_DATASTORE_PATH

#include <libxml/parser.h>
#include <libxml/tree.h> // For dumping to XML

#include <sqlite3.h>
#include "DataObject.h"
#include "Metadata.h"

#ifdef DEBUG_DATASTORE
#define DEBUG_SQLDATASTORE
#endif

/** */
class SQLDataStore : public DataStore
{
private:
	sqlite3 *db; 
	bool isInMemory;
	bool recreate;
	string filepath;

	int cleanupDataStore();
	int createTables();
	int sqlQuery(const char *sql_cmd);

	int setViewLimitedDataobjectAttributes(sqlite_int64 dataobject_rowid = 0);
	int setViewLimitedNodeAttributes(sqlite_int64 dataobject_rowid = 0);
	int evaluateDataObjects(long eventType);
	int evaluateFilters(const DataObjectRef& dObj, sqlite_int64 dataobject_rowid = 0);

	sqlite_int64 getDataObjectRowId(const DataObjectId_t& id);
	sqlite_int64 getAttributeRowId(const Attribute* attr);
	sqlite_int64 getNodeRowId(const NodeRef& node);
	sqlite_int64 getNodeRowId(const InterfaceRef& iface);
	sqlite_int64 getInterfaceRowId(const InterfaceRef& iface);

	DataObject *createDataObject(sqlite3_stmt *stmt);
	NodeRef createNode(sqlite3_stmt *in_stmt);

	Attribute *getAttrFromRowId(const sqlite_int64 attr_rowid, const sqlite_int64 node_rowid);
	DataObject *getDataObjectFromRowId(const sqlite_int64 dataObjectRowId);
	NodeRef getNodeFromRowId(const sqlite_int64 nodeRowId);
	Interface *getInterfaceFromRowId(const sqlite_int64 ifaceRowId);
	
	int findAndAddDataObjectTargets(DataObjectRef& dObj, const sqlite_int64 dataObjectRowId, const long ratio);
	int deleteDataObjectNodeDescriptions(DataObjectRef ref_dObj, string *node_id);

	int backupDatabase(sqlite3 *pInMemory, const char *zFilename, int toFile = 1);
	string getFilepath();
		
	
#ifdef DEBUG_SQLDATASTORE
	void _print();
#endif
        xmlDocPtr dumpToXML();
protected:
//	Node *createNode(sqlite3 *db, sqlite3_stmt * in_stmt);
//	Node *getNodeFromRowId(sqlite3 * db, const sqlite_int64 nodeRowId);

	// These functions work through the task Queue
	// - insert implements update functionality
	int _insertNode(NodeRef& node, const EventCallback<EventHandler> *callback = NULL, bool mergeBloomfilter = false);
	int _deleteNode(NodeRef& node);
	int _retrieveNode(NodeRef& node, const EventCallback<EventHandler> *callback, bool forceCallback);
	int _retrieveNode(NodeType_t type, const EventCallback<EventHandler> *callback);
	int _retrieveNode(const InterfaceRef& iface, const EventCallback<EventHandler> *callback, bool forceCallback);
	int _insertDataObject(DataObjectRef& dObj, const EventCallback<EventHandler> *callback = NULL);
	int _deleteDataObject(const DataObjectId_t &id, bool shouldReportRemoval = true);
	int _deleteDataObject(DataObjectRef& dObj, bool shouldReportRemoval = true);
	int _ageDataObjects(const Timeval& minimumAge, const EventCallback<EventHandler> *callback = NULL);
	int _insertFilter(Filter *f, bool matchFilter = false, const EventCallback<EventHandler> *callback = NULL);
	int _deleteFilter(long eventtype);
	// matching Filters
	int _doFilterQuery(DataStoreFilterQuery *q);
	// matching Dataobject > Nodes
	/**
		Returns: The number of data objects filled in.
	*/
	int _doDataObjectQueryStep2(NodeRef &node, NodeRef alsoThisBF, DataStoreQueryResult *qr, int max_matches, unsigned int ratio, unsigned int attrMatch);
	int _doDataObjectQuery(DataStoreDataObjectQuery *q);
	int _doDataObjectForNodesQuery(DataStoreDataObjectForNodesQuery *q);
	// matching Node > Dataobjects
	int _doNodeQuery(DataStoreNodeQuery *q);
	int _insertRepository(DataStoreRepositoryQuery *q);
	int _readRepository(DataStoreRepositoryQuery *q, const EventCallback<EventHandler> *callback = NULL);
	int _deleteRepository(DataStoreRepositoryQuery *q);
	
	int _dump(const EventCallback<EventHandler> *callback = NULL);
	int _dumpToFile(const char *filename);
	int _onConfig();

public:
	SQLDataStore(const bool recreate = false, const string = DEFAULT_DATASTORE_FILEPATH, const string name = "SQLDataStore");
	~SQLDataStore();

	bool init();
};

#endif /* _SQLDATASTORE_H */
