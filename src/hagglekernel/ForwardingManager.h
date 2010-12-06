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
#ifndef _FORWARDINGMANAGER_H
#define _FORWARDINGMANAGER_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/
class ForwardingManager;

#include <libcpphaggle/List.h>
#include <libcpphaggle/Pair.h>

using namespace haggle;

#include "Event.h"
#include "Manager.h"
#include "DataObject.h"
#include "Node.h"
#include "Forwarder.h"

#define MAX_NODES_TO_FIND_FOR_NEW_DATAOBJECTS	(10)
#define ENABLE_RECURSIVE_ROUTING_UPDATES 1

typedef List< Pair< Pair<const DataObjectRef, const NodeRef>, int> > forwardingList;

/** */
class ForwardingManager : public Manager
{
	EventCallback<EventHandler> *dataObjectQueryCallback;
	EventCallback<EventHandler> *delayedDataObjectQueryCallback;
	EventCallback<EventHandler> *nodeQueryCallback;
	EventCallback<EventHandler> *forwardDobjCallback;
	EventCallback<EventHandler> *repositoryCallback;
	
	EventType moduleEventType, routingInfoEventType;
	
	forwardingList forwardedObjects;
	Forwarder *forwardingModule;
	List<NodeRef> pendingQueryList;
#if defined(ENABLE_RECURSIVE_ROUTING_UPDATES)
	bool recursiveRoutingUpdates;
#endif
	bool doQueryOnNewDataObject;
	
	void onPrepareStartup();
	void onPrepareShutdown();
	
        // See comment in ForwardingManager.cpp about isNeighbor()
        bool isNeighbor(const NodeRef& node);
        bool addToSendList(DataObjectRef& dObj, const NodeRef& node, int repeatCount = 0);
	/**
		This function changes out the current forwarding module (initially none)
		to the given forwarding module.
		
		The current forwarding module's state is stored in the repository before
		the forwarding module is stopped and deleted.
		
		The given forwarding module's state (if any) is retreived from the 
		repository, and all forwarding data objects that the forwarding module
		is interested in is retreived from the data store.
		
		This function takes possession of the given forwarding module, and will
		take responsibility for releasing it.
	*/
	void setForwardingModule(Forwarder *f, bool deRegisterEvents = false);

	bool init_derived();

	Forwarder *getForwarder() { return forwardingModule; }
	bool shouldForward(const DataObjectRef& dObj, const NodeRef& node);
	void forwardByDelegate(DataObjectRef &dObj, const NodeRef &target, const NodeRefList *other_targets = NULL);
	void onShutdown();
	void onForwardingTaskComplete(Event *e);
	void onDataObjectForward(Event *e);
	void onSendDataObjectResult(Event *e);
	void onDataObjectQueryResult(Event *e);
	void onNodeQueryResult(Event *e);
	void onNodeUpdated(Event *e);
	void onRoutingInformation(Event *e);
	void onNewDataObject(Event *e);
	void onNewNeighbor(Event *e);
	void onEndNeighbor(Event *e);
	void onRepositoryData(Event *e);
	void onTargetNodes(Event *e);
	void onDelegateNodes(Event *e);
	void onDelayedDataObjectQuery(Event *e);
	void onConfig(Metadata *m);
	void findMatchingDataObjectsAndTargets(NodeRef& node);
#ifdef DEBUG
	void onDebugCmd(Event *e);
#endif
#if defined(ENABLE_RECURSIVE_ROUTING_UPDATES)
	size_t metadataToRecurseList(Metadata *m, NodeRefList& trigger_list);
	Metadata *recurseListToMetadata(Metadata *m, const NodeRefList& trigger_list);
	void recursiveRoutingUpdate(NodeRef peer, Metadata *m);
#endif
public:
	ForwardingManager(HaggleKernel *_kernel = haggleKernel);
	~ForwardingManager();
};

#endif /* _FORWARDINGMANAGER_H */
