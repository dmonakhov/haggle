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

#include "ProtocolSocket.h"

#define MAX(a,b) (a > b ? a : b)

#if defined(ENABLE_IPv6)
#define SOCKADDR_SIZE sizeof(struct sockaddr_in6)
#else
#define SOCKADDR_SIZE sizeof(struct sockaddr_in)
#endif

ProtocolSocket::ProtocolSocket(const ProtType_t _type, const char *_name, InterfaceRef _localIface, 
			       InterfaceRef _peerIface, const int _flags, ProtocolManager * m, SOCKET _sock, size_t bufferSize) : 
	Protocol(_type, _name, _localIface, _peerIface, _flags, m, bufferSize), 
        sock(_sock), socketIsRegistered(false), nonblock(false)
{
	if (sock != INVALID_SOCKET) {
                setSocketOptions();
	}
}

void ProtocolSocket::setSocketOptions()
{
#if defined(OS_LINUX)
        int ret;
        long optval;
        socklen_t optlen = sizeof(optval);

        ret = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &optval, &optlen);

        if (ret != -1) {
                optval = optval * 4; // Quadruple receive buffer size

                ret = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &optval, optlen);

                if (ret == -1) {
                        HAGGLE_ERR("Could not set recv buffer size to %ld bytes\n", optval);
                } else {
                        HAGGLE_DBG("Set recv buffer size to %ld bytes on protocol %s\n", optval, getName());
                }
        }
#endif
}

ProtocolSocket::~ProtocolSocket()
{
	if (sock != INVALID_SOCKET)
		closeSocket();
}

bool ProtocolSocket::openSocket(int domain, int type, int protocol, bool registersock, bool nonblock)
{
	if (sock != INVALID_SOCKET) {
		HAGGLE_ERR("%s: socket already open\n", getName());
		return false;
	}

	sock = socket(domain, type, protocol);

	if (sock == INVALID_SOCKET) {
		HAGGLE_ERR("%s: could not open socket : %s\n", getName(), STRERROR(ERRNO));
		return false;
	}
	
	if (!setNonblock(nonblock)) {
		CLOSE_SOCKET(sock);
		return false;
	}
        
        setSocketOptions();

	if (registersock && !registerSocket()) {
		CLOSE_SOCKET(sock);
		return false;
	}
	return true;
}

bool ProtocolSocket::setSocket(SOCKET _sock, bool registersock)
{
	if (sock != INVALID_SOCKET)
		return false;
	
	sock = _sock;
	
	if (registersock && !registerSocket()) {
		CLOSE_SOCKET(sock);
		return false;
	}
	return true;
}


bool ProtocolSocket::bindSocket(const struct sockaddr *saddr, socklen_t addrlen)
{
	if (!saddr)
		return false;

	if (bind(sock, saddr, addrlen) == SOCKET_ERROR) {
		HAGGLE_ERR("%s: could not bind socket : %s\n", getName(), STRERROR(ERRNO));
		return false;
	}
	return true;
}

SOCKET ProtocolSocket::acceptOnSocket(struct sockaddr *saddr, socklen_t *addrlen)
{
	if (sock == INVALID_SOCKET) {
		HAGGLE_ERR("%s: cannot accept client on invalid server socket\n", getName());
		return INVALID_SOCKET;
	}
	if (!saddr || !addrlen) {
		HAGGLE_ERR("%s: cannot accept connection as address is invalid\n", getName());
		return INVALID_SOCKET;
	}
	if (getMode() != PROT_MODE_LISTENING) {
		HAGGLE_ERR("%s: cannot accept connection on non-listening socket\n", getName());
		return INVALID_SOCKET;
	}
	SOCKET clientsock = accept(sock, saddr, addrlen);

	if (clientsock == INVALID_SOCKET) {
		HAGGLE_ERR("%s: accept failed : %s\n", getName(), STRERROR(ERRNO));
		return INVALID_SOCKET;
	}
	return clientsock;
}

bool ProtocolSocket::setListen(int backlog)
{
	if (!localIface)
		return false;

	if (listen(sock, backlog) == SOCKET_ERROR) {
		HAGGLE_DBG("Could not set listen on %s\n", getName());
		return false;
	}
	
	setMode(PROT_MODE_LISTENING);

	return true;
}

