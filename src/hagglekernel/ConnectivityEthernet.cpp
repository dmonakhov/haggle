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

#if defined(ENABLE_ETHERNET)

#include "ConnectivityEthernet.h"

#include <stdio.h>
#include <string.h>

#if defined(OS_LINUX) || defined(OS_MACOSX)
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/time.h>
#include <signal.h>
#endif
#if defined(OS_MACOSX_IPHONE)
#include <net/if_dl.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(OS_MACOSX)
#include <net/route.h>
#include <net/if_dl.h>
#include <sys/kern_event.h>
#include <sys/types.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>
#elif defined(OS_LINUX)
#include <unistd.h>
#include <linux/ip.h>
//#include <net/ethernet.h>
#elif defined(OS_WINDOWS)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>
#include <winioctl.h>
#include <winerror.h>
#include <iphlpapi.h>
#endif

#include <libcpphaggle/Watch.h>

// To get the TCP_DEFAULT_PORT macro:
#include "ProtocolTCP.h"

#if defined(ENABLE_IPv6)
#define SOCKADDR_SIZE sizeof(struct sockaddr_in6)
#else
#define SOCKADDR_SIZE sizeof(struct sockaddr_in)
#endif

class ConnEthIfaceListElement {
public:
	InterfaceRef iface;
	SOCKET broadcastSocket;
	socklen_t broadcast_addr_len;
	union {
		struct sockaddr broadcast_addr;
		struct sockaddr_in broadcast_addr4;
#if defined(ENABLE_IPv6)
		struct sockaddr_in6 broadcast_addr6;
#endif
	};
	struct haggle_beacon broadcast_packet;
	
	ConnEthIfaceListElement(const InterfaceRef &_iface);
	~ConnEthIfaceListElement();
};

ConnEthIfaceListElement::ConnEthIfaceListElement(const InterfaceRef &_iface) :
	iface(_iface)
{
#if defined(ENABLE_IPv6)
	struct sockaddr_in6 my_addr6;
#endif
	struct sockaddr_in my_addr4;
	struct sockaddr	*my_addr = NULL;
	socklen_t my_addr_len = -1;
	bool has_broadcast = false;
	Address *addr;
	int on = 1;
	
	addr = iface->getAddressByType(AddressType_EthMAC);
#if HAVE_EXCEPTION
	if (!addr)
		throw Exception(0, "No ethernet MAC address in interface!");
#endif
	// Initialize:
	memset(&broadcast_packet, 0, sizeof(broadcast_packet));
	
	memcpy(broadcast_packet.mac, addr->getRaw(), addr->getLength());
	
#if defined(ENABLE_IPv6)
	// Prefer IPv6 addresses:
	addr = iface->getAddressByType(AddressType_IPv6);

	if (addr != NULL) {
		if(addr->hasBroadcast()) {
			has_broadcast = true;
			
			// Fill in the socket address for the broadcasting socket:
			my_addr = (struct sockaddr *) &my_addr6;
			
			my_addr_len = addr->fillInSockaddr(my_addr);
			my_addr6.sin6_port = htons(0);
			
			// Fill in the broadcast address to send to:
			broadcast_addr_len = 
				addr->fillInBroadcastSockaddr(
					&broadcast_addr, 
					HAGGLE_UDP_CONNECTIVITY_PORT);
		}
	}
#endif
	
	// Fallback: IPv4 address:
	if (!has_broadcast) {
		addr = iface->getAddressByType(AddressType_IPv4);
                
		if (addr != NULL) {
			if (addr->hasBroadcast()) {
				has_broadcast = true;
				
				// Fill in the socket address for the broadcasting socket:
				my_addr = (struct sockaddr *) &my_addr4;
				
				my_addr_len = addr->fillInSockaddr(my_addr);
				my_addr4.sin_port = htons(0);
				
				// Fill in the broadcast address to send to:
				broadcast_addr_len = addr->fillInBroadcastSockaddr(&broadcast_addr, 
                                                                              HAGGLE_UDP_CONNECTIVITY_PORT);
			}
		}
	}
	
#if HAVE_EXCEPTION
	if(!has_broadcast)
		throw Exception(0, "No broadcast address found\n");
#endif
	// Open a UDP socket:
	broadcastSocket = socket(AF_INET, SOCK_DGRAM, 0);

#if HAVE_EXCEPTION
	if (broadcastSocket == INVALID_SOCKET)
		throw Exception(0, "Could not open socket\n");
#endif
	if (setsockopt(broadcastSocket, SOL_SOCKET, SO_BROADCAST, (const char *) &on, sizeof(on)) == -1) {
		CLOSE_SOCKET(broadcastSocket);
#if HAVE_EXCEPTION
		throw Exception(0, "Could not set broadcast socketopt\n");
#endif
	}
#if defined(OS_LINUX)
#if defined(OS_ANDROID)
        // On Android we run with root privileges, so we can use a
        // higher priority on that platform
        int priority = IPTOS_LOWDELAY;
#else
	int priority = 6;
#endif
	if (setsockopt(broadcastSocket, SOL_SOCKET, SO_PRIORITY, (const char *) &priority, sizeof(priority)) == -1) {
		CLOSE_SOCKET(broadcastSocket);
#if HAVE_EXCEPTION
		throw Exception(0, "Could not set SO_PRIORITY socketopt\n");
#endif
	}
#endif
	// Bind the socket to the address:
	if (bind(broadcastSocket, my_addr, my_addr_len) == -1) {
		CLOSE_SOCKET(broadcastSocket);
#if HAVE_EXCEPTION
		throw Exception(0, "Could not bind socket\n");
#endif
	}
}

