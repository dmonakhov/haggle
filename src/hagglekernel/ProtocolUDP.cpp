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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libcpphaggle/Platform.h>
#include <haggleutils.h>

#include "ProtocolUDP.h"

#if defined(ENABLE_IPv6)
#define SOCKADDR_SIZE sizeof(struct sockaddr_in6)
#else
#define SOCKADDR_SIZE sizeof(struct sockaddr_in)
#endif

void inline ProtocolUDP::init(const struct sockaddr *saddr, socklen_t addrlen)
{	
	int optval = 1;

	if (!openSocket(AF_INET, SOCK_DGRAM, 0, true)) {
#if HAVE_EXCEPTION
		throw SocketException(-1, "Could not open UDP socket");
#else
                return;
#endif
	}

	if (!setSocketOption(SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) {
		closeSocket();
#if HAVE_EXCEPTION
		throw SocketException(-1, "setsockopt SO_REUSEADDR failed");
#else
                return;
#endif
	}
	
	if (!setSocketOption(SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval))) {
		closeSocket();
#if HAVE_EXCEPTION
		throw SocketException(-1, "setsockopt SO_BROADCAST failed");
#else
                return;
#endif
	}
	
	if (!bindSocket(saddr, addrlen)) {
		closeSocket();
#if HAVE_EXCEPTION
		throw BindException(-1, "binding UDP socket failed");
#endif
	}
}

ProtocolUDP::ProtocolUDP(const InterfaceRef& _localIface, unsigned short _port, ProtocolManager * m) :
	ProtocolSocket(PROT_TYPE_UDP, "ProtocolUDP", _localIface, NULL, 
		       PROT_FLAG_SERVER | PROT_FLAG_CLIENT, m), port(_port)
{
        char buf[SOCKADDR_SIZE];
        struct sockaddr *sa = (struct sockaddr *)buf;
	socklen_t len;
	Address *addr = NULL;
	
	if (!localIface) {
#if HAVE_EXCEPTION
		throw ProtocolSocket::SocketException(-1, "Could not create UDP socket, no interface");
#else
                return;
#endif
	}
#if defined(ENABLE_IPv6)
	addr = localIface->getAddressByType(AddressType_IPv6);
#endif
	
	if (!addr)
		addr = localIface->getAddressByType(AddressType_IPv4);
	
	if (!addr) {
#if HAVE_EXCEPTION
		throw ProtocolSocket::SocketException(-1, "Could not create UDP socket, no IP address");
#else
                return;
#endif
        }
	
	len = addr->fillInSockaddr(sa);
	
	init(sa, len);
}

ProtocolUDP::ProtocolUDP(const char *ipaddr, unsigned short _port, ProtocolManager * m) : 
	ProtocolSocket(PROT_TYPE_UDP, "ProtocolUDP", NULL, NULL, 
		       PROT_FLAG_SERVER | PROT_FLAG_CLIENT, m), port(_port)
{
	struct in_addr addr;   
        char buf[SOCKADDR_SIZE];
        struct sockaddr *sa = (struct sockaddr *)buf;
	socklen_t len = 0;

#ifdef OS_WINDOWS
	unsigned long tmp_addr = inet_addr(ipaddr);
	memcpy(&addr, &tmp_addr, sizeof(struct in_addr));
#else
	inet_aton(ipaddr, &addr);
#endif
	Address address(AddressType_IPv4, (unsigned char *) &addr, NULL, 
		ProtocolSpecType_UDP, port);
	
	localIface = new Interface(IFTYPE_APPLICATION_PORT, NULL, &address, "Loopback", IFFLAG_UP);

	len = address.fillInSockaddr(sa);
	
	init(sa, len);
}

ProtocolUDP::~ProtocolUDP()
{
}

bool ProtocolUDP::isSender() 
{
	return true;
}

bool ProtocolUDP::isReceiver()
{
	return true;
}

bool ProtocolUDP::isForInterface(const InterfaceRef& iface)
{
	/*
		FIXME:
		This is a pretty crude check. We simply assume that 
		if this protocol is our IPC mechanism to communicate
		with applications, and the interface to check is also 
		an application interface, then this is the protocol to
		use.
	*/
	if (iface->getType() == IFTYPE_APPLICATION_PORT &&
		localIface->getType() == IFTYPE_APPLICATION_PORT)
		return true;
	else if (peerIface && iface == peerIface)
		return true;

	return false;
}

bool ProtocolUDP::sendDataObject(const DataObjectRef& dObj, const NodeRef& peer, const InterfaceRef& _peerIface)
{
	int ret;
        bool verdict = false;

	if (peerIface) {
		HAGGLE_ERR("Peer interface %s is set when it shouldn't be\n", peerIface->getIdentifierStr());
		return false;
	}
	
	// Reassign the peer interface to our target destination
	peerIface = _peerIface;
	
	/* Call send function */
	ret = sendDataObjectNow(dObj);
	
	// Send success/fail event with this DO:
	if (ret == PROT_EVENT_SUCCESS) {
		getKernel()->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL, dObj, peer));
                verdict = true;
	}
	
	// Release the peer interface again, such that we can assign it another 
	// destination next time
	peerIface = NULL;

	return verdict;
}


