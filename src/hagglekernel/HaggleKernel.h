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
#ifndef _HAGGLEKERNEL_H
#define _HAGGLEKERNEL_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/

class HaggleKernel;

#include <time.h>

#include <libcpphaggle/Platform.h>
#include <libcpphaggle/Timeval.h>
#include <libcpphaggle/Pair.h>
#include <libcpphaggle/Map.h>
#include <libcpphaggle/List.h>

using namespace haggle;

#include "Event.h"
#include "EventQueue.h"
#include "Manager.h"
#include "DataStore.h"
#include "Filter.h"
#include "SQLDataStore.h"
#include "NodeStore.h"
#include "InterfaceStore.h"
#include "Trace.h"
#include "Utility.h"
#include "Policy.h"

/** 
	HaggleKernel:
 
 */
class HaggleKernel : public EventQueue
{
	NodeRef thisNode;
	PolicyRef currentPolicy;
	InterfaceStore interfaceStore;
	NodeStore nodeStore;
	DataStore *dataStore;
	Timeval starttime;
	bool shutdownCalled;
	bool running; // true if running (after startup)
	
	/*
	 We have a registry of registered managers, where each 
	 manager has a set of <watchable, watch indices> pairs.
	 */
	typedef Map<Watchable, int> wregistry_t;
	typedef Map<Manager *, wregistry_t> registry_t;
	registry_t registry;
	const string storagepath; // Path to where we can write files, etc.
	void closeAllSockets();
	
	/**
		This function is called by the run() function to locate and read the
		startup data object file, and add the content to the event queue.
	*/
	void readStartupDataObjectFile(void);
public:
	
	InterfaceStore *getInterfaceStore() { return &interfaceStore; }
	NodeStore *getNodeStore() { return &nodeStore; }
	const string getStoragePath() const { return storagepath; }
        NodeRef& getThisNode() { return thisNode; }
        void setThisNode(NodeRef &_thisNode);
        PolicyRef& getCurrentPolicy() { return currentPolicy; }
        void setCurrentPolicy(PolicyRef &_currentPolicy) { currentPolicy = _currentPolicy; }
        DataStore *getDataStore() { return dataStore; }
	
	/**
		Register a manager with the kernel. The manager will be
		registered until unregisterManager() is called. The kernel
		will not exit until all managers have unregistered.
	 
		Returns: The number of currently registered managers on success.
		0 if the manager is already registered, and -1 on failure.
	 */
        int registerManager(Manager *m);
	/**
		Unregister a manager from the kernel. When no managers are
		registered with the kernel, the kernel will exit.
		
		Returns: The number of managers still registered after the given
		manager has unregistered. 0 if the manager was not registered with
		the kernel, and -1 on failure.
	 */
        int unregisterManager(Manager *m);
	/**
		Register a watchable with the kernel. This allows a manager to run
		in the same thread as the kernel and still be able to watch, e.g.,
		sockets. When the watchable is readable (or writable), the kernel
		will call the manager's onWatchableEvent() callback function.
	 
		A manager should not read or write lots of information in the 
		callback as that will block the kernel's event loop. A typical use 
		of a registered watchable is to listen for incoming connections. When
		such a connection occurs, the manager should start a manager module
		in a new thread to read and write on a new client watchable, which is
		no registered in the kernel.
	 
		Returns: The number of registered sockets that this manager has after
		the given watchable has been registered. 0 If the watchable is already 
		registered, and -1 on failure (e.g., a non-registered manager tries to
		register a watchable).
	 */
        int registerWatchable(Watchable wbl, Manager *m);
	/**
		Unregister a previously registered watchable. 
		Returns: The number of registered watchables that the calling manager has
		still registered after the given watchable has been unregistered. -1 if 
		the manager's watchable was already registered, or there was a failure.
	 */
        int unregisterWatchable(Watchable wbl);
	/**
		Since managers should not communicate directly with each other, this
		function should be used only in exceptional cases. It was needed by the 
		debug manager to create the xml dump, which is why it was written.
	*/
	Manager *getManager(char *name);
	
	/**
		Initiate kernel shutdown. This will first generate a PREPARE_SHUTDOWN event 
		which all managers will receive. When all managers have signaled that they 
		are prepared for a real shutdown, a SHUTDOWN event will automatically be 
		generated. This will cause the managers to themselves initiate
		shutdown and eventually unregister with the kernel. When all managers have
		unregistered, the kernel will itself exit.
	 
		Note: During shutdown, the kernel will shortcut the timeouts in the event
		queue, i.e., it will process the event queue as quickly as it can until all 
		managers have unregistered.
	 */
	void shutdown();
	
	bool isShuttingDown() { return shutdownCalled; }

	/**
	 This should be called by a manager when it is ready for startup.
	 */
	void signalIsReadyForStartup(Manager *m);
	
	/**
		This should be called by a manager when it is ready for shutdown.
	 */
	void signalIsReadyForShutdown(Manager *m);
	
	Timeval getStartTime() const { return starttime; }
	
#ifdef DEBUG
	void printRegisteredManagers();
#endif
	/**
	 
	 */
	HaggleKernel(DataStore *ds = new SQLDataStore(false, DEFAULT_DATASTORE_FILEPATH), 
		     const string storagepath = HAGGLE_DEFAULT_STORAGE_PATH);
	~HaggleKernel();
	
	void run();
};


#endif /* _HAGGLEKERNEL_H */
