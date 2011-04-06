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
	
	bool openBroadcastSocket();
	ConnEthIfaceListElement(const InterfaceRef &_iface);
	~ConnEthIfaceListElement();
};

ConnEthIfaceListElement::ConnEthIfaceListElement(const InterfaceRef &_iface) :
	iface(_iface), broadcastSocket(INVALID_SOCKET)
{
	// Initialize:
	memset(&broadcast_packet, 0, sizeof(broadcast_packet));
}

ConnEthIfaceListElement::~ConnEthIfaceListElement()
{
	if (broadcastSocket != INVALID_SOCKET)
		CLOSE_SOCKET(broadcastSocket);
}

bool ConnEthIfaceListElement::openBroadcastSocket()
{
#if defined(ENABLE_IPv6)
	struct sockaddr_in6 my_addr6;
#endif
	struct sockaddr_in my_addr4;
	struct sockaddr	*my_addr = NULL;
	socklen_t my_addr_len = -1;
	bool has_broadcast = false;
	const SocketAddress *addr;
	int on = 1;

	if (broadcastSocket != INVALID_SOCKET) {
		HAGGLE_ERR("Socket already set\n");
		return false;
	}

	memcpy(broadcast_packet.mac, iface->getIdentifier(), iface->getIdentifierLen());

#if defined(ENABLE_IPv6)
	// Prefer IPv6 addresses:
	addr = iface->getAddress<IPv6Address>();

	if (addr) {
		IPv6BroadcastAddress *ipv6bc;
		// Fill in the socket address for the broadcasting socket:
		my_addr = (struct sockaddr *) &my_addr6;
		
		my_addr_len = addr->fillInSockaddr(my_addr);
		my_addr6.sin6_port = htons(0);
		
		ipv6bc = iface->getAddress<IPv6BroadcastAddress>();
		
		if (ipv6bc) {
			has_broadcast = true;
			// Fill in the broadcast address to send to:
			broadcast_addr_len = ipv6bc->fillInSockaddr(&broadcast_addr, 
								    HAGGLE_UDP_CONNECTIVITY_PORT);
		}
	}
#endif
	
	// Fallback: IPv4 address:
	if (!has_broadcast) {
		addr = iface->getAddress<IPv4Address>();
                
		if (addr) {
			IPv4BroadcastAddress *ipv4bc;
			// Fill in the socket address for the broadcasting socket:
			my_addr = (struct sockaddr *) &my_addr4;
			
			my_addr_len = addr->fillInSockaddr(my_addr);
			my_addr4.sin_port = htons(0);
			
			ipv4bc = iface->getAddress<IPv4BroadcastAddress>();

			if (ipv4bc) {
				has_broadcast = true;
				// Fill in the broadcast address to send to:
				broadcast_addr_len = ipv4bc->fillInSockaddr(&broadcast_addr, 
									    HAGGLE_UDP_CONNECTIVITY_PORT);
			}
		}
	}
	
	if (!has_broadcast) {
		HAGGLE_ERR("No broadcast address found\n");
		return false;
	}
	// Open a UDP socket:
	broadcastSocket = socket(AF_INET, SOCK_DGRAM, 0);

	if (broadcastSocket == INVALID_SOCKET) {
		HAGGLE_ERR("Could not open socket\n");
		return false;
	}

	if (setsockopt(broadcastSocket, SOL_SOCKET, SO_BROADCAST, (const char *) &on, sizeof(on)) == -1) {
		CLOSE_SOCKET(broadcastSocket);
		HAGGLE_ERR("Could not set socket option SO_BROADCAST\n");
		return false;
	}
#if defined(OS_LINUX)
	int priority = IPTOS_LOWDELAY;

	if (setsockopt(broadcastSocket, SOL_SOCKET, SO_PRIORITY, (const char *) &priority, sizeof(priority)) == -1) {
		// If setting a very high priorty fails, lets try a
		// more reasonable one that does not require root
		// permissions.
		priority = 6;
		if (setsockopt(broadcastSocket, SOL_SOCKET, SO_PRIORITY, (const char *) &priority, sizeof(priority)) == -1) {
			CLOSE_SOCKET(broadcastSocket);
			HAGGLE_ERR("Could not set socket option SO_PRIORITY\n");
			return false;
		}
	}
#endif
	// Bind the socket to the address:
	if (bind(broadcastSocket, my_addr, my_addr_len) == -1) {
		CLOSE_SOCKET(broadcastSocket);
		HAGGLE_ERR("Could not bind socket\n");
		return false;
	}

	return true;
}

