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
#include <libcpphaggle/Platform.h>
#include <libcpphaggle/Watch.h>
#include <libcpphaggle/String.h>

#include "EventQueue.h"
#include "ApplicationManager.h"
#include "DataObject.h"
#include "Event.h"
#include "Interface.h"
#include "Attribute.h"
#include "Filter.h"
#include "Node.h"
#include "Utility.h"

#include <base64.h>

// This include is for the making sure we use the same attribute names as those in libhaggle. 
// TODO: remove dependency on libhaggle header
#include "../libhaggle/include/libhaggle/ipc.h"

using namespace haggle;

#define DEBUG_APPLICATION_API

static const char *intToStr(int n)
{
	static char intStr[5];

	sprintf(intStr, "%d", n);
	return intStr;
}

/*
  Define various criterias that the application manager uses to fetch
  nodes from the node store.

 */
class EventCriteria : public NodeStore::Criteria
{
	EventType etype;
public:
	EventCriteria(EventType _etype) : etype(_etype) {}
	virtual bool operator() (const NodeRef& n) const
	{
		return (n->getType() == NODE_TYPE_APPLICATION && 
			n->hasEventInterest(etype));
	}
};

class EventCriteria2 : public NodeStore::Criteria
{
	EventType etype1;
	EventType etype2;
public:
	EventCriteria2(EventType _etype1, EventType _etype2) : etype1(_etype1), etype2(_etype2) {}
	virtual bool operator() (const NodeRef& n1) const
	{
		return (n1->getType() == NODE_TYPE_APPLICATION && 
			n1->hasEventInterest(etype1) && 
			n1->hasEventInterest(etype2));
	}
};
class NeighborCriteria : public NodeStore::Criteria
{
	EventType etype1;
	EventType etype2;
public:
	NeighborCriteria(EventType _etype1, EventType _etype2) : etype1(_etype1), etype2(_etype2) {}
	virtual bool operator() (const NodeRef& n1) const
	{
		return (n1->getType() == NODE_TYPE_PEER && 
			n1->isAvailable() &&
			n1->hasEventInterest(etype1) && 
			n1->hasEventInterest(etype2));
	}
};

ApplicationManager::ApplicationManager(HaggleKernel * _kernel) :
	Manager("ApplicationManager", _kernel), numClients(0), sessionid(0),
	dataStoreFinishedProcessing(false)
{
#define __CLASS__ ApplicationManager
	int ret;

	ret = setEventHandler(EVENT_TYPE_NEIGHBOR_INTERFACE_DOWN, onNeighborStatusChange);

#if HAVE_EXCEPTION
	if (ret < 0)
		throw ApplicationException(ret, "Could not register event");
#endif
	ret = setEventHandler(EVENT_TYPE_NEIGHBOR_INTERFACE_UP, onNeighborStatusChange);

#if HAVE_EXCEPTION
	if (ret < 0)
		throw ApplicationException(ret, "Could not register event");
#endif
       
	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL, onSendResult);

#if HAVE_EXCEPTION
	if (ret < 0)
		throw ApplicationException(ret, "Could not register event");
#endif
	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_SEND_FAILURE, onSendResult);

#if HAVE_EXCEPTION
	if (ret < 0)
		throw ApplicationException(ret, "Could not register event");
#endif
       
	ret = setEventHandler(EVENT_TYPE_NODE_CONTACT_NEW, onNeighborStatusChange);

#if HAVE_EXCEPTION
	if (ret < 0)
		throw ApplicationException(ret, "Could not register event");
#endif

	ret = setEventHandler(EVENT_TYPE_NODE_UPDATED, onNeighborStatusChange);

#if HAVE_EXCEPTION
	if (ret < 0)
		throw ApplicationException(ret, "Could not register event");
#endif
        
	ret = setEventHandler(EVENT_TYPE_NODE_CONTACT_END, onNeighborStatusChange);

#if HAVE_EXCEPTION
	if (ret < 0)
		throw ApplicationException(ret, "Could not register event");
#endif
        
	onRetrieveNodeCallback = newEventCallback(onRetrieveNode);
	onDataStoreFinishedProcessingCallback = newEventCallback(onDataStoreFinishedProcessing);
	onRetrieveAppNodesCallback = newEventCallback(onRetrieveAppNodes);
	
	kernel->getDataStore()->retrieveNodeByType(NODE_TYPE_APPLICATION, onRetrieveAppNodesCallback);
	
        /* 
         * Register a filter that makes sure we receive all data
         * objects from applications that contain control information.
         */
	registerEventTypeForFilter(ipcFilterEvent, "Application API filter", onReceiveFromApplication, "HaggleIPC=*");
}