ConnEthIfaceListElement::~ConnEthIfaceListElement()
{
	CLOSE_SOCKET(broadcastSocket);
}

ConnectivityEthernet::ConnectivityEthernet(ConnectivityManager * m, const InterfaceRef& iface) :
	Connectivity(m, "Ethernet connectivity"), 
	ifaceListMutex("ConnEth:IfaceListMutex"), 
	seqno(0),
	beaconInterval(15)
{
	int on = 1;
	struct sockaddr_in my_addr;

	// Add the first interface to the broadcasting list:
	if (!handleInterfaceUp(iface)) {
#if HAVE_EXCEPTION
		throw ConnectivityException(0, "Unable to open first broadcasting socket!\n");
#else
                HAGGLE_ERR("Could not set interface to up\n");
                return;
#endif	
        }
	// FIXME: should support IPv6 here.
	memset(&my_addr, 0, sizeof(struct sockaddr_in));
	// Set up local port:
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = INADDR_ANY;
	//memcpy(&my_addr.sin_addr.s_addr, iface->getIPConfig()->getIP(), sizeof(struct in_addr));
	my_addr.sin_port = htons(HAGGLE_UDP_CONNECTIVITY_PORT);
	
	const Address addr((struct sockaddr *)&my_addr);

	rootInterfacePtr = new Interface(IFTYPE_ETHERNET, 
                                         // FIXME: Hotfix for hotfix: use the MAC address of the first interface, rather than a bogus MAC address.
                                         // not good, but works for now.
                                         iface->getRawIdentifier(), 
                                         &addr, 
                                         "Root ethernet connectivity interface",
                                         IFFLAG_UP|IFFLAG_LOCAL);
	rootInterface = rootInterfacePtr;
	
	// Open a UDP socket:
	listenSock = socket(AF_INET, SOCK_DGRAM, 0);

#if HAVE_EXCEPTION
	if (listenSock == INVALID_SOCKET)
		throw ConnectivityException(0, "Could not open socket\n");
#endif
	// Allow reuse of this address:
	if (setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on)) == -1) {
		CLOSE_SOCKET(listenSock);
#if HAVE_EXCEPTION
		throw ConnectivityException(0, "Could not set SO_REUSEADDR socketopt\n");
#endif
	}
	// Bind the socket to the address:
	if (bind(listenSock, (struct sockaddr *) &my_addr, sizeof(my_addr)) == -1) {
		CLOSE_SOCKET(listenSock);
#if HAVE_EXCEPTION
		throw ConnectivityException(0, "Could not bind socket\n");
#endif
	}
}