bool ProtocolSocket::setNonblock(bool _nonblock)
{
	
#ifdef OS_WINDOWS
	unsigned long on = _nonblock ? 1 : 0;

	if (ioctlsocket(sock, FIONBIO, &on) == SOCKET_ERROR) {
		HAGGLE_ERR("%s: Could not set %s mode on socket %d : %s\n", 
			   getName(), _nonblock ? "nonblocking" : "blocking", sock, STRERROR(ERRNO));
		return false;
	}
#elif defined(OS_UNIX)
	long mode = fcntl(sock, F_GETFL, 0);
	
	if (mode == -1) {
		HAGGLE_ERR("%s: could not get socket flags : %s\n", getName(), STRERROR(ERRNO));
		return false;
	}

	if (_nonblock)
		mode = mode | O_NONBLOCK;
	else
		mode = mode & ~O_NONBLOCK;

	if (fcntl(sock, F_SETFL, mode) == -1) {
		HAGGLE_ERR("%s: Could not set %s mode on socket %d : %s\n", 
			   getName(), _nonblock ? "nonblocking" : "blocking", sock, STRERROR(ERRNO));
		return false;
	}
#endif
        nonblock = _nonblock;

	return true;
}

bool ProtocolSocket::isNonblock()
{
        return nonblock;
}

void ProtocolSocket::closeSocket()
{
	if (sock == INVALID_SOCKET) {
		HAGGLE_ERR("%s: cannot close non-open socket\n", getName());
		return;
	}

	if (socketIsRegistered) {
		getKernel()->unregisterWatchable(sock);
		socketIsRegistered = false;
	}
	CLOSE_SOCKET(sock);
	setMode(PROT_MODE_DONE);
}