ApplicationManager::~ApplicationManager()
{
	if (onRetrieveNodeCallback)
		delete onRetrieveNodeCallback;
	
	if (onDataStoreFinishedProcessingCallback)
		delete onDataStoreFinishedProcessingCallback;
	
	if (onRetrieveAppNodesCallback)
		delete onRetrieveAppNodesCallback;
}

void ApplicationManager::onStartup()
{
	/*
		This function is called when haggle has finished starting up. Here
		we wish to tell any application waiting for haggle to start up that
		we've finished. We do this by sending a data object to a specific UDP
		port. Any application that is waiting for haggle to start will be 
		listening on that port.
	*/
	
	// We need a fake application node, for the protocol to be selected 
	// properly
	NodeRef fakeAppNode = new Node(NODE_TYPE_APPLICATION, "Startup application node");
	
	// Set up the address to the application:
	Address addr("udp://127.0.0.1:8788");
	// Reuse whatever is written in the URL above, to minimize the number of 
	// hardcoded values:
	short port = addr.getProtocolPortOrChannel();
	// Create an interface for the address:
	InterfaceRef iface = new Interface(IFTYPE_APPLICATION_PORT, &port, &addr);
	// Mark the interface as up:
	iface->up();
	
	// Add the interface to the node:
	fakeAppNode->addInterface(iface);
	
	// Create data object:
	DataObjectRef fakeDO = new DataObject(NULL, 0);
	
	fakeDO->addAttribute(HAGGLE_ATTR_CONTROL_NAME);
	
	// Send data object:
	kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, fakeDO, fakeAppNode));
}

// Updates the "this node" with all attributes that the application nodes have: 
void ApplicationManager::onRetrieveAppNodes(Event *e)
{
	if (!e)
		return;
	
	NodeRefList *nodes = (NodeRefList *) e->getData();
	
	/**
		FIXME: This function is currently not capable of determining if there
		is a difference in the interests of thisNode before and after, and 
		therefore assume a difference, and sends out the node description.
		
		This is perhaps unwanted behavior.
	*/
	
	// Remove all the old attributes:
	Attributes *al = kernel->getThisNode()->getAttributes()->copy();

	for (Attributes::iterator jt = al->begin(); jt != al->end(); jt++) {
		kernel->getThisNode()->removeAttribute((*jt).second);
	}

	delete al;
	
	if (nodes && !nodes->empty()) {
		// Insert all the attributes:
		for (NodeRefList::iterator it = nodes->begin(); it != nodes->end(); it++) {
			const Attributes *al;
			NodeRef	nr;
			
			// Try to get the most updated node from the node store:
			nr = kernel->getNodeStore()->retrieve(*it);
			// No node in the store? Default to the node in the data store:
			if (!nr)
				nr = (*it);
			
			nr.lock();
			al = nr->getAttributes();
			for (Attributes::const_iterator jt = al->begin(); jt != al->end(); jt++) {
				kernel->getThisNode()->addAttribute((*jt).second);
			}
			nr.unlock();
		}
	}
	
	// The node list will not be deleted along with the event, so we have to do it:
	if (nodes)
		delete nodes;
		
	// Push the updated node description to all neighbors
	if (kernel->getNodeStore()->numNeighbors())
		kernel->addEvent(new Event(EVENT_TYPE_NODE_DESCRIPTION_SEND));
}

void ApplicationManager::onDataStoreFinishedProcessing(Event *e)
{
	unregisterWithKernel();
}

void ApplicationManager::onSendResult(Event *e)
{
	// This function responds to both EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL and
	// EVENT_TYPE_DATAOBJECT_SEND_FAILURE, but doesn't distinguish between them
	// at all.

	// Check that the event exists (just in case)
	if (e == NULL)
		return;
	
	NodeRef app = e->getNode();
	
	if (!app || app->getType() != NODE_TYPE_APPLICATION)
		return;
	
	DataObjectRef dObj = e->getDataObject();
	
	// Check that the data object exists:
	if (!dObj)
		return;
	
	// Go through the list and find which (if any) sends this was in reference 
	// to
        SentToApplicationList::iterator it = pendingDOs.begin();

	while (it != pendingDOs.end()) {
		// Only check against the data object here in case some application
		// has deregistered. FIXME: should probably remove
		// all data objects belonging to a specific application
		// when the application deregisters.
                if ((*it).second == dObj) {
                        it = pendingDOs.erase(it);
                } else {
                        it++;
                }
	}
	// If we didn't find any matches, ignore.
	
	// If we are preparing for shutdown and the data store
	// is done with processing deregistered application nodes,
	// then signal we are ready for shutdown
	if (getState() == MANAGER_STATE_PREPARE_SHUTDOWN) {
		if (pendingDOs.empty())
			signalIsReadyForShutdown();
		else {
			HAGGLE_DBG("preparing shutdown, but %d data objects are still pending\n", pendingDOs.size());
		}
	}
}

