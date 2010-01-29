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
#ifndef _PROTOCOLTCP_H
#define _PROTOCOLTCP_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/

class ProtocolTCP;
class ProtocolTCPClient;
class ProtocolTCPServer;


#include <libcpphaggle/Platform.h>

#include "ProtocolSocket.h"


/* Configurable parameters */
#define TCP_BACKLOG_SIZE 10
#define TCP_DEFAULT_PORT 9697

/** */
class ProtocolTCP : public ProtocolSocket
{
        friend class ProtocolTCPServer;
        friend class ProtocolTCPClient;
	unsigned short localport;
	bool initbase();
        ProtocolTCP(SOCKET sock, const InterfaceRef& _localIface, const InterfaceRef& _peerIface,
		const unsigned short _port, const short flags = PROT_FLAG_CLIENT, ProtocolManager *m = NULL);
public:
        ProtocolTCP(const InterfaceRef& _localIface, const InterfaceRef& _peerIface,
                    const unsigned short _port = TCP_DEFAULT_PORT,
                    const short flags = PROT_FLAG_CLIENT, ProtocolManager *m = NULL);
        virtual ~ProtocolTCP() = 0;
};

/** */
class ProtocolTCPClient : public ProtocolTCP
{
        friend class ProtocolTCPServer;
	bool init_derived();
public:
        ProtocolTCPClient(SOCKET sock, const InterfaceRef& _localIface, const InterfaceRef& _peerIface, const unsigned short _port, ProtocolManager *m = NULL) : 
		ProtocolTCP(sock, _localIface, _peerIface, _port, PROT_FLAG_CLIENT | PROT_FLAG_CONNECTED, m) {}
        ProtocolTCPClient(const InterfaceRef& _localIface, const InterfaceRef& _peerIface,
                          const unsigned short _port = TCP_DEFAULT_PORT, ProtocolManager *m = NULL) :
                ProtocolTCP(_localIface, _peerIface, _port, PROT_FLAG_CLIENT, m) {}
        ProtocolEvent connectToPeer();};


/** */
class ProtocolTCPSender : public ProtocolTCPClient
{
public:
	ProtocolTCPSender(const InterfaceRef& _localIface, 
		const InterfaceRef& _peerIface,
		const unsigned short _port = TCP_DEFAULT_PORT, 
		ProtocolManager *m = NULL) :
	ProtocolTCPClient(_localIface, _peerIface, _port, m) {}
	bool isSender() { return true; }
};

/** */
class ProtocolTCPReceiver : public ProtocolTCPClient
{
public:
	ProtocolTCPReceiver(SOCKET sock, 
		const InterfaceRef& _localIface,
		const InterfaceRef& _peerIface,
		const unsigned short _port,
		ProtocolManager *m = NULL) : 
	ProtocolTCPClient(sock, _localIface, _peerIface, _port, m) {}
	bool isReceiver() { return true; }
};

/** */
class ProtocolTCPServer : public ProtocolTCP
{
        friend class ProtocolTCP;
        int backlog;
	bool init_derived();
public:
        ProtocolTCPServer(const InterfaceRef& _localIface = NULL, ProtocolManager *m = NULL,
                          const unsigned short _port = TCP_DEFAULT_PORT, int _backlog = TCP_BACKLOG_SIZE);
        ~ProtocolTCPServer();
        ProtocolEvent acceptClient();
};

#endif /* _PROTOCOLTCP_H */