ConnectivityEthernet::~ConnectivityEthernet()
{
	while (!ifaceList.empty()) {
		ConnEthIfaceListElement	*elem;
		
		elem = ifaceList.front();
		ifaceList.pop_front();
		delete elem;
	}
	CLOSE_SOCKET(listenSock);
}

bool ConnectivityEthernet::handleInterfaceUp(const InterfaceRef &iface)
{
        Mutex::AutoLocker l(ifaceListMutex);
	ConnEthIfaceListElement	*new_elem = NULL;
	
	if (!(iface->getType() == IFTYPE_ETHERNET ||
		 iface->getType() == IFTYPE_WIFI))
		return false;
	
	// HOTFIX: make sure not to add the ethernet root interface to the list
	// of scanned interfaces.
	if (iface == rootInterface)
		return true;
	
	// Check that we don't already have this interface:
	for (List<ConnEthIfaceListElement *>::iterator it = ifaceList.begin(); it != ifaceList.end(); it++) {
		if ((*it)->iface == iface)
			return true;
        }
	
	// Create a new element to hold this interface:

	new_elem = new ConnEthIfaceListElement(iface);

	if (new_elem != NULL) {
		// Insert into list:
		ifaceList.push_front(new_elem);
	}
	
	return true;
}

void ConnectivityEthernet::handleInterfaceDown(const InterfaceRef &iface)
{
        bool is_empty = true;

	if (!(iface->getType() == IFTYPE_ETHERNET ||
		 iface->getType() == IFTYPE_WIFI))
		return;
	
	synchronized(ifaceListMutex) {
                // Find matching interface (if any):
                for (List<ConnEthIfaceListElement *>::iterator it = ifaceList.begin(); it != ifaceList.end(); it++) {
                        if ((*it)->iface == iface) {
                                ConnEthIfaceListElement	*old_elem = (*it);
                                
                                // remove interface:
                                ifaceList.remove(old_elem);
                                delete old_elem;
                                break;
                        }
                }
                // Check if the list is empty (while it is locked):
                is_empty = ifaceList.empty();
        }
	
	// If there weren't any interfaces left:
	if (is_empty) {
		// Stop this connectivity:
		cancelDiscovery();
        }
}

void ConnectivityEthernet::hookCleanup()
{
	
}

void ConnectivityEthernet::cancelDiscovery(void)
{
	hookStopOrCancel();
	cancel();
}

void ConnectivityEthernet::setPolicy(PolicyRef newPolicy)
{
	PolicyType_t pol;
	
	if (!newPolicy)
		return;
	
	pol = newPolicy->getPowerPolicy();
	
	switch (pol) {
		case POLICY_RESOURCE_UNLIMITED:
			beaconInterval = 2;
		break;
		
		case POLICY_RESOURCE_HIGH:
			beaconInterval = 5;
		break;
		
		case POLICY_RESOURCE_MEDIUM:
			beaconInterval = 10;
		break;
		
		case POLICY_RESOURCE_LOW:
			beaconInterval = 15;
		break;
	}
}


// This is jitter in the range [ -1000000 : 1000000 ] microseconds.

#define BEACON_JITTER ((prng_uint32() % 2000000) - 1000000)