void ApplicationManager::sendToApplication(DataObjectRef& dObj, NodeRef& app)
{
	pendingDOs.push_back(make_pair(app, dObj));
	kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, dObj, app));
}

void ApplicationManager::onPrepareShutdown()
{	
	HAGGLE_DBG("Shutdown! Notifying applications\n");

	DataObjectRef dObj = DataObjectRef(new DataObject(NULL, 0), "DataObjectShutdown");
	
	if (!dObj)
		return;
	
	// Tell all applications that we are shutting down.
	Metadata *ctrl_m = addControlMetadata(CTRL_TYPE_EVENT, "All Applications", dObj->getMetadata());
	
	if (!ctrl_m)
		return;
		
	Metadata *event_m = ctrl_m->addMetadata(DATAOBJECT_METADATA_APPLICATION_CONTROL_EVENT);
	
	if (!event_m)
		return;
	
	event_m->setParameter(DATAOBJECT_METADATA_APPLICATION_CONTROL_EVENT_TYPE_PARAM, intToStr(LIBHAGGLE_EVENT_SHUTDOWN));
	
	sendToAllApplications(dObj, LIBHAGGLE_EVENT_SHUTDOWN);
	
	// Signal we are ready for shutdown here, or defer until
	// all pending data objects have been sent....

	if (pendingDOs.empty())
		signalIsReadyForShutdown();
}

void ApplicationManager::onShutdown()
{
	// Retrieve all application nodes from the Node store.
	NodeRefList lst;
	unsigned long num;
	
        num = kernel->getNodeStore()->retrieve(NODE_TYPE_APPLICATION, lst);
	
	if (num) {
		for (NodeRefList::iterator it = lst.begin(); it != lst.end(); it++) {
			NodeRef i = (NodeRef)*it;
			deRegisterApplication(i);
		}
		
		/*
		 The point here is to delay deregistering until after the data store 
		 has finished processing what deRegisterApplication sent it, not to
		 get the actual data.
		 */
		kernel->getDataStore()->retrieveNode(*(lst.begin()), onDataStoreFinishedProcessingCallback, true);
		
	} else {
		unregisterWithKernel();
	}
	
	unregisterEventTypeForFilter(ipcFilterEvent);	
}

int ApplicationManager::deRegisterApplication(NodeRef& app)
{
	HAGGLE_DBG("Removing Application node %s id=%s\n", app->getName().c_str(), app->getIdStr());
	
	numClients--;

	// Remove the application node
	kernel->getNodeStore()->remove(app);

	// Remove the application's filter
	kernel->getDataStore()->deleteFilter(app->getFilterEvent());
	
	// Remove the node's interface (we don't want to save an old port no. in the data store):
	app.lock();

        const InterfaceRefList *lst = app->getInterfaces();
        
        while (!lst->empty()) {
                app->removeInterface(*(lst->begin()));
        }

	app.unlock();
	
	// Go through the send list and find which (if any) sends were in reference 
	// to this application
        SentToApplicationList::iterator it = pendingDOs.begin();
	
	while (it != pendingDOs.end()) {
                if ((*it).first == app) {
                        it = pendingDOs.erase(it);
                } else {
                        it++;
                }
	}
	// Save the application node state
	kernel->getDataStore()->insertNode(app);

	return 1;
}

static EventType translate_event(int eid)
{
	switch (eid) {
		case LIBHAGGLE_EVENT_SHUTDOWN:
			return EVENT_TYPE_SHUTDOWN;
		case LIBHAGGLE_EVENT_NEIGHBOR_UPDATE:
			return EVENT_TYPE_NODE_CONTACT_NEW;
		case LIBHAGGLE_EVENT_NEW_DATAOBJECT:
			return EVENT_TYPE_DATAOBJECT_RECEIVED;
	}
	return -1;
}

