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
#ifndef _APPLICATIONMANAGER_H
#define _APPLICATIONMANAGER_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/
class ApplicationHandle;
class ApplicationManager;

#include <libcpphaggle/Map.h>

#include "Manager.h"
#include "Event.h"
#include "DataObject.h"
#include "Node.h"

/*
	We import the control attributes from libhaggle in order to make sure
	we use the same values here. It is not a nice solution, as it makes haggle
	dependent on the presence of libhaggle. It should be considered or a 
	temoporary solution until the API is stabalized.
*/
#include "../libhaggle/include/libhaggle/ipc.h"

typedef List< Pair<NodeRef, DataObjectRef> > SentToApplicationList;

/** */
class ApplicationHandle
{
        bool event_types[_LIBHAGGLE_NUM_EVENTS];
        NodeRef app;
	// NOTE: Using the map here is not optimal as we do not
	// need the value. However, this works until we have a non-stl
	// version of set.
	typedef Map<EventType, char> eventTypeRegistry;
	typedef Pair<EventType, char> eventTypeRegistryPair;
        eventTypeRegistry eventTypes;
        int id;
public:
        const NodeRef getNode() const {
                return app;
        }
        const int getId() const {
                return id;
        }

        bool hasEventInterest(int eid) {
                return event_types[eid];
        }
        void addEventInterest(int eid) {
                event_types[eid] = true;
        }
        void addInterestAttribute(const string name, const string value) {
                app->addAttribute(name, value);
        }
        void addFilterEventType(const EventType etype) {
                eventTypes.insert(make_pair(etype,'\0'));
        }
        bool hasFilterEventType(const EventType etype) {
                return (eventTypes.find(etype) != eventTypes.end()) ? true : false;
        }
        const eventTypeRegistry *getEventTypes() const {
                return &eventTypes;
        }

        ApplicationHandle(NodeRef _app, int _id) : app(_app), id(_id) {
                for (int i = 0; i < _LIBHAGGLE_NUM_EVENTS; i++) event_types[i] = false;
        }
        ~ApplicationHandle() {}
};

/** */
class ApplicationManager : public Manager
{
	SentToApplicationList pendingDOs;
        unsigned long numClients;
	unsigned long sessionid;
	bool dataStoreFinishedProcessing;
	EventCallback<EventHandler> *onRetrieveNodeCallback;
	EventCallback<EventHandler> *onDataStoreFinishedProcessingCallback;
	EventCallback<EventHandler> *onRetrieveAppNodesCallback;
	EventType ipcFilterEvent;
	void onDataStoreFinishedProcessing(Event *e);
        int deRegisterApplication(NodeRef& app);
        void sendToApplication(DataObjectRef& dObj, NodeRef& app);
        int sendToAllApplications(DataObjectRef& dObj, long eid);
        int addApplicationEventInterest(NodeRef& app, long eid);
        int updateApplicationInterests(NodeRef& app);
        /* Event handler functions */
        void onSendResult(Event *e);
        void onReceiveFromApplication(Event *e);
        void onNeighborStatusChange(Event *e);
	void onRetrieveNode(Event *e);
	void onRetrieveAppNodes(Event *e);
        void onApplicationFilterMatchEvent(Event *e);
	void onPrepareShutdown();
	void onShutdown();
	void onStartup();
public:
        ApplicationManager(HaggleKernel *_kernel = haggleKernel);
        ~ApplicationManager();

class ApplicationException : public ManagerException
        {
        public:
                ApplicationException(const int err = 0, const char* data = "Data manager Error") : ManagerException(err, data) {}
        };
};


#endif /* _APPLICATIONMANAGER_H */
