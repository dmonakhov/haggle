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
#include "ProtocolManager.h"
#include "Protocol.h"
#include "ProtocolUDP.h"
#include "ProtocolTCP.h"
#if defined(OS_UNIX)
#include "ProtocolLOCAL.h"
#endif
#if defined(ENABLE_MEDIA)
#include "ProtocolMedia.h"
#endif
#if defined(ENABLE_BLUETOOTH)
#include "ProtocolRFCOMM.h"
#endif
#include <haggleutils.h>

#define DISCOVERY_PORT 6666
#define PROTOCOL_GARBAGE_COLLECT_INTERVAL 30.0

ProtocolManager::ProtocolManager(HaggleKernel * _kernel) :
	Manager("ProtocolManager", _kernel)
{	
}

ProtocolManager::~ProtocolManager()
{
	while (!protocol_registry.empty()) {
		Protocol *p = (*protocol_registry.begin()).second;
		protocol_registry.erase(p->getId());
		delete p;
	}
}

bool ProtocolManager::init_derived()
{
	int ret;
#define __CLASS__ ProtocolManager

	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_SEND, onSendDataObject);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}

	ret = setEventHandler(EVENT_TYPE_LOCAL_INTERFACE_UP, onLocalInterfaceUp);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}

	ret = setEventHandler(EVENT_TYPE_LOCAL_INTERFACE_DOWN, onLocalInterfaceDown);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}

	delete_protocol_event = registerEventType("ProtocolManager protocol deletion event", onDeleteProtocolEvent);

	add_protocol_event = registerEventType("ProtocolManager protocol addition event", onAddProtocolEvent);
	
	send_data_object_actual_event = registerEventType("ProtocolManager send data object actual event", onSendDataObjectActual);
#ifdef DEBUG
	ret = setEventHandler(EVENT_TYPE_DEBUG_CMD, onDebugCmdEvent);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}
#endif
	return true;
}

#ifdef DEBUG
void ProtocolManager::onDebugCmdEvent(Event *e)
{	
	if (e->getDebugCmd()->getType() != DBG_CMD_PRINT_PROTOCOLS)
		return;

	protocol_registry_t::iterator it;

	for (it = protocol_registry.begin(); it != protocol_registry.end(); it++) {
		Protocol *p = (*it).second;

		printf("Protocol \'%s\':\n", p->getName());

		printf("\tcommunication interfaces: %s <-> %s\n", 
                       p->getLocalInterface() ? p->getLocalInterface()->getIdentifierStr() : "None",  
                       p->getPeerInterface() ?  p->getPeerInterface()->getIdentifierStr() : "None");
		if (p->getQueue() && p->getQueue()->size()) {
			printf("\tQueue:\n");
			p->getQueue()->print();
		} else {
			printf("\tQueue: empty\n");
		}
	}
}
#endif /* DEBUG */

void ProtocolManager::onAddProtocolEvent(Event *e)
{	
	registerProtocol(static_cast<Protocol *>(e->getData()));
}

bool ProtocolManager::registerProtocol(Protocol *p)
{
	protocol_registry_t::iterator it;

	if (!p || protocol_registry.insert(make_pair(p->getId(), p)).second == false) {
		HAGGLE_ERR("Protocol %s already registered!\n", p->getName());
		return false;
	}

	p->setRegistered();

	return true;
}

void ProtocolManager::onDeleteProtocolEvent(Event * e)
{
	Protocol *p;

	if (!e)
		return;

	p = static_cast < Protocol * >(e->getData());

	if (!p)
		return;
		
	if (protocol_registry.find(p->getId()) == protocol_registry.end()) {
		HAGGLE_ERR("Trying to deregister unregistered protocol %s\n", p->getName());
		return;
	}

	protocol_registry.erase(p->getId());
	
	HAGGLE_DBG("Deleting protocol %s\n", p->getName());

	delete p;

	if (getState() == MANAGER_STATE_SHUTDOWN) {
		if (protocol_registry.empty()) {
			unregisterWithKernel();
		} 
#if defined(DEBUG)
		else {
			for (protocol_registry_t::iterator it = protocol_registry.begin(); it != protocol_registry.end(); it++) {
				Protocol *p = (*it).second;

				HAGGLE_DBG("Protocol \'%s\' still registered\n", p->getName());
			}

		}
#endif
	}
}

