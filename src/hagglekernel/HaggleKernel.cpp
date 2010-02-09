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
#include <time.h>

#include <libcpphaggle/Platform.h>
#include <libcpphaggle/Watch.h>

#include "HaggleKernel.h"
#include "Event.h"
#include "EventQueue.h"
#include "Interface.h"
#include "SQLDataStore.h"

using namespace haggle;

void HaggleKernel::setThisNode(NodeRef &_thisNode)
{
	thisNode = _thisNode;
}

#define HOSTNAME_LEN 100

HaggleKernel::HaggleKernel(DataStore *ds , const string _storagepath) :
	dataStore(ds), starttime(Timeval::now()), shutdownCalled(false),
	running(false), storagepath(_storagepath)
{
	
}

bool HaggleKernel::init()
{
	char hostname[HOSTNAME_LEN];

#if defined(OS_WINDOWS_MOBILE) || defined(OS_ANDROID)
	if (!Trace::trace.enableFileTrace())
		HAGGLE_ERR("Could not enable file tracing\n");
#endif

#ifdef OS_WINDOWS
	WSADATA wsaData;
	int iResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (iResult != 0) {
		HAGGLE_ERR("WSAStartup failed: %d\n", iResult);
                return false;
	}
#endif
	
	if (dataStore) {
		dataStore->kernel = this;
		if (!dataStore->init()) {
			HAGGLE_ERR("Data store could not be initialized\n");
			return false;
		}
	} else {
		HAGGLE_ERR("No data store!!!\n");
		return false;
	}

	// The interfaces on this node will be discovered when the
	// ConnectivityManager is started. Hence, they will not be
	// part of thisNode when we insert it in the datastore. This
	// is a potential problem.
	//dataStore->insertNode(&thisNode);

	int res = gethostname(hostname, HOSTNAME_LEN);

	if (res != 0) {
		HAGGLE_ERR("Could not get hostname\n");
		return false;
	}
	
	// Always zero terminate in case the hostname didn't fit
	hostname[HOSTNAME_LEN-1] = '\0';
        
        HAGGLE_DBG("Hostname is %s\n", hostname);

	thisNode = nodeStore.add(Node::create(NODE_TYPE_THIS_NODE, hostname));

	if (!thisNode) {
		HAGGLE_ERR("Could not create this node\n");
		return false;
	}
	currentPolicy = NULL;

	return true;
}

HaggleKernel::~HaggleKernel()
{
#ifdef OS_WINDOWS
	// Cleanup winsock
	WSACleanup();
#endif
	// Now that it has finished processing, delete the data store:
	if (dataStore)
		delete dataStore;

	HAGGLE_DBG("Done\n");
}

int HaggleKernel::registerManager(Manager *m)
{
	wregistry_t wr;

	if (!m)
		return -1;
	
	/*
		Insert this empty wregistry_t. When we add it to the registry,
		the empty wregistry will be copied, so it doesn't matter 
		that the source object is stack-allocated.
	 */
	
	if (!registry.insert(make_pair(m, wr)).second) {
		HAGGLE_ERR("Manager \'%s\' already registered\n", m->getName());
		return -1;
	}

	HAGGLE_DBG("Manager \'%s\' registered\n", m->getName());

	return registry.size();
}

int HaggleKernel::unregisterManager(Manager *m)
{
	if (!m)
		return -1;
	
	if (registry.erase(m) != 1) {
		HAGGLE_ERR("Manager \'%s\' not registered\n", m->getName());
		return 0;
	}

#ifdef DEBUG
        registry_t::iterator it;
        string registeredManagers;
	
	for (it = registry.begin(); it != registry.end(); it++) {
                registeredManagers += String(" ").append((*it).first->getName());
        }
	HAGGLE_DBG("Manager \'%s\' unregistered. Still %lu registered:%s\n", 
		   m->getName(), registry.size(), registeredManagers.c_str());
#endif	
	/*
	if (registry.size() == 0) {
		// Tell the data store to cancel itself
		dataStore->cancel();
		HAGGLE_DBG("Data store cancelled!\n");
	}
	 */
	return registry.size();
}