ssize_t ProtocolSocket::sendTo(const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
{
	if (!to || !buf) {
		HAGGLE_ERR("%s: invalid argument\n", getName());
		return -1;
	}
	if (sock == INVALID_SOCKET) {
		HAGGLE_ERR("%s: cannot sendto on closed socket\n", getName());
		return -1;
	}
	ssize_t ret =  sendto(sock, (const char *)buf, len, flags, to, tolen);

	if (ret == -1) {
		HAGGLE_ERR("%s: sendto failed : %s\n", getName(), STRERROR(ERRNO));
	}
	return ret;
}

ssize_t ProtocolSocket::recvFrom(void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
	if (!from || !buf) {
		HAGGLE_ERR("%s: invalid argument\n", getName());
		return -1;
	}
	if (sock == INVALID_SOCKET) {
		HAGGLE_ERR("%s: cannot recvfrom on closed socket\n", getName());
		return -1;
	}
	ssize_t ret = recvfrom(sock, (char *)buf, len, flags, from, fromlen);

	if (ret == -1) {
		HAGGLE_ERR("%s: recvfrom failed : %s\n", getName(), STRERROR(ERRNO));
	}
	return ret;
}

InterfaceRef ProtocolSocket::resolvePeerInterface(const Address& addr)
{
	int res;
        InterfaceRef pIface = getKernel()->getInterfaceStore()->retrieve(addr);

	if (pIface) {
		HAGGLE_DBG("Peer interface is [%s]\n", pIface->getIdentifierStr());
	} else if (addr.getType() == AddressType_IPv4 
#if defined(ENABLE_IPv6)
		|| addr.getType() == AddressType_IPv6
#endif
		) {
                char buf[SOCKADDR_SIZE];
                unsigned char mac[6];
                struct sockaddr *peer_addr = (struct sockaddr *)buf;
                addr.fillInSockaddr(peer_addr);
                
                HAGGLE_DBG("trying to figure out peer mac for IP %s on interface %s\n", addr.getAddrStr(), localIface->getName());

                res = get_peer_mac_address(peer_addr, localIface->getName(), mac, 6);

		if (res < 0) {
			HAGGLE_ERR("Error when retreiving mac address for peer %s, error=%d\n", addr.getAddrStr(), res);
		} else if (res == 0) {
			HAGGLE_ERR("No corresponding mac address for peer %s\n", addr.getAddrStr());
		} else {
			Address addr2(AddressType_EthMAC, mac);
			pIface = new Interface(localIface->getType(), mac, &addr, "TCP peer", IFFLAG_UP);
			pIface->addAddress(&addr2);
			HAGGLE_DBG("Peer interface is [%s]\n", pIface->getIdentifierStr());
		}
	}

	return pIface;
}

bool ProtocolSocket::registerSocket()
{
	if (sock == INVALID_SOCKET) {
		HAGGLE_ERR("%s: Cannot register invalid socket\n", getName());
		return false;
	} 
	if (socketIsRegistered) {
		HAGGLE_ERR("%s: Socket already registered\n", getName());
		return false;
	}
	if (getKernel()->registerWatchable(sock, getManager()) <= 0) {
		HAGGLE_ERR("%s: Could not register socket with kernel\n", getName());
		return false;
	}
	
	socketIsRegistered = true;
	
	return true;
}

bool ProtocolSocket::hasWatchable(const Watchable &wbl)
{
	return wbl == sock;
}

void ProtocolSocket::handleWatchableEvent(const Watchable &wbl)
{
	if (wbl != sock) {
		HAGGLE_ERR("ERROR! : %s does not belong to Protocol %s\n", wbl.getStr(), getName());
		return;
	}

	if (isClient())
		receiveDataObject();
	else if (isServer())
		acceptClient();
}

ProtocolEvent ProtocolSocket::openConnection(const struct sockaddr *saddr, socklen_t addrlen)
{
        bool wasNonblock = nonblock;

	if (sock == INVALID_SOCKET) {
		HAGGLE_ERR("%s: cannot open connection as socket is invalid\n", getName());
		return PROT_EVENT_ERROR;
	}

	if (!saddr) {
		HAGGLE_ERR("%s: cannot open connection because address is invalid\n", getName());
		return PROT_EVENT_ERROR;
	}

	if (isConnected()) {
		HAGGLE_ERR("%s: cannot open connection because a connection is already open\n", getName());
		return PROT_EVENT_ERROR;
	}

        // Make sure that we block while trying to connect
        if (nonblock)
                setNonblock(false);

	if (connect(sock, saddr, addrlen) == SOCKET_ERROR) {
		HAGGLE_ERR("%s Connection failed : %s\n", getName(), getProtocolErrorStr());

                if (wasNonblock)
                        setNonblock(true);

		return PROT_EVENT_ERROR;
	}

        if (wasNonblock)
                setNonblock(true);

	setFlag(PROT_FLAG_CONNECTED);

	return PROT_EVENT_SUCCESS;
}

void ProtocolSocket::closeConnection()
{
	if (sock == INVALID_SOCKET) {
		HAGGLE_ERR("%s: cannot close connection as socket is not valid\n", getName());
	}	return;

	if (socketIsRegistered)
		getKernel()->unregisterWatchable(sock);
	
	CLOSE_SOCKET(sock);

	unSetFlag(PROT_FLAG_CONNECTED);
}

bool ProtocolSocket::setSocketOption(int level, int optname, void *optval, socklen_t optlen)
{
	if (setsockopt(sock, level, optname, (char *)optval, optlen) == SOCKET_ERROR) {
		HAGGLE_ERR("%s: setsockopt failed : %s\n", getName(), STRERROR(ERRNO));
		return false;
	}
	return true;
}

ProtocolEvent ProtocolSocket::receiveData(void *buf, size_t len, const int flags, size_t *bytes)
{
	ssize_t ret;
	
	*bytes = 0;
	
	ret = recv(sock, (char *)buf, len, flags);
	
	if (ret < 0) {
		return PROT_EVENT_ERROR;
	} else if (ret == 0)
		return PROT_EVENT_PEER_CLOSED;
	
	*bytes = ret;
	
	return PROT_EVENT_SUCCESS;
}

ProtocolEvent ProtocolSocket::sendData(const void *buf, size_t len, const int flags, size_t *bytes)
{
	ssize_t ret;

	*bytes = 0;

	ret = send(sock, (const char *)buf, len, flags);

	if (ret < 0) {
		return PROT_EVENT_ERROR;
	} else if (ret == 0)
		return PROT_EVENT_PEER_CLOSED;

	*bytes = ret;
	
	return PROT_EVENT_SUCCESS;
}

ProtocolEvent ProtocolSocket::waitForEvent(Timeval *timeout, bool writeevent)
{
	Watch w;
	int ret, index;

	index = w.add(sock, writeevent ? WATCH_STATE_WRITE : WATCH_STATE_READ);

	ret = w.wait(timeout);
	
	if (ret == Watch::TIMEOUT)
		return PROT_EVENT_TIMEOUT;
	else if (ret == Watch::FAILED)
		return PROT_EVENT_ERROR;
	else if (ret == Watch::ABANDONED)
                return PROT_EVENT_SHOULD_EXIT;

	if (w.isReadable(index))
		return PROT_EVENT_INCOMING_DATA;
	else if (w.isWriteable(index)) 
		return PROT_EVENT_WRITEABLE;

	return PROT_EVENT_ERROR;
}

ProtocolEvent ProtocolSocket::waitForEvent(DataObjectRef &dObj, Timeval *timeout, bool writeevent)
{
	QueueElement *qe = NULL;
	Queue *q = getQueue();

	if (!q)
		return PROT_EVENT_ERROR;

	QueueEvent_t qev = q->retrieve(&qe, sock, timeout, writeevent);

	switch (qev) {
	case QUEUE_TIMEOUT:
		return  PROT_EVENT_TIMEOUT;
	case QUEUE_WATCH_ABANDONED:
		return PROT_EVENT_SHOULD_EXIT;
	case QUEUE_WATCH_READ:
		return PROT_EVENT_INCOMING_DATA;
	case QUEUE_WATCH_WRITE:
		return PROT_EVENT_WRITEABLE;
	case QUEUE_ELEMENT:
		dObj = qe->getDataObject();
		delete qe;
		return PROT_EVENT_TXQ_NEW_DATAOBJECT;
	case QUEUE_EMPTY:
		return PROT_EVENT_TXQ_EMPTY;
	default:
		break;
	}

	return PROT_EVENT_ERROR;
}
