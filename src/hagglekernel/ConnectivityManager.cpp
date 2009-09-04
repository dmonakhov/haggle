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

#include "ConnectivityLocal.h"
#if defined(ENABLE_ETHERNET)
#include "ConnectivityEthernet.h"
#endif
#if defined(ENABLE_BLUETOOTH)
#include "ConnectivityBluetooth.h"
#endif
#if defined(ENABLE_MEDIA)
#include "ConnectivityMedia.h"
#endif
#include "ConnectivityManager.h"

#include "Utility.h"
/*
  CM_IFACE_DBG macro - debugging for the connectivity manager.

  Turn this on to make the connectivity manager verbose about what it is
  doing with interfaces (adding, tagging as up, deleting, etc.).

  WARNING: turning this on makes the connectivity manager EXTREMELY VERBOSE.
*/
#if 1
#define CM_IFACE_DBG(f, ...)					\
	HAGGLE_DBG("***CM_IFACE*** " f "\n", ## __VA_ARGS__)
#else
#define CM_IFACE_DBG(f, ...)
#endif

ConnectivityManager::ConnectivityManager(HaggleKernel * _haggle) :
                Manager("ConnectivityManager", _haggle), blMutex("blacklist"),
                connMutex("ConnRegistry"), ifMutex("ifRegistry")
{
#define __CLASS__ ConnectivityManager
	CM_DBG("Starting up\n");

	garbageEType = registerEventType("ConnectivityManager Garbage Collect Event", on_GC_snooped_ifaces);

#if HAVE_EXCEPTION
	if (garbageEType < 0)
		throw ConnectivityException(garbageEType, "Could not register garbage collection event type...");
#endif
	setEventHandler(EVENT_TYPE_DATAOBJECT_RECEIVED, onReceivedDataObject);
	setEventHandler(EVENT_TYPE_RESOURCE_POLICY_NEW, onNewPolicy);
#ifdef DEBUG
	setEventHandler(EVENT_TYPE_DEBUG_CMD, onDebugCmdEvent);
#endif
	
	deleteConnectivityEType = registerEventType("ConnectivitManager Delete Connectivity Event", onDeleteConnectivity);

#if HAVE_EXCEPTION
	if (deleteConnectivityEType < 0)
		throw ConnectivityException(deleteConnectivityEType, "Could not register connectivity event type...");
#endif
	
	registerEventTypeForFilter(blacklistFilterEvent, "Blacklist filter", onBlacklistDataObject, "Connectivity=*");
}

/*
	Defer the startup of local connectivity until every other manager is
	prepared for startup. This will avoid any discovery of other nodes
	that might trigger events before other managers are prepared.
 */
void ConnectivityManager::onStartup()
{
	// Create and start local connectivity module
	Connectivity *conn = new ConnectivityLocal(this);
	
	if (conn->startDiscovery()) {
		// Success! Store the connectivity:
                Mutex::AutoLocker l(connMutex);
		conn_registry.push_back(conn);
	} else {
		// Failure: delete the connectivity:
		delete conn;

		HAGGLE_ERR("Unable to start local connectivity module.\n");
	}
}

void ConnectivityManager::onPrepareShutdown()
{
	int n = 0;

	unregisterEventTypeForFilter(blacklistFilterEvent);
	
	CM_DBG("Telling all connectivities to cancel.\n");

	// If some connectivities are still running, then stop them.
	// Stopping a connectivity will wait for it to join... which
	// means that we will block until all connectivities are done.
        Mutex::AutoLocker l(connMutex);

	if (conn_registry.empty()) {
		signalIsReadyForShutdown();
	} else {
		// Go through all the connectivities:
		for (connectivity_registry_t::iterator it = conn_registry.begin(); it != conn_registry.end(); it++) {
			// Tell connectivity to cancel all activity:
			CM_DBG("Telling %s to cancel\n", (*it)->getName());
			(*it)->cancelDiscovery();
			n++;
		}
	}

	CM_DBG("Cancelled %d connectivity conn_registry\n", n);
}

ConnectivityManager::~ConnectivityManager()
{
        while (!blacklist.empty()) {
                Interface *iface = blacklist.front();
                blacklist.pop_front();
                delete iface;
        }
}

void ConnectivityManager::onBlacklistDataObject(Event *e)
{
        DataObjectRef dObj = e->getDataObject();
	
        if (!isValidConfigDataObject(dObj)) {
		HAGGLE_DBG("Received INVALID config data object\n");
		return;
	}
        HAGGLE_DBG("Received blacklist data object\n");

        Metadata *mc = dObj->getMetadata()->getMetadata("Connectivity");

        if (!mc) {
                HAGGLE_ERR("No connectivity metadata in data object\n");
                return;
        }
        
        Metadata *blm = mc->getMetadata("Blacklist");

        while (blm) {
                const char *type = blm->getParameter("type");
                const char *action = blm->getParameter("action");
                string mac = blm->getContent();
                InterfaceType_t iftype = Interface::strToType(type);
                int act = 3;
                
                /*
                  "action=add" means add interface to blacklist if not 
                  present.
                  
                  "action=remove" means remove interface from blacklist if 
                  present.
                  
                  All other values, including not having an "action" parameter
                  means add interface if it is not in the blacklist, remove 
                  interface if it is in the blacklist (effectively a toggle).
                */

                if (action != NULL) {
                        if (strcmp(action, "add") == 0)
                                act = 1;
                        else if (strcmp(action, "remove") == 0)
                                act = 2;
                }
				
                if (iftype == IFTYPE_ETHERNET || 
                    iftype == IFTYPE_BLUETOOTH ||
                    iftype == IFTYPE_WIFI) {
                        struct ether_addr etha;
                        
                        if (ether_aton_r(mac.c_str(), &etha)) {
                                if (isBlacklisted(iftype, (char *)&etha)) {
                                        if (act != 1) {
                                                HAGGLE_DBG("Removing interface [%s - %s] from blacklist\n", type, mac.c_str());
                                                removeFromBlacklist(iftype, (char *)&etha);
                                        } else {
                                                HAGGLE_DBG("NOT removing interface [%s - %s] from blacklist (it's not there)\n", type, mac.c_str());
                                        }
                                } else {
                                        if (act != 2) {
                                                HAGGLE_DBG("Blacklisting interface [%s - %s]\n", type, mac.c_str());
                                                addToBlacklist(iftype, (char *)&etha);
                                        } else {
                                                HAGGLE_DBG("NOT blacklisting interface [%s - %s] - already blacklisted\n", type, mac.c_str());
                                        }
                                }
                        }
                }
                
                blm = mc->getNextMetadata();
        }
}

void ConnectivityManager::addToBlacklist(InterfaceType_t type, const void *identifier)
{
        if (isBlacklisted(type, identifier))
                return;

        blMutex.lock();

        Interface *iface = new Interface(type, identifier);

        blacklist.push_back(iface);
        
        delete_interface(iface);
        blMutex.unlock();
}

bool ConnectivityManager::isBlacklisted(InterfaceType_t type, const void *identifier)
{
	Mutex::AutoLocker l(blMutex);

        for (List<Interface *>::iterator it = blacklist.begin(); it != blacklist.end(); it++) {
                if ((*it)->getType() == type && 
                    memcmp((*it)->getRawIdentifier(), identifier, (*it)->getIdentifierLen()) == 0)
                        return true;
        }
        return false;
}

bool ConnectivityManager::removeFromBlacklist(InterfaceType_t type, const void *identifier)
{
	Mutex::AutoLocker l(blMutex);

        for (List<Interface *>::iterator it = blacklist.begin(); it != blacklist.end(); it++) {
                if ((*it)->getType() == type && 
                    memcmp((*it)->getRawIdentifier(), identifier, (*it)->getIdentifierLen()) == 0) {
                        Interface *iface = *it;
                        blacklist.erase(it);
                        delete iface;
                        return true;
                }
        }
        return false;
}

#ifdef DEBUG
void ConnectivityManager::onDebugCmdEvent(Event *e)
{
	
	if (e->getDebugCmd()->getType() != DBG_CMD_PRINT_INTERNAL_STATE)
		return;
	
	kernel->getInterfaceStore()->print();
}
#endif /* DEBUG */

void ConnectivityManager::onDeleteConnectivity(Event *e)
{
	Connectivity *conn = (static_cast <Connectivity *>(e->getData()));

	// Take the connectivity out of the connectivity list
	connMutex.lock();
	conn_registry.remove(conn);

	// Delete the connectivity:
	delete conn;

	// Are we preparing for shutdown?
	if (getState() == MANAGER_STATE_PREPARE_SHUTDOWN) {
		// Are there any connectivities left?
		if (conn_registry.empty()) {
			signalIsReadyForShutdown();
			CM_DBG("ConnectivityManager is prepared for shutdown!\n");
		} else {
			CM_DBG("ConnectivityManager preparing for shutdown: %ld connectivities left\n", 
				conn_registry.size());
		}
	}
	connMutex.unlock();
}


void ConnectivityManager::spawn_connectivity(const InterfaceRef& iface)
{
        Mutex::AutoLocker l(connMutex);
	Connectivity *conn = NULL;
	
	// Is the shutdown procedure going?
	if (kernel->isShuttingDown())
		return;

	// Does the interface exist and is it a local interface?
	if (!iface || !iface->isLocal()) {
		HAGGLE_ERR("Trying to spawn connectivity on non-local interface\n");
		return;
	}

	connectivity_registry_t::iterator it = conn_registry.begin();

	for (; it != conn_registry.end() && conn == NULL; it++) {
		// Does this connectivity want the interface?
		if ((*it)->handleInterfaceUp(iface)) {
			// Yep. Don't start another one:
			conn = (*it);
		}
	}
	// Connectivity found?
	if (conn) {
		// Don't start another one:
		return;
	}
	
	// Create new connectivity module:
	switch (iface->getType()) {
#if defined(ENABLE_ETHERNET)
		case IFTYPE_ETHERNET:
		case IFTYPE_WIFI:
                        conn = new ConnectivityEthernet(this, iface);
			break;
#endif			
#if defined(ENABLE_BLUETOOTH)
		case IFTYPE_BLUETOOTH:
                        conn = new ConnectivityBluetooth(this, iface);
			break;
#endif
#if defined(ENABLE_MEDIA)
			// Nothing here yet.
		case IFTYPE_MEDIA:
                        conn = new ConnectivityMedia(this, iface);
			break;
#endif
		default:
			break;
	}
	
	if (conn) {
		// Start this connectivity:
		if (conn->startDiscovery()) {
			// Tell the connectivity what the current resource policy is:
			conn->setPolicy(kernel->getCurrentPolicy());
			// Success! Store the connectivity:
			conn_registry.push_back(conn);
		} else {
			// Failure: delete the connectivity:
			delete conn;
		}
	}
}

InterfaceStatus_t ConnectivityManager::report_known_interface(const Interface& iface, bool isHaggle)
{
	Mutex::AutoLocker l(ifMutex);
	InterfaceStats stats(isHaggle);

	Pair<known_interface_registry_t::iterator, bool> p = known_interface_registry.insert(make_pair(iface, stats));

	// The interface was not known since before.
	if (p.second)
		return INTERFACE_STATUS_UNKNOWN;

	(*p.first).second++;

	if (isHaggle)
		(*p.first).second.isHaggle = true;

	return (*p.first).second.isHaggle ? INTERFACE_STATUS_HAGGLE : INTERFACE_STATUS_OTHER;
}

InterfaceStatus_t ConnectivityManager::report_known_interface(const InterfaceType_t type, const char *identifier, bool isHaggle)
{
	Interface iface(type, identifier);

	return report_known_interface(iface, isHaggle);
}

InterfaceStatus_t ConnectivityManager::report_known_interface(const InterfaceRef& iface, bool isHaggle)
{
	InterfaceStatus_t ret;

	iface.lock();

	ret = report_known_interface(*iface.getObj(), isHaggle);

	iface.unlock();

	return ret;
}

/*
  This function makes sure an interface is in the table.
*/
InterfaceStatus_t ConnectivityManager::report_interface(const Interface *found, const InterfaceRef& found_by, ConnectivityInterfacePolicy *add_callback(void))
{
	bool was_added;
	
        if (!found || isBlacklisted(found->getType(), found->getRawIdentifier()))
                return INTERFACE_STATUS_NONE;

	InterfaceRef iface = kernel->getInterfaceStore()->addupdate(found, found_by, add_callback, &was_added);

	if (!iface || !was_added )
		return INTERFACE_STATUS_NONE;

	// Make sure the interface is up
	iface->up();

	CM_IFACE_DBG("%s interface [%s/%s] added/updated\n", 
		iface->isLocal() ? "Local" : "Neighbor", 
		iface->getIdentifierStr(), iface->getName());

	// Tell everyone about this new interface
	if(iface->isLocal()) {
		spawn_connectivity(iface);
		kernel->addEvent(new Event(EVENT_TYPE_LOCAL_INTERFACE_UP, iface));
	} else {
		kernel->addEvent(new Event(EVENT_TYPE_NEIGHBOR_INTERFACE_UP, iface));
	}
	return INTERFACE_STATUS_HAGGLE;
}

InterfaceStatus_t ConnectivityManager::report_interface(const InterfaceRef &found, const InterfaceRef& found_by, ConnectivityInterfacePolicy *add_callback(void))
{
	bool was_added;
	
        if (!found || isBlacklisted(found->getType(), found->getRawIdentifier()))
                return INTERFACE_STATUS_NONE;

	InterfaceRef iface = kernel->getInterfaceStore()->addupdate(found, found_by, add_callback, &was_added);

	if (!iface || !was_added)
		return INTERFACE_STATUS_NONE;

	// Make sure the interface is up
	iface->up();

	CM_IFACE_DBG("%s interface [%s/%s] added/updated\n", 
		iface->isLocal() ? "Local" : "Neighbor", 
		iface->getIdentifierStr(), iface->getName());

	// Tell everyone about this new interface
	if (iface->isLocal()) {
		spawn_connectivity(iface);
		kernel->addEvent(new Event(EVENT_TYPE_LOCAL_INTERFACE_UP, iface));
	} else {
		kernel->addEvent(new Event(EVENT_TYPE_NEIGHBOR_INTERFACE_UP, iface));
	}
	return INTERFACE_STATUS_HAGGLE;
}

static ConnectivityInterfacePolicy *onReceivedDataObject_helper(void)
{
	Timeval then(5);
	
	return new ConnectivityInterfacePolicyTime(then);
}

/*
  The connectivity manager watches all incoming data objects to see
  if the source of the received data object is not a registered
  neighbor.
*/
void ConnectivityManager::onReceivedDataObject(Event *e)
{
	bool res = false;

	if (!e || !e->hasData())
		return;

	DataObjectRef dObj = e->getDataObject();
	InterfaceRef localIface = dObj->getLocalInterface();
	InterfaceRef remoteIface = dObj->getRemoteInterface();

	if (!localIface) {
		HAGGLE_DBG("No local interface set on snooped data object, IGNORING\n");
		return;
	}
	if (!remoteIface) {
		HAGGLE_DBG("No remote interface set on snooped data object, IGNORING\n");
		return;
	}
	
	// Make sure the interface is marked as up.
	remoteIface->up();

	/* Check that there is a receive interface set and that it is
	 * not from an application socket */
	if (!remoteIface->isApplication()) {
		CM_IFACE_DBG("%s - DataObject received on interface [%s] from neighbor with interface [%s]\n", 
			   getName(), localIface->getIdentifierStr(), 
			   remoteIface->getIdentifierStr());

		// Check whether this interface is already registered or not
		if (!have_interface(remoteIface->getType(), remoteIface->getRawIdentifier())) {
			unsigned int timeout = 20;
			remoteIface->setFlag(IFFLAG_SNOOPED);
			report_interface(remoteIface, localIface, onReceivedDataObject_helper);
			report_known_interface(remoteIface, true);
			res = true;

			if (remoteIface->getType() == IFTYPE_BLUETOOTH)
				timeout = 120;

			// Set a longer timeout for Bluetooth since the device may not be verified
			// until we scan next time, which may be up to 2 minutes by default.
			// If the inteface would timeout before the device is detected for real,
			// it is anyhow not really a problem, since the device just goes away
			// and then reappears again.
			kernel->addEvent(new Event(garbageEType, remoteIface, timeout));
		}
	}
}

/*
	The connectivity manager watches all data objects that failed to send to
	detect interfaces that have gone down.
*/
void ConnectivityManager::onFailedToSendDataObject(Event *e)
{
	if (!e || !e->hasData())
		return;

	DataObjectRef dObj = e->getDataObject();
	InterfaceRef localIface = dObj->getLocalInterface();
	InterfaceRef remoteIface = dObj->getRemoteInterface();

	if (!localIface || !remoteIface)
		return;

	/* Check that there is a receive interface set and that it is
	 * not from an application socket */
	if (!remoteIface->isApplication()) {
		CM_IFACE_DBG("%s - DataObject failed to send on interface [%s] to neighbor with interface [%s]\n", 
			   getName(), localIface->getIdentifierStr(), 
			   remoteIface->getIdentifierStr());

		delete_interface(remoteIface);
	}
}

void ConnectivityManager::report_dead(const InterfaceRef &iface)
{
	iface->down();

	if (iface->isLocal()) {
		// This is a local interface - there should be a Connectivity associated
		// with it.
		
		synchronized(connMutex) {
                        connectivity_registry_t::iterator it = conn_registry.begin();
                        // Go through the entire registry:
                        for (; it != conn_registry.end(); it++) {
                                // Tell this interface to handle the interface going down:
                                (*it)->handleInterfaceDown(iface);
                        }
                }
		
		CM_IFACE_DBG("Local interface [%s/%s] deleted.", 
			     iface->getIdentifierStr(), iface->getName());
		
		// Tell the rest of haggle that this interface has gone down:
		kernel->addEvent(new Event(EVENT_TYPE_LOCAL_INTERFACE_DOWN, iface));
	} else {
		CM_IFACE_DBG("Neighbour interface [%s/%s] deleted.", 
			     iface->getIdentifierStr(), iface->getName());
		
		// Tell the rest of haggle that this interface has gone down:
		kernel->addEvent(new Event(EVENT_TYPE_NEIGHBOR_INTERFACE_DOWN, iface));	
	}
}

void ConnectivityManager::delete_interface(const InterfaceRef &iface)
{
	InterfaceRefList dead;
        
        kernel->getInterfaceStore()->remove(iface, &dead);

	while (!dead.empty()) {
		report_dead(dead.pop());
	}
}

void ConnectivityManager::delete_interface(const Interface *iface)
{
	InterfaceRefList dead;

        kernel->getInterfaceStore()->remove(iface, &dead);

	while (!dead.empty()) {
		report_dead(dead.pop());
	}
}

void ConnectivityManager::delete_interface(const InterfaceType_t type, const char *identifier)
{
	InterfaceRefList dead;
        
        kernel->getInterfaceStore()->remove(type, identifier, &dead);

	while (!dead.empty()) {
		report_dead(dead.pop());
	}
}

void ConnectivityManager::delete_interface(const string name)
{
	InterfaceRefList dead;
        
        kernel->getInterfaceStore()->remove(name, &dead);

	while (!dead.empty()) {
		report_dead(dead.pop());
	}
}

InterfaceStatus_t ConnectivityManager::have_interface(const Interface *iface)
{
	return kernel->getInterfaceStore()->stored(*iface) ? INTERFACE_STATUS_HAGGLE : INTERFACE_STATUS_NONE;
}

InterfaceStatus_t ConnectivityManager::have_interface(const InterfaceType_t type, const char *identifier)
{
	return kernel->getInterfaceStore()->stored(type, identifier) ? INTERFACE_STATUS_HAGGLE : INTERFACE_STATUS_NONE;
}

InterfaceStatus_t ConnectivityManager::is_known_interface(const InterfaceType_t type, const char *identifier)
{
	Mutex::AutoLocker l(ifMutex);
	Interface iface(type, identifier);

	known_interface_registry_t::iterator it = known_interface_registry.find(iface);

	if (it == known_interface_registry.end())
		return INTERFACE_STATUS_UNKNOWN;

	(*it).second++;

	return (*it).second.isHaggle ? INTERFACE_STATUS_HAGGLE : INTERFACE_STATUS_OTHER;
}

InterfaceStatus_t ConnectivityManager::is_known_interface(const Interface *iface)
{
	Mutex::AutoLocker l(ifMutex);
	known_interface_registry_t::iterator it = known_interface_registry.find(*iface);

	if (it == known_interface_registry.end())
		return INTERFACE_STATUS_UNKNOWN;

	(*it).second++;

	return (*it).second.isHaggle ? INTERFACE_STATUS_HAGGLE : INTERFACE_STATUS_OTHER;
}

void ConnectivityManager::on_GC_snooped_ifaces(Event *e)
{
	InterfaceRef ifaceRef = kernel->getInterfaceStore()->retrieve(e->getInterface());

        if (ifaceRef && ifaceRef->isSnooped()) {
                delete_interface(ifaceRef);
                
                CM_IFACE_DBG("Snooped interface [%s/%s] deleted because of timeout.", 
                             ifaceRef->getIdentifierStr(), 
                             ifaceRef->getName());
        }
}

void ConnectivityManager::age_interfaces(const InterfaceRef &whose)
{
	InterfaceRefList dead;
	
	kernel->getInterfaceStore()->age(whose, &dead);

	while (!dead.empty()) {
		report_dead(dead.pop());
	}
}

void ConnectivityManager::onNewPolicy(Event *e)
{
	Mutex::AutoLocker l(connMutex);
	PolicyRef pr;
	
	pr = e->getPolicy();
	
	for (connectivity_registry_t::iterator it = conn_registry.begin(); it != conn_registry.end(); it++) {
		(*it)->setPolicy(pr);
	}
}