int HaggleKernel::registerWatchable(Watchable wbl, Manager *m)
{
	if (!m)
		return -1;
	
	if (!wbl.isValid()) {
		HAGGLE_ERR("Manager \'%s\' tried to register invalid watchable\n", m->getName());
		return -1;
	}
	registry_t::iterator it = registry.find(m);
	
	if (it == registry.end()) {
		HAGGLE_ERR("Non-registered manager \'%s\' tries to register a watchable\n", m->getName());
		return -1;
	}
	
	wregistry_t& wr = (*it).second;
	
        if (!wr.insert(make_pair(wbl, 0)).second) {
		HAGGLE_ERR("Manager \'%s\' has already registered %s\n", m->getName(), wbl.getStr());
                return -1;
        }
	
	HAGGLE_DBG("Manager \'%s\' registered %s\n", m->getName(), wbl.getStr());

	return wr.size();
}

int HaggleKernel::unregisterWatchable(Watchable wbl)
{
	registry_t::iterator it;
	
	for (it = registry.begin(); it != registry.end(); it++) {
		wregistry_t& wr = (*it).second;
		
		if (wr.erase(wbl) == 1) {			
			HAGGLE_DBG("Manager \'%s\' unregistered %s\n", (*it).first->getName(), wbl.getStr());
			return wr.size();
		}
	}
	
	HAGGLE_ERR("Could not unregister %s, as it was not found in registry\n", wbl.getStr());

	return 0;
}

void HaggleKernel::signalIsReadyForStartup(Manager *m)
{
	for (registry_t::iterator it = registry.begin(); it != registry.end(); it++) {
		if (!(*it).first->isReadyForStartup())
			return;
	}
	HAGGLE_DBG("All managers are ready for startup, generating startup event!\n");
	
	running = true;
	addEvent(new Event(EVENT_TYPE_STARTUP));
}

void HaggleKernel::signalIsReadyForShutdown(Manager *m)
{
	HAGGLE_DBG("%s signals it is ready for shutdown\n", m->getName());
	
	for (registry_t::iterator it = registry.begin(); it != registry.end(); it++) {
		if (!(*it).first->isReadyForShutdown()) {
			HAGGLE_DBG("%s is not ready for shutdown\n", (*it).first->getName());
			return;
		}
	}
	
	HAGGLE_DBG("All managers are ready for shutdown, generating shutdown event!\n");
	running = false;
	enableShutdownEvent();
}

#ifdef DEBUG
void HaggleKernel::printRegisteredManagers()
{
	registry_t::iterator it;
	
	printf("============= Manager list ==================\n");
	
	for (it = registry.begin(); it != registry.end(); it++) {
		Manager *m = (*it).first;
		printf("%s:", m->getName());
		wregistry_t& wr = (*it).second;
		wregistry_t::iterator itt = wr.begin();
		
		for (; itt != wr.end(); itt++) {
			printf(" %s ", (*itt).first.getStr());
		}
		printf("\n");
	}
	
	printf("=============================================\n");

}
#endif

Manager *HaggleKernel::getManager(char *name)
{
	for (registry_t::iterator it = registry.begin(); it != registry.end(); it++) {
		if (strcmp((*it).first->getName(), name) == 0) {
			return (*it).first;
		}
	}
	return NULL;
}
void HaggleKernel::shutdown()
{ 
	if (!running || shutdownCalled) 
		return;
	
	shutdownCalled = true;
	
	addEvent(new Event(EVENT_TYPE_PREPARE_SHUTDOWN));
}