/*
	Do not stop protocols until we are in shutdown, because other managers
	may rely on protocol services while they are preparing for shutdown.
 */
void ProtocolManager::onShutdown()
{
	if (protocol_registry.empty()) {
		unregisterWithKernel();
	} else {
		// Go through the registered protocols
		protocol_registry_t::iterator it = protocol_registry.begin();

		for (; it != protocol_registry.end(); it++) {
			// Tell this protocol we're shutting down!
			(*it).second->shutdown();
		}
	}
}

#if defined(OS_WINDOWS_XP) && !defined(DEBUG)
// This is here to avoid a warning with catching the exception in the functions
// below.
#pragma warning( push )
#pragma warning( disable: 4101 )
#endif

class LocalByTypeCriteria : public InterfaceStore::Criteria
{
	InterfaceType_t type;
public:
	LocalByTypeCriteria(InterfaceType_t _type) : type(_type) {}
	virtual bool operator() (const InterfaceRecord& ir) const
	{
		if (ir.iface->getType() == type && ir.iface->isLocal() && ir.iface->isUp())
			return true;
		return false;
	}
};

class PeerParentCriteria : public InterfaceStore::Criteria
{
	InterfaceRef parent;
	InterfaceRef peerIface;
public:
	PeerParentCriteria(const InterfaceRef& iface) : parent(NULL), peerIface(iface) {}
	virtual bool operator() (const InterfaceRecord& ir)
	{
		if (parent && ir.iface && parent == ir.iface && ir.iface->isUp())
			return true;

		if (ir.iface == peerIface)
			parent = ir.parent;

		return false;
	}
};

Protocol *ProtocolManager::getSenderProtocol(const ProtType_t type, const InterfaceRef& peerIface)
{
	Protocol *p = NULL;
	protocol_registry_t::iterator it = protocol_registry.begin();
	InterfaceRef localIface = NULL;
	InterfaceRefList ifl;

	// Go through the list of current protocols until we find one:
	for (; it != protocol_registry.end(); it++) {
		p = (*it).second;

		// Is this protocol the one we're interested in?
		if (p->isSender() && type == p->getType() && p->isForInterface(peerIface)) {
			break;
		}
		
		p = NULL;
	}
	
	// Did we find a protocol?
	if (p == NULL) {
		// Nope. Find a suitable local interface to associate with the protocol
		kernel->getInterfaceStore()->retrieve(PeerParentCriteria(peerIface), ifl);
		
		// Parent interface found?
		if (ifl.size() != 0) {
			// Create a new one:
			switch (type) {
#if defined(ENABLE_BLUETOOTH)
				case PROT_TYPE_RFCOMM:
					// We always grab the first local Bluetooth interface
					p = new ProtocolRFCOMMSender(ifl.pop(), peerIface, RFCOMM_DEFAULT_CHANNEL, this);
					break;
#endif				
				case PROT_TYPE_TCP:
					// TODO: should retrieve local interface by querying for the parent interface of the peer.
					p = new ProtocolTCPSender(ifl.pop(), peerIface, TCP_DEFAULT_PORT, this);
					break;                           
				case PROT_TYPE_LOCAL:
					// FIXME: shouldn't be able to get here!
					HAGGLE_DBG("No local sender protocol running!\n");
					break;
				case PROT_TYPE_UDP:
					// FIXME: shouldn't be able to get here!
					HAGGLE_DBG("No UDP sender protocol running!\n");
					break;
#if defined(ENABLE_MEDIA)
				case PROT_TYPE_MEDIA:
					p = new ProtocolMedia(NULL, peerIface, this);
					break;
#endif
				default:
					HAGGLE_DBG("Unable to create client sender protocol for type %ld\n", type);
					break;
			}
		}
		// Were we successful?
		if (p != NULL) {
			if (p->init()) {
				// Put it in the list
				registerProtocol(p);
			} else {
				HAGGLE_ERR("Could not initialize protocol %s\n", p->getName());
				delete p;
				p = NULL;
			}
		}
	}
	// Return any found or created protocol:
	return p;
}