int ApplicationManager::addApplicationEventInterest(NodeRef& app, long eid)
{
	HAGGLE_DBG("Application %s registered event interest %d\n", app->getName().c_str(), eid);

	switch (eid) {
		case LIBHAGGLE_EVENT_SHUTDOWN:
			app->addEventInterest(EVENT_TYPE_SHUTDOWN);
			break;
		case LIBHAGGLE_EVENT_NEIGHBOR_UPDATE:
			app->addEventInterest(EVENT_TYPE_NODE_CONTACT_NEW);
			app->addEventInterest(EVENT_TYPE_NODE_CONTACT_END);
			onNeighborStatusChange(NULL);
			break;
	}
	return 0;
}

int ApplicationManager::sendToAllApplications(DataObjectRef& dObj, long eid)
{
	int numSent = 0;
	EventCriteria ec(translate_event(eid));

	NodeRefList apps;
        unsigned long num;

        num = kernel->getNodeStore()->retrieve(ec, apps);

	if (num == 0) {
		HAGGLE_ERR("No applications to send to for event id=%ld\n", eid);
		return 0;
	}
	for (NodeRefList::iterator it = apps.begin(); it != apps.end(); it++) {
		NodeRef& app = *it;

		DataObjectRef sendDO = DataObjectRef(dObj->copy(), "DataObject[App=" + app->getName() + "]");

#ifdef DEBUG_APPLICATION_API
                       sendDO->print();
#endif
		sendToApplication(sendDO, app);
		numSent++;
		HAGGLE_DBG("Sent event id=%ld to application %s\n", eid, app->getName().c_str());
	}

	return numSent;
}

int ApplicationManager::updateApplicationInterests(NodeRef& app)
{
	if (!app)
		return -1;

	long etype = app->getFilterEvent();

	if (etype == -1) {
		etype = registerEventType("Application filter event", onApplicationFilterMatchEvent);
		app->setFilterEvent(etype);
	}

	app->addEventInterest(etype);

	// TODO: This should be kept in a list so that it can be
	// removed when the application deregisters.
	Filter appfilter(app->getAttributes(), etype);

	// Insert the filter, and also match it immediately:
	kernel->getDataStore()->insertFilter(appfilter, true);

	HAGGLE_DBG("Registered interests for application %s\n", app->getName().c_str());

	return 0;
}

void ApplicationManager::onApplicationFilterMatchEvent(Event *e)
{
	DataObjectRefList& dObjs = e->getDataObjectList();

	if (dObjs.size() == 0) {
		HAGGLE_ERR("No Data objects in filter match event!\n");
		return;
	}
	EventCriteria ec(e->getType());

	HAGGLE_DBG("Filter match event - checking applications\n");

	NodeRefList apps;
        unsigned long num = kernel->getNodeStore()->retrieve(ec, apps);

	if (num == 0) {
		HAGGLE_ERR("No applications matched filter\n");
		return;
	}
	for (NodeRefList::iterator it = apps.begin(); it != apps.end(); it++) {
		NodeRef& app = *it;

		HAGGLE_DBG("Application %s's filter matched %lu data objects\n", 
			app->getName().c_str(), dObjs.size());

		for (DataObjectRefList::iterator it = dObjs.begin(); it != dObjs.end(); it++) {
			DataObjectRef& dObj = *it;

			// Have we already sent this data object to this app?
			if (app->getBloomfilter()->has(dObj)) {
				// Yep. Don't resend.
				HAGGLE_DBG("Application %s already has data object. Not sending.\n", 
					app->getName().c_str());
			} else {
				// Nope. Add it to the bloomfilter, then send.
				app->getBloomfilter()->add(dObj);
				string dObjName = "DataObject[App:" + app->getName() + "]";

				DataObjectRef dObjSend(dObj->copy(), dObjName);
				
				Metadata *ctrl_m = addControlMetadata(CTRL_TYPE_EVENT, app->getName(), dObjSend->getMetadata());
				
				if (!ctrl_m)
					continue;
				
				Metadata *event_m = ctrl_m->addMetadata(DATAOBJECT_METADATA_APPLICATION_CONTROL_EVENT);
				
				if (!event_m)
					continue;
				
				event_m->setParameter(DATAOBJECT_METADATA_APPLICATION_CONTROL_EVENT_TYPE_PARAM, intToStr(LIBHAGGLE_EVENT_NEW_DATAOBJECT));
			
				dObjSend->setPersistent(false);

				/*
				Indicate that this data object is for a
				local application, which means the file path
				to the local file will be added to the
				metadata once the data object is transformed
				to wire format.
				*/
				dObjSend->setIsForLocalApp();
#ifdef DEBUG
				char *raw;
				size_t len;

				dObjSend->getRawMetadataAlloc(&raw, &len);

				if (raw) {
					printf("App - DataObject METADATA:\n%s\n", raw);
					free(raw);
				}
#endif
				sendToApplication(dObjSend, app);
			}
		}
	}
}