bool ConnectivityEthernet::run()
{
	Watch w;
	int socketIndex, waitRet = 0; 
#define BUFLEN 200
	char buffer[BUFLEN];
	struct haggle_beacon *beacon = (struct haggle_beacon *) buffer;
	Timeval next_beacon_time, next_beacon_time_unjittered;
	u_int32_t prev_seqno = seqno+1;
	
	/*
		HOTFIX: report the root interface as being local and existing.
		This is necessary because the interface store needs a parent interface
		to be in the store if ageing is to work.
	*/
	report_interface(rootInterfacePtr, NULL, newConnectivityInterfacePolicyAgeless);
	
	socketIndex = w.add(listenSock); 
	
	next_beacon_time = Timeval::now();
	next_beacon_time_unjittered = next_beacon_time;
		
	while (!shouldExit()) {
		Timeval timeout;

		// Be explicit
		timeout.zero();

		if (!w.getRemainingTime(&timeout)) {
			timeout = Timeval::now();
			
			// Has the timeout expired, and a new beacon been sent?
			if (timeout > next_beacon_time && 
				prev_seqno != seqno)
			{
				// Reset the "beacon sent" test:
				prev_seqno = seqno;
				// Move the next beacon time into the future:
				while (timeout > next_beacon_time_unjittered)
					next_beacon_time_unjittered += beaconInterval;
				// Add jitter to the timeout:
				next_beacon_time = 
					next_beacon_time_unjittered + Timeval(0, BEACON_JITTER);
			}
			
			timeout = next_beacon_time - timeout;
		}
		
		w.reset();

		waitRet = w.wait(&timeout); 

		if (waitRet == Watch::TIMEOUT) {
			InterfaceRefList downedIfaces;
			int ret;

			// Timeout --> send next beacon
			seqno++;
			
			synchronized(ifaceListMutex) {
                                List<ConnEthIfaceListElement *>::iterator it = ifaceList.begin();
                                
                                for (;it != ifaceList.end(); it++) {
                                        (*it)->broadcast_packet.seqno = htonl(seqno);
                                        ret = sendto((*it)->broadcastSocket, 
                                                     (const char *) &((*it)->broadcast_packet), 
                                                     HAGGLE_BEACON_LEN, 
                                                     MSG_DONTROUTE, 
                                                     &((*it)->broadcast_addr), 
                                                     (*it)->broadcast_addr_len);
                                        
                                        if (ret < 0) {
                                                downedIfaces.push_front((*it)->iface);
                                                CM_DBG("Sendto error: could not send beacon: %s\n", STRERROR(ERRNO));
                                        }
                                }
                        }
			age_interfaces(rootInterface);
			
			while (!downedIfaces.empty()) {
				handleInterfaceDown(downedIfaces.pop());
			}
			
		} else if (waitRet == Watch::FAILED) {
			CM_DBG("wait/select error: %s\n", STRERROR(ERRNO));
			// Assume unrecoverable error:
			// FIXME: determine if above assumption holds.
			return false;
		} else if (waitRet == Watch::ABANDONED) {
			return false;
		}

		
		if (w.isSet(socketIndex)) {
			int len;
			socklen_t addr_len = SOCKADDR_SIZE;;
			char buf[SOCKADDR_SIZE];
			struct sockaddr *in_addr = (struct sockaddr *)buf;
			
			len = recvfrom(listenSock, buffer, BUFLEN,
#if defined(OS_WINDOWS)
				       0,
#else
				       MSG_WAITALL,
#endif
				       in_addr, &addr_len);
			
			if (len == -1) {
				CM_DBG("Unable to recvfrom: %s\n", STRERROR(ERRNO));
				// Handle error in other way?
			} else if (len != sizeof(struct haggle_beacon)) {
				CM_DBG("Bad size of beacon: len=%d\n", len);
			} else {
				Addresses addrs;
				
				// We'll assume that this protocol is available:
				addrs.add(new Address(AddressType_EthMAC, (unsigned char *)beacon->mac));
				
				if (in_addr->sa_family == AF_INET6) {
					addrs.add(new Address(in_addr, NULL, ProtocolSpecType_TCP, TCP_DEFAULT_PORT));
					
					Interface iface(IFTYPE_ETHERNET, beacon->mac, &addrs, "Remote Ethernet", IFFLAG_UP);
					
					report_interface(&iface, rootInterface, newConnectivityInterfacePolicyTTL3);
				} else if (in_addr->sa_family == AF_INET) {					
					addrs.add(new Address(in_addr, NULL, ProtocolSpecType_TCP, TCP_DEFAULT_PORT));
					
					Interface iface(IFTYPE_ETHERNET, beacon->mac, &addrs, "Remote Ethernet", IFFLAG_UP);
					report_interface(&iface, rootInterface, newConnectivityInterfacePolicyTTL3);
				}
			}
		}
	}
	return false;
}
#endif /* ENABLE_ETHERNET */