// FIXME: seems to be unused. Delete?
Protocol *ProtocolManager::getReceiverProtocol(const ProtType_t type, const InterfaceRef& iface)
{
	Protocol *p = NULL;
	protocol_registry_t::iterator it = protocol_registry.begin();

	// For an explanation of how this function works, see getSenderProtocol
	for (; it != protocol_registry.end(); it++) {
		p = (*it).second;

		if (p->isReceiver() && type == p->getType() && p->isForInterface(iface)) {
			if (type == PROT_TYPE_UDP || type == PROT_TYPE_LOCAL)
				break;
			else if (p->isConnected())
				break;
		}

		p = NULL;
	}

	if (p == NULL) {
		switch (type) {
#if defined(ENABLE_BLUETOOTH)
                        case PROT_TYPE_RFCOMM:
				// FIXME: write a correct version of this line:
				p = NULL;//new ProtocolRFCOMMReceiver(0, iface, RFCOMM_DEFAULT_CHANNEL, this);
			break;
#endif
                        case PROT_TYPE_TCP:
				// FIXME: write a correct version of this line:
				p = NULL;//new ProtocolTCPReceiver(0, (struct sockaddr *) NULL, iface, this);
			break;

#if defined(ENABLE_MEDIA)
		case PROT_TYPE_MEDIA:
			// does not apply to protocol media
			break;
#endif

		default:
			HAGGLE_DBG("Unable to create client receiver protocol for type %ld\n", type);
			break;
		}
		if (p != NULL) {
			if (p->init()) {
				registerProtocol(p);
			} else {
				HAGGLE_ERR("Could not initialize protocol %s\n", p->getName());
				delete p;
				p = NULL;
			}
		}
	}

	return p;
}

Protocol *ProtocolManager::getServerProtocol(const ProtType_t type, const InterfaceRef& iface)
{
	Protocol *p = NULL;
	protocol_registry_t::iterator it = protocol_registry.begin();

	// For an explanation of how this function works, see getSenderProtocol

	for (; it != protocol_registry.end(); it++) {
		p = (*it).second;

		if (p->isServer() && type == p->getType() && p->isForInterface(iface))
			break;
		
		p = NULL;
	}

	if (p == NULL) {
		switch (type) {
#if defined(ENABLE_BLUETOOTH)
                        case PROT_TYPE_RFCOMM:
				p = new ProtocolRFCOMMServer(iface, this);
                                break;
#endif
                        case PROT_TYPE_TCP:
                                p = new ProtocolTCPServer(iface, this);
                                break;
                                
#if defined(ENABLE_MEDIA)
			case PROT_TYPE_MEDIA:
                                //if (!strstr(iface->getMacAddrStr(), "00:00:00:00")) {
                                //						// mac address is 0: dummy interface (for unmount) > no protocol needed
//						p = new ProtocolMediaServer(iface, this);
                                //					}
				break;
#endif
		default:
			HAGGLE_DBG("Unable to create server protocol for type %ld\n", type);
			break;
		}
		if (p != NULL) {
			if (p->init()) {
				registerProtocol(p);
			} else {
				HAGGLE_ERR("Could not initialize protocol %s\n", p->getName());
				delete p;
				p = NULL;
			}
		}
	}

	return p;
}

#if defined(OS_WINDOWS_XP) && !defined(DEBUG)
#pragma warning( pop )
#endif

void ProtocolManager::onWatchableEvent(const Watchable& wbl)
{
	protocol_registry_t::iterator it = protocol_registry.begin();

	HAGGLE_DBG("Receive on %s\n", wbl.getStr());

	// Go through each protocol in turn:
	for (; it != protocol_registry.end(); it++) {
		Protocol *p = (*it).second;

		// Did the Watchable belong to this protocol
		if (p->hasWatchable(wbl)) {
			// Let the protocol handle whatever happened.
			p->handleWatchableEvent(wbl);
			return;
		}
	}

	HAGGLE_DBG("Was asked to handle a socket no protocol knows about!\n");
	// Should not happen, but needs to be dealt with because if it isn't,
	// the kernel will call us again in an endless loop!

	kernel->unregisterWatchable(wbl);

	CLOSE_SOCKET(wbl.getSocket());
}