void ApplicationManager::onNeighborStatusChange(Event *e)
{
       	if (numClients == 0)
		return;
	
	HAGGLE_DBG("Contact update (new or end)! Notifying applications\n");
	
	DataObjectRef dObj = DataObjectRef(new DataObject(NULL, 0), "DataObjectOnNodeContactNewOrEnd");
	
	if (!dObj)
		return;

	Metadata *ctrl_m = addControlMetadata(CTRL_TYPE_EVENT, "All Applications", dObj->getMetadata());
	
	if (!ctrl_m)
		return;
	
	Metadata *event_m = ctrl_m->addMetadata(DATAOBJECT_METADATA_APPLICATION_CONTROL_EVENT);
	
	if (!event_m)
		return;
	
	event_m->setParameter(DATAOBJECT_METADATA_APPLICATION_CONTROL_EVENT_TYPE_PARAM, intToStr(LIBHAGGLE_EVENT_NEIGHBOR_UPDATE));

	NodeRefList neighList;
        unsigned long num = kernel->getNodeStore()->retrieveNeighbors(neighList);

	if (num) {
		HAGGLE_DBG("Neighbor list size is %d\n", neighList.size());

		int numNeigh = 0;

		for (NodeRefList::iterator it = neighList.begin(); it != neighList.end(); it++) {
			NodeRef neigh = *it;
		
                        if (e && e->getType() == EVENT_TYPE_NEIGHBOR_INTERFACE_DOWN) {
                                InterfaceRef neighIface = e->getInterface();

                                /*
                                  If this was an interface down event
                                  and it was the last interface that
                                  goes down we do nothing. The update
                                  will be handled in a 'node contact
                                  end' event instead.
                                 */
                                if (neighIface && neigh->hasInterface(neighIface) && 
                                    neigh->numActiveInterfaces() == 1)
                                        return;
                        }

			// Add node without bloomfilter to reduce the size of the data object
			Metadata *md = neigh->toMetadata(false);

			if (md) {
				// mark available interfaces
                                struct base64_decode_context b64_ctx;
                                size_t len;

				// find Node Metadata
				Metadata *mdNode = md;
				
                                /*
                                  Add extra information about which interfaces are marked up or down.
                                  This can be of interest to the application.
                                 */
				if (mdNode) {
					Metadata *mdIface = md->getMetadata("Interface");
					while (mdIface) {
                                                // interface type
						const char *typeString = mdIface->getParameter("type");
						InterfaceType_t ifaceType = Interface::strToType(typeString);
						// interface id
						const char *strBase64 = mdIface->getParameter("identifier");
						base64_decode_ctx_init(&b64_ctx);
						char* idString = NULL;
						base64_decode_alloc(&b64_ctx, strBase64, strlen(strBase64), &idString, &len);
						
                                                if (idString) {
							InterfaceRef iface = kernel->getInterfaceStore()->retrieve(ifaceType, idString); 
                                                        if (iface) {
								if (iface->isUp()) {
                                                                        mdIface->setParameter("status", "up");
								} else {
                                                                        mdIface->setParameter("status", "down");
								} 
							} else {
                                                                mdIface->setParameter("status", "down");
							}
                                                        free(idString);
						}
						
						mdIface = md->getNextMetadata();
					}
				}
			}
			
			if (!event_m->addMetadata(md)) {
				HAGGLE_ERR("Could not add neighbor to IPC data object\n");
			} else {
				numNeigh++;
			}
		}
	}

	sendToAllApplications(dObj, LIBHAGGLE_EVENT_NEIGHBOR_UPDATE);
}

void ApplicationManager::onRetrieveNode(Event *e)
{
	if(!e || !e->hasData())
		return;
	
	NodeRef	appNode = e->getNode();
	
	HAGGLE_DBG("Sending registration reply to application %s\n", appNode->getName().c_str());
	
	appNode->getBloomfilter()->reset();
	kernel->getNodeStore()->add(appNode);
	updateApplicationInterests(appNode);
	numClients++;
	
	DataObjectRef dObjReply = DataObjectRef(new DataObject(NULL, 0), "DataObjectReply");
	
	if (!dObjReply) {
		HAGGLE_ERR("Could not allocate data object\n");
		return;
	}
	
	Metadata *ctrl_m = addControlMetadata(CTRL_TYPE_REGISTRATION_REPLY, appNode->getName(), dObjReply->getMetadata());
	
	if (!ctrl_m) {
		
		HAGGLE_ERR("Could not allocate control metadata\n");
		return;
	}
	
	ctrl_m->addMetadata(DATAOBJECT_METADATA_APPLICATION_CONTROL_SESSION, intToStr(sessionid++));
	
	ctrl_m->addMetadata(DATAOBJECT_METADATA_APPLICATION_CONTROL_MESSAGE, "OK");
	
	sendToApplication(dObjReply, appNode);
	
	HAGGLE_DBG("Sent registration reply to application %s\n", appNode->getName().c_str());	
	
	dObjReply->print(stdout);
}

