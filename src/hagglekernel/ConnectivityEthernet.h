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
#ifndef _CONNECTIVITYETH_H_
#define _CONNECTIVITYETH_H_

#include <libcpphaggle/Platform.h>

#if defined(ENABLE_ETHERNET)
#include <libcpphaggle/List.h>
#include "Connectivity.h"
#include "Interface.h"

// The haggle connectivity UDP port number.
#define HAGGLE_UDP_CONNECTIVITY_PORT 9697

struct haggle_beacon {
        u_int32_t seqno;
	u_int8_t interval; // The beacon interval used by the other node (in seconds)
        char mac[6];
        char pad[2];
};

#define HAGGLE_BEACON_LEN (sizeof(struct haggle_beacon))

class ConnEthIfaceListElement;

/**
	Ethernet (really IP) connectivity manager module listener

	This module listens on all IP connected interfaces using UDP broadcast
	messages from other Haggle nodes.

	Reports to the connectivity manager when it finds new haggle nodes.
*/
class ConnectivityEthernet : public Connectivity
{
private:
	InterfaceRef rootInterface;
	Interface *rootInterfacePtr;
	int listenSock;
	List<ConnEthIfaceListElement *>	ifaceList;
	Mutex ifaceListMutex;
	u_int32_t seqno;
	u_int8_t beaconInterval;

        bool run();
        void hookCleanup();
public:
	virtual bool handleInterfaceUp(const InterfaceRef &iface);
	virtual void handleInterfaceDown(const InterfaceRef &iface);
	virtual void setPolicy(PolicyRef newPolicy);
	/**
	   Tells the connectivity to finish. The connectivity will not 
	   neccesarily have finished by the time cancelDiscovery() returns.
	   
	   This function is intended to be the exact same as 
	   Connectivity::cancelDiscovery(), but with one specific exception:
	   that it does NOT call Thread::cancel() either directly or 
	   indirectly.
	   
	   The reason for this is explained in haggle trac system, ticket #106.
	*/
	virtual void cancelDiscovery(void);
        ConnectivityEthernet(ConnectivityManager *m, const InterfaceRef& iface);
        ~ConnectivityEthernet();
};

#endif

#endif