void ProtocolManager::onLocalInterfaceUp(Event *e)
{
	if (!e)
		return;

	InterfaceRef iface = e->getInterface();

	if (!iface)
		return;

	const Addresses *adds = iface->getAddresses();
	
	for (Addresses::const_iterator it = adds->begin() ; it != adds->end() ; it++) {
		switch((*it)->getType()) {
			case AddressType_IPv4:
#if defined(ENABLE_IPv6)
			case AddressType_IPv6:
#endif
				getServerProtocol(PROT_TYPE_TCP, iface);
				return;
			break;
			
#if defined(ENABLE_BLUETOOTH)
			case AddressType_BTMAC:
				getServerProtocol(PROT_TYPE_RFCOMM, iface);
				return;
			break;
#endif			
#if defined(ENABLE_MEDIA)
			// FIXME: should probably separate loop interfaces from media interfaces somehow...
			case AddressType_FilePath:
				getServerProtocol(PROT_TYPE_MEDIA, iface);
				return;
			break;
#endif
			
			default:
			break;
		}
	}
	
	HAGGLE_DBG("Interface with no known address type - no server started\n");
}

void ProtocolManager::onLocalInterfaceDown(Event *e)
{
	InterfaceRef& iface = e->getInterface();

	if (!iface)
		return;

	// Go through the protocol list
	protocol_registry_t::iterator it = protocol_registry.begin();

	for (;it != protocol_registry.end(); it++) {
		Protocol *p = (*it).second;

		/* 
		Never bring down our application IPC protocol when
		application interfaces go down (i.e., applications deregister).
		*/
		if (p->getLocalInterface()->getType() == IFTYPE_APPLICATION_PORT) {
			continue;
		}
		// Is the associated with this protocol?
		if (p->isForInterface(iface)) {
			/*
			   NOTE: I am unsure about how this should be done. Either:

			   p->handleInterfaceDown();

			   or:

			   protocol_list_mutex->unlock();
			   p->handleInterfaceDown();
			   return;

			   or:

			   protocol_list_mutex->unlock();
			   p->handleInterfaceDown();
			   protocol_list_mutex->lock();
			   (*it) = protocol_registry.begin();

			   The first has the benefit that it doesn't assume that there is
			   only one protocol per interface, but causes the deletion of the
			   interface to happen some time in the future (as part of event
			   queue processing), meaning the protocol will still be around,
			   and may be given instructions before being deleted, even when
			   it is incapable of handling instructions, because it should
			   have been deleted.

			   The second has the benefit that if the protocol tries to have
			   itself deleted, it happens immediately (because there is no
			   deadlock), but also assumes that there is only one protocol per
			   interface, which may or may not be true.

			   The third is a mix, and has the pros of both the first and the
			   second, but has the drawback that it repeatedly locks and
			   unlocks the mutex, and also needs additional handling so it
			   won't go into an infinite loop (a set of protocols that have
			   already handled the interface going down, for instance).

			   For now, I've chosen the first solution.
			 */
			// Tell the protocol to handle this:
			HAGGLE_DBG("Shutting down protocol %s because interface %s went down\n",
				p->getName(), iface->getIdentifierStr());
			p->handleInterfaceDown(iface);
		}
	}
}

void ProtocolManager::onSendDataObject(Event *e)
{
	/*
		Since other managers might want to modify the data object before it is
		actually sent, we delay the actual send processing by sending a private
		event to ourselves, effectively rescheduling our own processing of this
		event to occur just after this event.
	*/
	
	kernel->addEvent(new Event(send_data_object_actual_event, e->getDataObject(), e->getNodeList()));
}