// TODO: Allow us to read an XML file from stdin
bool HaggleKernel::readStartupDataObject(FILE *fp)
{
	long i, len;
	ssize_t	k;
	size_t j;
	char *data;
	bool ret = false;
	
	// Not using a ref here, because it's faster not to, and no other thread
	// could possibly see this data object before we put it into an event,
	// at which time it will be made into a data object reference anyway.
	DataObject *theDO = NULL;
	
	if (!fp)
		return false;
	
	// Figure out how large the file is.
	fseek(fp, 0, SEEK_END);
	// No, this does not handle files larger than 2^31 bytes, but that 
	// shouldn't be a problem, and could be easily fixed if necessary.
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	// Allocate space to put the data in:
	data = (char *) malloc(sizeof(char) * len);
	
	if (!data)
		return false;
	
	// Read the data:
	if (fread(data, len, 1, fp) == 1) {
		// Success! Now create data object(s):
		i = 0;
		
		while (i < len) {
			if (theDO == NULL)
				theDO = DataObject::create_for_putting();
			
			if (!theDO) {
				ret = false;
				goto out;
			}

			k = theDO->putData(&(data[i]), len - i, &j);
			
			// Check return value:
			if (k < 0) {
				// Error occured, stop everything:
				HAGGLE_ERR("Error parsing startup data object "
					   "(byte %ld of %ld)\n", i, len);
				i = len;
			} else {
				// No error, check if we're done:
				if (k == 0 || j == 0) {
					// Done with this data object:
					addEvent(new Event(EVENT_TYPE_DATAOBJECT_RECEIVED, 
							   DataObjectRef(theDO)));
					// Should absolutely not deallocate this, since it 
					// is in the event queue:
					theDO = NULL;
					// These bytes have been consumed:
					i += k;
					ret = true;
				} else if (k > 0) {
					// These bytes have been consumed:
					i += k;
				}
			}
		}
		// Clean up:
		if (theDO) {
			delete theDO;
		}
	}
out:
	free(data);
	
	return ret;
}

bool HaggleKernel::readStartupDataObjectFile(string cfgObjPath)
{
	FILE *fp;
	bool ret;
	int i = 0;
	
	// Open file, try two paths for backwards compatibility
	do {	
		fp = fopen(cfgObjPath.c_str(), "r");
		
		// Did we succeed?
		if (fp)
			break;
		
		cfgObjPath = STARTUP_DATAOBJECT_PATH;
	} while (i++ < 1);
	
	if (!fp)
		return false;
	
	ret = readStartupDataObject(fp);

	// Close the file:
	fclose(fp);
	
	return ret;
}