bool ConnectivityEthernet::handleInterfaceUp(const InterfaceRef &iface)
{
        Mutex::AutoLocker l(ifaceListMutex);
	ConnEthIfaceListElement	*new_elem = NULL;
	
	if (!(iface->getType() == Interface::TYPE_ETHERNET ||
	      iface->getType() == Interface::TYPE_WIFI))
		return false;
	
	// HOTFIX: make sure not to add the ethernet root interface to the list
	// of scanned interfaces.
	if (iface == fakeRootInterface)
		return true;
	
	// Check that we don't already have this interface:
	for (List<ConnEthIfaceListElement *>::iterator it = ifaceList.begin(); it != ifaceList.end(); it++) {
		if ((*it)->iface == iface)
			return true;
        }
	
	// Create a new element to hold this interface:
	new_elem = new ConnEthIfaceListElement(iface);

	if (!new_elem)
		return false;

	if (!new_elem->openBroadcastSocket()) {
		delete new_elem;
		return false;
	}

	// Insert into list:
	ifaceList.push_front(new_elem);
	
	return true;
}

void ConnectivityEthernet::handleInterfaceDown(const InterfaceRef &iface)
{
        bool is_empty = true;

	if (!(iface->getType() == Interface::TYPE_ETHERNET ||
	      iface->getType() == Interface::TYPE_WIFI))
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
	/* TODO: Make beacon interval dynamic */
#if defined(ENABLE_DYNAMIC_BEACON_INTERVAL)
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
        CM_DBG("Setting beacon interval to %u seconds\n", beaconInterval);
#endif
}


bool ConnectivityEthernet::isBeaconMine(struct haggle_beacon *b)
{
	synchronized(ifaceListMutex) {
		List<ConnEthIfaceListElement *>::iterator it = ifaceList.begin();
		
		for (;it != ifaceList.end(); it++) {
			if (memcmp((*it)->iface->getIdentifier(), b->mac, (*it)->iface->getIdentifierLen()) == 0) {
				return true;
			}
		}
	}
	return false;
}

ConnectivityEthernet::ConnectivityEthernet(ConnectivityManager * m, const InterfaceRef& iface) :
	Connectivity(m, iface, "Ethernet connectivity"), listenSock(INVALID_SOCKET), 
	seqno(0), beaconInterval(5)
{
}

ConnectivityEthernet::~ConnectivityEthernet()
{
	while (!ifaceList.empty()) {
		ConnEthIfaceListElement	*elem;
		
		elem = ifaceList.front();
		ifaceList.pop_front();
		delete elem;
	}
	if (listenSock != INVALID_SOCKET)
		CLOSE_SOCKET(listenSock);
}

bool ConnectivityEthernet::init()
{
	int on = 1;
	struct sockaddr_in my_addr;

	// Add the first interface to the broadcasting list:
	if (!handleInterfaceUp(rootInterface)) {
                HAGGLE_ERR("Could not bring up ethernet connectivity interface\n");
                return false;
        }
	// FIXME: should support IPv6 here.
	memset(&my_addr, 0, sizeof(struct sockaddr_in));
	// Set up local port:
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = INADDR_ANY;
	my_addr.sin_port = htons(HAGGLE_UDP_CONNECTIVITY_PORT);
	
	const IPv4Address addr(my_addr);

	fakeRootInterface = new EthernetInterface( 
		// FIXME: Hotfix for hotfix: use the MAC address of the first interface, 
		// rather than a bogus MAC address. Not good, but works for now.
		rootInterface->getIdentifier(), 
		"Root ethernet",
		&addr, 
		IFFLAG_UP|IFFLAG_LOCAL);
	
	if (!fakeRootInterface) {
		HAGGLE_ERR("Could not create fake root interface\n");
		return false;
	}

	// Open a UDP socket:
	listenSock = socket(AF_INET, SOCK_DGRAM, 0);

	if (listenSock == INVALID_SOCKET) {
		HAGGLE_ERR("Could not open socket\n");
		return false;
	}
	// Allow reuse of this address:
	if (setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on)) == -1) {
		CLOSE_SOCKET(listenSock);
		HAGGLE_ERR("Could not set SO_REUSEADDR socketopt\n");
		return false;
	}
	// Bind the socket to the address:
	if (bind(listenSock, (struct sockaddr *) &my_addr, sizeof(my_addr)) == -1) {
		CLOSE_SOCKET(listenSock);
		HAGGLE_ERR("Could not bind socket\n");
		return false;
	}
	/*
		HOTFIX: report the fake root interface as being local and existing.
		This is necessary because the interface store needs a parent interface
		to be in the store if ageing is to work.
	*/
	report_interface(fakeRootInterface, NULL, new ConnectivityInterfacePolicyAgeless());
	
	return true;
}

// This is jitter in the range [ -1000000 : 1000000 ] microseconds.

#define BEACON_JITTER ((prng_uint32() % 2000000) - 1000000)
#define BEACON_EPSILON (1) // Add at least BEACON_EPSILON seconds to any timeouts set on an interface
#define BEACON_LOSS_MAX (3)
#define BEACON_TIMEOUT(interval) (Timeval::now() + ((interval + BEACON_EPSILON) * BEACON_LOSS_MAX))