static const char *ctrl_type_names[] = { 
	"registration_request", 
	"registration_reply", 
	"deregistration", 
	"register_interest",
	"remove_interest",
	"get_interests",
	"register_event_interest", 
	"matching_dataobject",
	"delete_dataobject",
	"get_dataobjects",
	"shutdown", 
	"event", 
	NULL 
};

static const control_type_t ctrl_name_to_type(const char *name)
{
	unsigned int i = 0;
	
	if (!name)
		return CTRL_TYPE_INVALID;
	
	while (ctrl_type_names[i]) {
		if (strcmp(ctrl_type_names[i], name) == 0) {
			return (control_type_t)i;
		}
		i++;
	}
	
	return CTRL_TYPE_INVALID;
}

Metadata *ApplicationManager::addControlMetadata(const control_type_t type, const string app_name, Metadata *parent)
{
	if (!parent)
		return NULL;
	
	Metadata *m = parent->addMetadata(DATAOBJECT_METADATA_APPLICATION);
	
	if (!m) {
		return NULL;
	}
	
	m->setParameter(DATAOBJECT_METADATA_APPLICATION_NAME_PARAM, app_name);
	
	Metadata *mc = m->addMetadata(DATAOBJECT_METADATA_APPLICATION_CONTROL);
	
	if (!mc) {
		return NULL;
	}
	
	mc->setParameter(DATAOBJECT_METADATA_APPLICATION_CONTROL_TYPE_PARAM, ctrl_type_names[type]);
		
	if (type == CTRL_TYPE_REGISTRATION_REPLY) {
		if (!mc->addMetadata(DATAOBJECT_METADATA_APPLICATION_CONTROL_DIRECTORY, HAGGLE_DEFAULT_STORAGE_PATH)) {
			return NULL;
		}
	}
	
	return mc;
}