void ProtocolManager::onSendDataObjectActual(Event *e)
{
	int numTx = 0;

	if (!e || !e->hasData())
		return;

	// Get a copy to work with
	DataObjectRef dObj = e->getDataObject();

	// Get target list:
	NodeRefList *targets = (e->getNodeList()).copy();

	if (!targets) {
		HAGGLE_ERR("no targets in data object when sending\n");
		return;
	}

	unsigned int numTargets = targets->size();

	// Go through all targets:
	while (!targets->empty()) {
		
		// A current target reference
		NodeRef targ = targets->pop();
		
		if (!targ) {
			HAGGLE_ERR("Target num %u is NULL!\n", numTargets);
			numTargets--;
			continue;
		}

		HAGGLE_DBG("Sending to target %u - %s \n", numTargets, targ->getName().c_str());
		
		// If we are going to loop through the node's interfaces, we need to lock the node.
		targ.lock();	
		
		const InterfaceRefList *interfaces = targ->getInterfaces();
		
		// Are there any interfaces here?
		if (interfaces == NULL || interfaces->size() == 0) {
			// No interfaces for target, so we generate a
			// send failure event and skip the target
		
			HAGGLE_DBG("Target %s has no interfaces\n", targ->getName().c_str());

			targ.unlock();

			kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND_FAILURE, dObj, targ));
			numTargets--;
			continue;
		}
		
		/*	
			Find the target interface that suits us best
			(we assume that for any remote target
			interface we have a corresponding local interface).
		*/
		InterfaceRef peerIface = NULL;
		bool done = false;
		
		InterfaceRefList::const_iterator it = interfaces->begin();
		
		//HAGGLE_DBG("Target node %s has %lu interfaces\n", targ->getName().c_str(), interfaces->size());

		for (; it != interfaces->end() && done == false; it++) {
			InterfaceRef iface = *it;
			
			// If this interface is up:
			if (iface->isUp()) {
				
				if (iface->getAddresses()->empty()) {
					HAGGLE_DBG("Interface %s:%s has no addresses - IGNORING.\n",
						   iface->getTypeStr(), iface->getIdentifierStr());
					continue;
				}
				
				switch (iface->getType()) {
#if defined(ENABLE_BLUETOOTH)
				case IFTYPE_BLUETOOTH:
					/*
					  Select Bluetooth only if there are no Ethernet or WiFi
					  interfaces.
					*/
					if (!iface->getAddressByType(AddressType_BTMAC)) {
						HAGGLE_DBG("Interface %s:%s has no Bluetooth address - IGNORING.\n",
							   iface->getTypeStr(), iface->getIdentifierStr());
						break;
					}
					
					if (!peerIface)
						peerIface = iface;
					else if (peerIface->getType() != IFTYPE_ETHERNET &&
						 peerIface->getType() != IFTYPE_WIFI)
						peerIface = iface;
					break;
#endif
#if defined(ENABLE_ETHERNET)
				case IFTYPE_ETHERNET:
					/*
					  Let Ethernet take priority over the other types.
					*/
					if (!iface->getAddressByType(AddressType_IPv4)
#if defined(ENABLE_IPv6)
					    && !iface->getAddressByType(AddressType_IPv6)
#endif
						) {
						HAGGLE_DBG("Interface %s:%s has no IPv4 or IPv6 addresses - IGNORING.\n",
							   iface->getTypeStr(), iface->getIdentifierStr());
						break;
					}
					if (!peerIface)
						peerIface = iface;
					else if (peerIface->getType() == IFTYPE_BLUETOOTH ||
						 peerIface->getType() == IFTYPE_WIFI)
						peerIface = iface;
					break;
				case IFTYPE_WIFI:
					if (!iface->getAddressByType(AddressType_IPv4) 
#if defined(ENABLE_IPv6)
					    && !iface->getAddressByType(AddressType_IPv6)
#endif
						) {
						HAGGLE_DBG("Interface %s:%s has no IPv4 or IPv6 addresses - IGNORING.\n",
							   iface->getTypeStr(), iface->getIdentifierStr());
						break;
					}
					if (!peerIface)
						peerIface = iface;
					else if (peerIface->getType() == IFTYPE_BLUETOOTH &&
						 peerIface->getType() != IFTYPE_ETHERNET)
						peerIface = iface;
					break;
#endif
				case IFTYPE_APPLICATION_PORT:
				case IFTYPE_APPLICATION_LOCAL:
                                        
					if (!iface->getAddressByType(AddressType_IPv4) 
#if defined(ENABLE_IPv6)
					    && !iface->getAddressByType(AddressType_IPv6)
#endif
						) {
						HAGGLE_DBG("Interface %s:%s has no IPv4 or IPv6 addresses - IGNORING.\n",
							   iface->getTypeStr(), iface->getIdentifierStr());
						break;
					}
					// Not much choise here.
					if (targ->getType() == NODE_TYPE_APPLICATION) {
						peerIface = iface;
						done = true;
					} else {
						HAGGLE_DBG("ERROR: Node %s is not application, but its interface is\n",
							targ->getName().c_str());
					}
                                        
					break;
				case IFTYPE_MEDIA:
#if defined(ENABLE_MEDIA)
					
#endif
					break;
				case IFTYPE_UNDEF:
				default:
					break;
				}
			} else {
				//HAGGLE_DBG("Send interface %s was down, ignoring...\n", iface->getIdentifierStr());
			}
		}
		
		// We are done looking for a suitable send interface
		// among the node's interface list, so now we unlock
		// the node.
		targ.unlock();
		
		if (!peerIface) {
			HAGGLE_DBG("No send interface found for target %s. Aborting send of data object!!!\n", 
				targ->getName().c_str());
			// Failed to send to this target, send failure event:
			kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND_FAILURE, dObj, targ));
			numTargets--;
			continue;
		}

		// Ok, we now have a target and a suitable interface,
		// now we must figure out a protocol to use when we
		// transmit to that interface
		Protocol *p = NULL;
		
		// We make a copy of the addresses list here so that we do not
		// have to lock the peer interface while we call getSenderProtocol().
		// getSenderProtocol() might do a lookup in the interface store in order
		// to find the local interface which is parent of the peer interface.
		// This might cause a deadlock in case another thread also does a lookup
		// in the interface store while we hold the interface lock.
		const Addresses *adds = peerIface->getAddresses()->copy();
		
		// Figure out a suitable protocol given the addresses associated 
		// with the selected interface
		for (Addresses::const_iterator it = adds->begin(); p == NULL && it != adds->end(); it++) {
			
			switch ((*it)->getType()) {
#if defined(ENABLE_BLUETOOTH)
			case AddressType_BTMAC:
				p = getSenderProtocol(PROT_TYPE_RFCOMM, peerIface);
				break;
#endif
			case AddressType_IPv4:
#if defined(ENABLE_IPv6)
			case AddressType_IPv6:
#endif
				if (peerIface->isApplication()) {
#ifdef USE_UNIX_APPLICATION_SOCKET
					p = getSenderProtocol(PROT_TYPE_LOCAL, peerIface);
#else
					p = getSenderProtocol(PROT_TYPE_UDP, peerIface);
#endif
				}
				else
					p = getSenderProtocol(PROT_TYPE_TCP, peerIface);
				break;
#if defined(ENABLE_MEDIA)
			case AddressType_FilePath:
				p = getSenderProtocol(PROT_TYPE_MEDIA, peerIface);
				break;
#endif
			default:
				break;
			}
		}
		
		delete adds;
		
                // Send data object to the found protocol:

                if (p) {
			if (p->sendDataObject(dObj, targ, peerIface)) {
				numTx++;
			} else {
				// Failed to send to this target, send failure event:
                                kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND_FAILURE, dObj, targ));
			}
		} else {
			HAGGLE_DBG("No suitable protocol found for interface %s:%s!\n", 
				   peerIface->getTypeStr(), peerIface->getIdentifierStr());
			// Failed to send to this target, send failure event:
			kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND_FAILURE, dObj, targ));			
		}

		numTargets--;
	}
	HAGGLE_DBG("Scheduled %d data objects for sending\n", numTx);

	delete targets;
}