void HaggleKernel::run()
{
	bool shutdownmode = false;
	
	// Start the data store
	dataStore->start();

	addEvent(new Event(EVENT_TYPE_PREPARE_STARTUP));
	
	readStartupDataObjectFile();
	
	while (registry.size()) {
		Watch w;
		Timeval now = Timeval::now();
		int signalIndex, res;
		EQEvent_t ee;
		Timeval timeout, *t = NULL;
		Event *e = NULL;
		
		/*
		 We make a copy of the registry each time we loop. This is because a manager
		 can unregister sockets, or itself, in the event (or socket) it processes.
		 Therefore, if we'd use the original registry, it might become inconsistent as 
		 we iterate it in the event loop.
		 */
		registry_t reg = registry;

		/* 
		   Get the time until the next event and check the status of
		   the event.

		*/
		ee = getNextEventTime(&timeout);
		
		switch (ee) {
			case EQ_EVENT_SHUTDOWN:
				HAGGLE_DBG("\n****************** SHUTDOWN EVENT *********************\n\n");
				shutdownmode = true;
			case EQ_EVENT:
				if (shutdownmode)
					timeout = 0;
						
				/* Convert the timeout from absolute time to relative time.
				 We need to make sure we do not have a negative time, which sometimes can 
				 happen when there are events added with a timeout of zero.
				 */
				if (now >= timeout)
					timeout.zero();
				else
					timeout -= now;
				
				// Set the pointer to our Timeval
				t = &timeout;
				
				break;
			case EQ_EMPTY:
				// Timeval NULL pointer = no timeout
				t = NULL;
				break;
			case EQ_ERROR:
			default:
				// This should not really happen
				HAGGLE_ERR("ERROR: Bad state in event loop\n");
				break;
		}
		
		/*
			Iterate through the registered sockets and add them to our Watch. The registry
			does not require locking, as only managers and manager modules running 
			in the same thread as the kernel may register sockets. Modules running
			in separate threads do not need to register sockets, as they can easily 
			implement their own run-loop.
		 
		 */
		registry_t::iterator it = reg.begin();
		
		for (; it != reg.end(); it++) {
			wregistry_t& wr = (*it).second;
			wregistry_t::iterator itt = wr.begin();
			for (; itt != wr.end(); itt++) {
				(*itt).second = w.add((*itt).first);
				//HAGGLE_DBG("watchable %s added to watch with index %d\n", (*itt).first.getStr(), (*itt).second);
			}
		}
		
		/*
		 Add the Signal that is raised whenever something is added to the event queue.
		 */
		signalIndex = w.add(signal);
				
		//HAGGLE_DBG("Waiting on kernel watch with timeout %s\n", t ? t->getAsString().c_str() : "INFINATE");
		
		res = w.wait(t);
		
		if (res == Watch::TIMEOUT) {
			// Timeout occurred -> Process event from EventQueue
			e = getNextEvent();

                        LOG_ADD("%s: %s\n", Timeval::now().getAsString().c_str(), e->getDescription().c_str());
			
			if (e->isPrivate()) {
				//HAGGLE_DBG("Doing private event callback: %s\n", e->getName());
				e->doPrivateCallback();
			} else if (e->isCallback()) {
				//HAGGLE_DBG("Doing callback\n");
				e->doCallback();
			} else {
				/* 
				 Loop through all registered managers and check whether they are 
				 interested in this event.
				 */
				registry_t::iterator it = reg.begin();
				
				//HAGGLE_DBG("Doing public event %s\n", e->getName());
				
				for (; it != reg.end(); it++) {
					Manager *m = (*it).first;
					EventCallback < EventHandler > *callback = m->getEventInterest(e->getType());
					if (callback) {
						(*callback) (e);
					}
				}
			}
			
			/*
				Delete the event object. This may also delete
				data associated with the event. Data passed in public events 
				should be reference counted with the Reference class. Data in
				private events and callback events may not be reference counted,
				but the associated event data will not be deleted in that case.
				It is up to the private handlers to manage that data.
			 */
			delete e;
		} else if (res == Watch::FAILED) {
			HAGGLE_ERR("Main run-loop error on Watch : %s\n", STRERROR(ERRNO));
			continue;
		}
		
		//HAGGLE_DBG("%d objects are set\n", res);
		
		if (w.isSet(signalIndex)) {
			/* Something was added to the queue. We do not
			   need to do anyting here as this signal is
			   only a trigger for us to check the queue
			   again. */  

			//HAGGLE_DBG("Queue signal was set\n");
		}
		// Check and handle readable watchables. 
		it = reg.begin();
		
		for (; it != reg.end(); it++) {
			Manager *m = (*it).first;
			wregistry_t& wr = (*it).second;
			wregistry_t::iterator itt = wr.begin();
			
			for (; itt != wr.end(); itt++) {				
				//HAGGLE_DBG("Checking if watchable %s with watch index %d is set\n", (*itt).first.getStr(), (*itt).second);

				if (w.isSet((*itt).second)) {
					//HAGGLE_DBG("Watchable %s with watch index %d is set\n", (*itt).first.getStr(), (*itt).second);
					m->onWatchableEvent((*itt).first);
				}
			}
		}
	}
	HAGGLE_DBG("Kernel exits from main loop\n");

	// stop the dataStore thread and try to join with its thread
	HAGGLE_DBG("Joining with DataStore thread\n");
	dataStore->stop();
	HAGGLE_DBG("Joined\n");
}