void ApplicationManager::onReceiveFromApplication(Event *e)
{
	char id[NODE_ID_LEN];
	size_t decodelen = NODE_ID_LEN;
	struct base64_decode_context ctx;
	const Attribute *ctrlAttr;

	if (!e || !e->hasData())
		return;

	DataObjectRefList& dObjs = e->getDataObjectList();

	if (dObjs.size() == 0) {
		HAGGLE_ERR("No data objects in event\n");
		return;
	}

	while (dObjs.size()) {
		DataObjectRef dObj = dObjs.pop();

		if (!dObj->getRemoteInterface()) {
			HAGGLE_DBG("Data object has no source interface, ignoring!\n");
			return;
		}

		dObj->print();

		ctrlAttr = dObj->getAttribute(HAGGLE_ATTR_CONTROL_NAME);

		if (!ctrlAttr) {
			HAGGLE_ERR("Control data object from application does not have control attribute\n");
			return;
		}

		Metadata *m = dObj->getMetadata()->getMetadata(DATAOBJECT_METADATA_APPLICATION);
		
		if (!m) {
			HAGGLE_ERR("Control data object from application does not have application metadata\n");
			return;
		}
		
		const char *id_str = m->getParameter(DATAOBJECT_METADATA_APPLICATION_ID_PARAM);
		const char *name_str = m->getParameter(DATAOBJECT_METADATA_APPLICATION_NAME_PARAM);
		
		if (!id_str || !name_str) {
			HAGGLE_ERR("Control data object from application does not have id or name\n");
			return;
		}
		base64_decode_ctx_init(&ctx);
		base64_decode(&ctx, id_str, strlen(id_str), id, &decodelen);

		// Check if the node is in the node store. The result will be a null-node 
		// in case the application is not registered.
		NodeRef appNode = kernel->getNodeStore()->retrieve(id);
		
		Metadata *mc = m->getMetadata(DATAOBJECT_METADATA_APPLICATION_CONTROL);
		
		if (!mc) {
			HAGGLE_ERR("Data object from application is not a valid control data object\n");
			return;
		}
		
		/*
		  A control data object may have more than one control element.
		  Loop through them all...
		 */
		while (mc) {
			const char *type_str = mc->getParameter(DATAOBJECT_METADATA_APPLICATION_CONTROL_TYPE_PARAM);
			
			if (!type_str || ctrl_name_to_type(type_str) == CTRL_TYPE_INVALID) {
				HAGGLE_ERR("Data object from application has an invalid control type\n");
				return;
			}
			
			switch (ctrl_name_to_type(type_str)) {
				case CTRL_TYPE_REGISTRATION_REQUEST:
					HAGGLE_DBG("Received registration request from Application %s\n", name_str);
					
					if (appNode) {
						HAGGLE_DBG("Application %s is already registered\n", name_str);
						
						// Create a temporary application node to serve as target for the 
						// return value, since the data object most likely came from a 
						// different interface than the application is registered on.
						NodeRef newAppNode = new Node(NODE_TYPE_APPLICATION, (char *) NULL);
						newAppNode->addInterface(dObj->getRemoteInterface());
						
						DataObjectRef dObjReply = new DataObject(NULL, 0);
						
						if (!dObjReply)
							break;
						
						// Create a reply saying "BUSY!"
						Metadata *ctrl_m = addControlMetadata(CTRL_TYPE_REGISTRATION_REPLY, name_str, dObjReply->getMetadata());
						
						if (ctrl_m) {
							ctrl_m->addMetadata(DATAOBJECT_METADATA_APPLICATION_CONTROL_MESSAGE, "Already registered");
							sendToApplication(dObjReply, newAppNode);
						}
					} else {
						HAGGLE_DBG("app name=%s\n", name_str);
						
						appNode = NodeRef(new Node(NODE_TYPE_APPLICATION, id, name_str));
						
						appNode->addInterface(dObj->getRemoteInterface());
						
						// The reply will be generated by the callback event handler.
						kernel->getDataStore()->retrieveNode(appNode, onRetrieveNodeCallback, true);
					}
					break;
				case CTRL_TYPE_DEREGISTRATION_NOTICE:
					if (!appNode)
						break;
					
					HAGGLE_DBG("Application %s wants to deregister\n", appNode->getName().c_str());
					deRegisterApplication(appNode);
					break;
				case CTRL_TYPE_SHUTDOWN:
					if (!appNode)
						break;
					
					HAGGLE_DBG("Application %s wants to shutdown\n", appNode->getName().c_str());
					kernel->shutdown();
					break;
				case CTRL_TYPE_REGISTER_INTEREST:
					if (appNode) {
						unsigned long numattrs = 0;
						unsigned long numattrsThisNode = 0;						
						Metadata *interest = mc->getMetadata(DATAOBJECT_METADATA_APPLICATION_CONTROL_INTEREST);
						
						while (interest) {
							const char *interest_name_str = interest->getParameter(DATAOBJECT_METADATA_APPLICATION_CONTROL_INTEREST_NAME_PARAM);
							const char *interest_weight_str = interest->getParameter(DATAOBJECT_METADATA_APPLICATION_CONTROL_INTEREST_WEIGHT_PARAM);
							unsigned long weight = 1;
							
							if (interest_weight_str) {
								weight = strtoul(interest_weight_str, NULL, 10);
							}
							if (interest_name_str) {
								
								if (kernel->getThisNode()->addAttribute(interest_name_str, interest->getContent(), weight)) {
									numattrsThisNode++;
									HAGGLE_DBG("Application %s adds interest %s:%s:%lu to thisNode\n", 
										   appNode->getName().c_str(), interest_name_str, 
										   interest->getContent().c_str(), weight);
								}
								if (appNode->addAttribute(interest_name_str, interest->getContent(), weight)) {
									numattrs++;
									HAGGLE_DBG("Application %s adds interest %s:%s:%lu\n", 
										   appNode->getName().c_str(), interest_name_str, 
										   interest->getContent().c_str(), weight);
								}
							}
							interest = interest->getNextMetadata();
						}
						if (numattrs) {
							updateApplicationInterests(appNode);
						}
						if (numattrsThisNode) {
							// Push the updated node description to all neighbors
							kernel->addEvent(new Event(EVENT_TYPE_NODE_DESCRIPTION_SEND));
						}						
					}
					break;
				case CTRL_TYPE_REMOVE_INTEREST:
					if (appNode) {
						unsigned long numattrs = 0;

						Metadata *interest = mc->getMetadata(DATAOBJECT_METADATA_APPLICATION_CONTROL_INTEREST);
						
						while (interest) {
							const char *interest_name_str = interest->getParameter(DATAOBJECT_METADATA_APPLICATION_CONTROL_EVENT_TYPE_PARAM);
							
							if (interest_name_str) {
								if (appNode->removeAttribute(interest_name_str, interest->getContent()))
									numattrs++;
								
								HAGGLE_DBG("Application %s removes interest %s:%s\n", 
									   appNode->getName().c_str(), interest_name_str, interest->getContent().c_str());
							}
							interest = interest->getNextMetadata();
						}
						if (numattrs) {
							updateApplicationInterests(appNode);
							
							// Update the node description and send it to all 
							// neighbors.
							kernel->getDataStore()->retrieveNodeByType(NODE_TYPE_APPLICATION, onRetrieveAppNodesCallback);
						}
					}
					break;
				case CTRL_TYPE_GET_INTERESTS:
					HAGGLE_DBG("Request for application interests\n");
					
					if (appNode) {
						DataObjectRef dObjReply = new DataObject(NULL, 0);
						
						if (!dObjReply)
							break;
						
						Metadata *ctrl_m = addControlMetadata(CTRL_TYPE_EVENT, name_str, dObjReply->getMetadata());
												
						if (!ctrl_m) 
							break;

						Metadata *event_m = ctrl_m->addMetadata(DATAOBJECT_METADATA_APPLICATION_CONTROL_EVENT);
						
						if (!event_m)
							break;
						
						event_m->setParameter(DATAOBJECT_METADATA_APPLICATION_CONTROL_EVENT_TYPE_PARAM, intToStr(LIBHAGGLE_EVENT_INTEREST_LIST));
				
						appNode.lock();
						
						const Attributes *attrs = appNode->getAttributes();
						
						for (Attributes::const_iterator it = attrs->begin(); it != attrs->end(); it++) {
							const Attribute& a = (*it).second;
							HAGGLE_DBG("Adding interest %s to the reply\n", a.getString().c_str());
							
							Metadata *interest = event_m->addMetadata(DATAOBJECT_METADATA_APPLICATION_CONTROL_EVENT_INTEREST, a.getValue());
							
							if (interest) {
								interest->setParameter(DATAOBJECT_METADATA_APPLICATION_CONTROL_EVENT_INTEREST_NAME_PARAM, a.getName());
								interest->setParameter(DATAOBJECT_METADATA_APPLICATION_CONTROL_EVENT_INTEREST_WEIGHT_PARAM, a.getWeightAsString());
							}
						}
						appNode.unlock();
						
						sendToApplication(dObjReply, appNode);
					}
					break;
				case CTRL_TYPE_REGISTER_EVENT_INTEREST:
					if (appNode) {						
						Metadata *event = mc->getMetadata(DATAOBJECT_METADATA_APPLICATION_CONTROL_EVENT);
						
						HAGGLE_DBG("REGISTER event interest\n");
						while (event) {
							const char *event_type_str = event->getParameter(DATAOBJECT_METADATA_APPLICATION_CONTROL_EVENT_TYPE_PARAM);
							
							HAGGLE_DBG("event type is %s\n", event_type_str);
							if (event_type_str) {
								addApplicationEventInterest(appNode, atoi(event_type_str));
							}
							event = event->getNextMetadata();
						}
					}
					break;
				case CTRL_TYPE_GET_DATAOBJECTS:
					if (!appNode)
						break;
					
					// Clear the bloomfilter:
					appNode->getBloomfilter()->reset();
					// And do a filter matching for this node:
					updateApplicationInterests(appNode);
					break;
				case CTRL_TYPE_DELETE_DATAOBJECT:
					if (appNode) {
						Metadata *dobj_m = mc->getMetadata(DATAOBJECT_METADATA_APPLICATION_CONTROL_DATAOBJECT);
				
						if (!dobj_m)
							break;
						
						const char *id_str = dobj_m->getParameter(DATAOBJECT_METADATA_APPLICATION_CONTROL_DATAOBJECT_ID_PARAM);
						
						if (!id_str)
							break;
												
						DataObjectId_t id;
						base64_decode_context ctx;
						size_t len = DATAOBJECT_ID_LEN;
						base64_decode_ctx_init(&ctx);
						
						if (base64_decode(&ctx, id_str, strlen(id_str), (char *)id, &len)) {
							kernel->getDataStore()->deleteDataObject(id);
						}
					}
					break;
				case CTRL_TYPE_INVALID:
				default:
					HAGGLE_ERR("Data object from application has invalid control type=%s\n", type_str);
					return;
			}
			mc = mc->getNextMetadata();
		}
	}
}