#if defined(OS_WINDOWS_XP) && !defined(DEBUG)
// This is here to avoid a warning with catching the exception in the function below.
#pragma warning( push )
#pragma warning( disable: 4101 )
#endif
ProtocolEvent ProtocolUDP::receiveDataObject()
{
	size_t len = 0;
	string haggleTag = "</Haggle>";
	DataObjectRef dObj;
        char buf[SOCKADDR_SIZE];
        struct sockaddr *peer_addr = (struct sockaddr *)buf;
	ProtocolEvent pEvent;
        unsigned short port;
        Address *addr = NULL;
	struct sockaddr_in *sa = NULL;

#ifdef OS_WINDOWS
	pEvent = receiveData(buffer, PROTOCOL_BUFSIZE, peer_addr, 0, &len);
#else
	pEvent = receiveData(buffer, PROTOCOL_BUFSIZE, peer_addr, MSG_DONTWAIT, &len);
#endif

	if (pEvent != PROT_EVENT_SUCCESS)
		return pEvent;

        if (peer_addr->sa_family == AF_INET) {
                sa = (struct sockaddr_in *)peer_addr;
                port = ntohs(sa->sin_port);
                
                addr = new Address(AddressType_IPv4, (unsigned char *) &(sa->sin_addr), 
                                   NULL, ProtocolSpecType_UDP, port);                
        }
#if defined(ENABLE_IPv6) 
        else if (peer_addr->sa_family == AF_INET6) {
                struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)peer_addr;
                port = ntohs(sa6->sin6_port);
                
                addr = new Address(AddressType_IPv6, (unsigned char *) &(sa6->sin6_addr), 
                                   NULL, ProtocolSpecType_UDP, port);
        }
#endif

        if (addr == NULL)
                return PROT_EVENT_ERROR;

	if (peerIface) {
		HAGGLE_ERR("UDP peer interface was not null\n");          
                delete addr;
		return PROT_EVENT_ERROR;
	}

        peerIface = new Interface(IFTYPE_APPLICATION_PORT, &port, addr, "Application", IFFLAG_UP);
        
        delete addr;

        dObj = new DataObject(buffer, len, localIface, peerIface);
        // Haggle doesn't own files that applications have put in:
        dObj->setOwnsFile(false);
        // We must release the peer interface reference after
        // the data object is created as the next incoming
        // data might be from another peer
        peerIface = NULL;

        if (!dObj) {
                HAGGLE_DBG("%s:%lu Could not create data object\n", getName(), getId());
		return PROT_EVENT_ERROR;
	}

	dObj->setReceiveTime(Timeval::now());

	if (getKernel()->getThisNode()->getBloomfilter()->has(dObj)) {
		HAGGLE_DBG("Data object [%s] from interface %s:%u has already been received, ignoring.\n", 
			dObj->getIdStr(), sa ? ip_to_str(sa->sin_addr) : "undefined", port);
		return PROT_EVENT_SUCCESS;
	}

	// Generate first an incoming event to conform with the base Protocol class
	getKernel()->addEvent(new Event(EVENT_TYPE_DATAOBJECT_INCOMING, dObj));
	
	HAGGLE_DBG("Received data object [%s] from interface %s:%u\n", 
		dObj->getIdStr(), sa ? ip_to_str(sa->sin_addr) : "undefined", port);

	// Since there is no data following, we generate the received event immediately 
	// following the incoming one
	getKernel()->addEvent(new Event(EVENT_TYPE_DATAOBJECT_RECEIVED, dObj));

	return PROT_EVENT_SUCCESS;
}

#if defined(OS_WINDOWS_XP) && !defined(DEBUG)
#pragma warning( pop )
#endif

ProtocolEvent ProtocolUDP::sendData(const void *buffer, size_t len, const int flags, size_t *bytes)
{
        char buf[SOCKADDR_SIZE];
        struct sockaddr *sa = (struct sockaddr *)buf;		
	socklen_t addrlen;
	Address	*addr = NULL;
	ssize_t ret;
	
	if (!buffer) {
		HAGGLE_DBG("Send buffer is NULL\n");
		return PROT_EVENT_ERROR;
	}
	
	if (!peerIface) {
		HAGGLE_DBG("Send interface invalid\n");
		*bytes = 0;
		return PROT_EVENT_ERROR;
	}
	
#if defined(ENABLE_IPv6)
	addr = peerIface->getAddressByType(AddressType_IPv6);
#endif

	if (!addr)
		addr = peerIface->getAddressByType(AddressType_IPv4);
	
	if (!addr) {
		HAGGLE_DBG("Send interface has no valid address\n");
		*bytes = 0;
		return PROT_EVENT_ERROR;
	}
	
	if (addr->getProtocolType() != ProtocolSpecType_UDP) {
		HAGGLE_DBG("Send interface [%s:%u] has no valid UDP port\n",
			addr->getAddrStr(), 
			addr->getProtocolPortOrChannel());
		*bytes = 0;
		return PROT_EVENT_ERROR;
	}
	
	addrlen = addr->fillInSockaddr(sa);
	
	HAGGLE_DBG("%s:%lu sending to address %s:%u\n", 
		getName(), getId(), addr->getAddrStr(), addr->getProtocolPortOrChannel());

	ret = sendTo(buffer, len, flags, sa, addrlen);

	if (ret < 0)
		return PROT_EVENT_ERROR;
	else if (ret == 0)
		return PROT_EVENT_PEER_CLOSED;

	*bytes = ret;

	return PROT_EVENT_SUCCESS;	
}

ProtocolEvent ProtocolUDP::receiveData(void *buf, size_t buflen, struct sockaddr *peer_addr, const int flags, size_t *bytes)
{
	ssize_t ret;

	*bytes = 0;

	socklen_t addrlen = SOCKADDR_SIZE;

	memset(peer_addr, 0, SOCKADDR_SIZE);

	ret = recvFrom(buf, buflen, flags, peer_addr, &addrlen);

	if (ret < 0) {
		return PROT_EVENT_ERROR;
	} else if (ret == 0)
		return PROT_EVENT_PEER_CLOSED;

	*bytes = ret;

	return PROT_EVENT_SUCCESS;	
}