bool ConnectivityEthernet::run()
{
	Watch w;
	int socketIndex, waitRet = 0;
	char buffer[HAGGLE_BEACON_LEN];
	struct haggle_beacon *beacon = (struct haggle_beacon *)buffer;
	Timeval next_beacon_time = Timeval::now();
	Timeval lifetime = -1; // The lifetime of the neighbor interface closest to death
	
	socketIndex = w.add(listenSock); 
	
	while (!shouldExit()) {
		Timeval timeout;

                if (!w.getRemainingTime(&timeout)) {
                        timeout = Timeval::now();
                        
			if (!lifetime.isValid() || next_beacon_time < lifetime) {
				//CM_DBG("Computing timeout based on next beacon time\n");
				timeout = next_beacon_time - timeout;
			} else {
				//CM_DBG("Computing timeout based on lifetime\n");
				timeout = lifetime - timeout;
			}
                }
                
		w.reset();
		
		//CM_DBG("Next timeout is in %lf seconds\n", timeout.getTimeAsSecondsDouble());
		
		waitRet = w.wait(&timeout); 

		if (waitRet == Watch::TIMEOUT) {				
			if (Timeval::now() >= next_beacon_time) {
				InterfaceRefList downedIfaces;		
				int ret;
				
				// Timeout --> send next beacon
				seqno++;	
				
				//CM_DBG("Sending beacon seqno=%lu\n", seqno);
				
				synchronized(ifaceListMutex) {
					List<ConnEthIfaceListElement *>::iterator it = ifaceList.begin();
					
					for (;it != ifaceList.end(); it++) {
						(*it)->broadcast_packet.seqno = htonl(seqno);
						(*it)->broadcast_packet.interval = htonl(beaconInterval);
						 
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
				
				while (!downedIfaces.empty()) {
					handleInterfaceDown(downedIfaces.pop());
				}

                                /* Compute next beacon time. */
                                Timeval jitter(0, BEACON_JITTER);
                                next_beacon_time += (beaconInterval + jitter);
			} 
			
			// Age the neighbor interfaces
			age_interfaces(fakeRootInterface, &lifetime);
			
			/*
			if (lifetime.isValid()) {
				Timeval now = Timeval::now();
				CM_DBG("Closest to death interface will expire in %lf seconds\n", (lifetime - now).getTimeAsSecondsDouble());
			}
			*/
		} else if (waitRet == Watch::FAILED) {
			CM_DBG("wait/select error: %s\n", STRERROR(ERRNO));
			// Assume unrecoverable error:
			// FIXME: determine if above assumption holds.
			return false;
		} else if (waitRet == Watch::ABANDONED) {
			CM_DBG("Watch was abandoned\n");
			return false;
		}

		
		if (w.isSet(socketIndex)) {
			int len;
			socklen_t addr_len = SOCKADDR_SIZE;;
			char buf[SOCKADDR_SIZE];
			struct sockaddr *in_addr = (struct sockaddr *)buf;
			
			len = recvfrom(listenSock, buffer, HAGGLE_BEACON_LEN,
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
			} else if (!isBeaconMine(beacon)) {
				Addresses addrs;
				Timeval received_lifetime = BEACON_TIMEOUT(ntohl(beacon->interval));
				
				if (received_lifetime < lifetime)
					lifetime = received_lifetime;
				
				// We'll assume that this protocol is available:
				addrs.add(new EthernetAddress(beacon->mac));

				if (in_addr->sa_family == AF_INET) {	
					addrs.add(new IPv4Address((struct sockaddr_in&)*in_addr, TransportTCP(TCP_DEFAULT_PORT)));
										
					/*
					  CM_DBG("Neighbor interface (%s) will expire in %lf seconds\n", 
					  ipv4->getURI(), (received_lifetime - Timeval::now()).getTimeAsSecondsDouble());
					*/
					
					EthernetInterface iface(beacon->mac, "Remote Ethernet", NULL, IFFLAG_UP);
					iface.addAddresses(addrs);
					report_interface(&iface, fakeRootInterface, new ConnectivityInterfacePolicyTime(received_lifetime));
				}
#if defined(ENABLE_IPV6)
				else if (in_addr->sa_family == AF_INET6) {
					addrs.add(new IPv6Address((struct sockaddr_in6&)*in_addr, TransportTCP(TCP_DEFAULT_PORT)));
					
					EthernetInterface iface(beacon->mac, "Remote Ethernet", NULL, IFFLAG_UP);
					iface.addAddresses(addrs);
					report_interface(&iface, fakeRootInterface, new ConnectivityInterfacePolicyTime(received_lifetime));
				} 
#endif
			} else {
				//CM_DBG("Beacon is my own\n");
			}
		}
	}
	return false;
}
#endif /* ENABLE_ETHERNET */
