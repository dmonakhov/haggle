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

#include "DebugManager.h"

#if defined(ENABLE_DEBUG_MANAGER)
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libcpphaggle/Platform.h>

#include <haggleutils.h>
#include <libcpphaggle/Thread.h>

#include "DataObject.h"
#include "Node.h"
#include "Interface.h"
#include "Event.h"
#include "XMLMetadata.h"
#include "Debug.h"
#include "DataStore.h"

#include "ForwardingManager.h"

SOCKET openSocket(int port);

#define HAGGLE_XML_FILENAME "/tmp/haggle.xml"
#define HAGGLE_XSLT_FILENAME "/tmp/haggle.xslt"
#define HAGGLE_SVG_FILENAME "/tmp/haggle.svg"
#define DEFAULT_DEBUG_PORT 9090

// #ifdef DEBUG_LEAKS
// #undef DEBUG_LEAKS
// #endif

#define ADD_LOG_FILE_TO_DATASTORE	0

DebugManager::DebugManager(HaggleKernel * _kernel, bool interactive) : 
                Manager("DebugManager", _kernel), onFindRepositoryKeyCallback(NULL), 
                onDumpDataStoreCallback(NULL), server_sock(-1), console(INVALID_STDIN)
{
#define __CLASS__ DebugManager
#if defined(DEBUG)
	int i;

	for (i = EVENT_TYPE_PUBLIC_MIN; i < EVENT_TYPE_PUBLIC_MAX + 1; i++) {
		setEventHandler(i, publicEvent);
		HAGGLE_DBG("Listening on %d:%s\n", i, Event::getPublicName(i));
	}
#endif

	server_sock = openSocket(DEFAULT_DEBUG_PORT);

	HAGGLE_DBG("Server sock is %d\n", server_sock);

	if (server_sock == INVALID_SOCKET || !kernel->registerWatchable(server_sock, this)) {
			CLOSE_SOCKET(server_sock);
#if HAVE_EXCEPTION
			throw DebugException(-1, "Could not register socket");
#else
                        return;
#endif
	}

#if defined(OS_LINUX) || (defined(OS_MACOSX) && !defined(OS_MACOSX_IPHONE))
	if (interactive) {
		console = open("/dev/stdin", O_RDONLY);
		
		if (console == -1 || !kernel->registerWatchable(console, this)) {
			HAGGLE_ERR("Unable to open STDIN!\n");
#if HAVE_EXCEPTION
			throw DebugException(0, strerror(errno));
#else
                        return;
#endif
		}
	}
#elif defined(OS_WINDOWS_DESKTOP)
	if (interactive) {
		console = GetStdHandle(STD_INPUT_HANDLE);

		if (console == INVALID_HANDLE_VALUE || !kernel->registerWatchable(console, this)) {
#if HAVE_EXCEPTION
			throw DebugException(0, StrError(GetLastError()));
#else
			return;
#endif
		}
		// This will reset the console mode so that getchar() returns for 
		// every character read
		SetConsoleMode(console, 0);
	}
#endif
#ifdef DEBUG_LEAKS
	debugEType = registerEventType("DebugManager Debug Event", onDebugReport);

	if (debugEType < 0) {
#if HAVE_EXCEPTION
		throw DebugException(debugEType, "Could not register debug report event type...");
#else
		return;
#endif
	}
#if defined(OS_WINDOWS_MOBILE) || defined(OS_ANDROID) 
	kernel->addEvent(new Event(debugEType, NULL, 40));
#endif
#endif
	
#if ADD_LOG_FILE_TO_DATASTORE
	/*
		Log file distribution code:
		
		This uses the repository to save information about wether or not we have
		already added a data object to the source code. This is necessary in 
		order not to insert more than one data object. Also, the data object
		will need to include the node ID of this node, which may not have been
		created yet, so this code delays the creation of that data object (if
		it needs to be created) until that is done. 
	*/
	onFindRepositoryKeyCallback = newEventCallback(onFindRepositoryKey);
	kernel->getDataStore()->readRepository(new RepositoryEntry("DebugManager", "has saved log file data object"), onFindRepositoryKeyCallback);
#endif

	onDumpDataStoreCallback = newEventCallback(onDumpDataStore);
}

DebugManager::~DebugManager()
{
	if (onFindRepositoryKeyCallback)
		delete onFindRepositoryKeyCallback;

        if (onDumpDataStoreCallback)
                delete onDumpDataStoreCallback;
}

void DebugManager::onFindRepositoryKey(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	DataStoreQueryResult *qr = static_cast < DataStoreQueryResult * >(e->getData());
	
	RepositoryEntryRef re = qr->detachFirstRepositoryEntry();
	
	if (!re) {
		// No repository entry: no data object.
		DataObjectRef dObj;
		
		// Create data object:
		
		// Empty at first:
		dObj = new DataObject((const char *)NULL, 0);
		
		// Add log file attribute:
		Attribute a("Log file","Trace");
		dObj->addAttribute(a);
		
		// Add node id of local node, to make sure that two logs from different 
		// nodes don't clash:
		Attribute b("Node id", kernel->getThisNode()->getIdStr());
		dObj->addAttribute(b);
		
		// Give the data object the log as data:
		dObj->setFilePath(LogTrace::ltrace.getFile());
		
		// Name the log so that the files are more easily readable on the 
		// machine that receives them:
		char str[128];
		sprintf(str, "log-%s.txt", kernel->getThisNode()->getIdStr());
		dObj->setFileName(str);
		
		// Set the data object so that the getData_begin() function figures out
		// the file length, rather than specify it here.
		dObj->setDynamicDataLen(true);
		
		// Insert data object:
		kernel->getDataStore()->insertDataObject(dObj);
		
		// Insert a repository entry to show the data object exists:
		kernel->getDataStore()->insertRepository(new RepositoryEntry("DebugManager", "has saved log file data object", "yes"));
	}
	
	delete qr;
}

static bool sendBuffer(SOCKET sock, const char *data, size_t toSend)
{
	size_t i = 0;
	
	do {
		ssize_t ret = send(sock, &(data[i]), toSend, 0);
        
		if (ret == -1) {
#if defined(OS_WINDOWS)
			if (WSAGetLastError() != WSAEWOULDBLOCK) {
				HAGGLE_ERR("Could not write HTTP to socket err=%d\n", ERRNO);
				goto out;
			}
#else
			if (errno != EAGAIN) {
				HAGGLE_ERR("Could not write HTTP to socket err=%d\n", errno);
				goto out;
			}
#endif
		} else {
			toSend -= ret;
			i += ret;
		}
	} while (toSend > 0);
out:
	return toSend == 0;
}

static bool sendString(SOCKET sock, const char *str)
{
	return sendBuffer(sock, str, strlen(str));
}

static size_t skipXMLTag(const char *data, size_t len)
{
	size_t i = 0;
	
	// Skip over the <?xml version="1.0"?> tag:
	while(strncmp(&(data[i]), "?>", 2) != 0)
		i++;
	i += 2;
	return i;
}

void DebugManager::dumpTo(SOCKET client_sock, DataStoreDump *dump)
{
	size_t toSend = dump->getLen();
	const char *data = dump->getData();
	size_t i = 0;
	
	i = skipXMLTag(data, toSend);
	toSend -= i;
	// Send the <?xml version="1.0"?> tag:
	if (!sendString(client_sock, "<?xml version=\"1.0\"?>\n"))
		return;
	// Send the root tag:
	if (!sendString(client_sock, "<HaggleInfo>"))
		return;
	// Send the data:
	if (!sendBuffer(client_sock, &(data[i]), toSend))
		return;
	
        DataObjectRef dObj = kernel->getThisNode()->getDataObject(false);
        char *buf;
        size_t len;
        if (dObj->getMetadata()->getRawAlloc(&buf, &len)) {
                i = skipXMLTag(buf, len);
                len -= i;
                if (!sendString(client_sock, "<ThisNode>\n")) {
			free(buf);
			return;
		}
                if (!sendBuffer(client_sock, &(buf[i]), len)) {
			free(buf);
			return;
		}
                if (!sendString(client_sock, "</ThisNode>\n")) {
			free(buf);
			return;
		}
                free(buf);
        }
	
	/*
	 
	 FIXME: With the new forwarding this thing is broken.
	 
        Manager *mgr = kernel->getManager((char *)"ForwardingManager");
	
        if (mgr) {
                ForwardingManager *fmgr = (ForwardingManager *) mgr;
		
                DataObjectRef dObj = fmgr->getForwarder()->myMetricDO;
                if (dObj) {
                        char *buf;
                        size_t len;
                        if (dObj->getMetadata()->getRawAlloc(&buf, &len)) {
                                i = skipXMLTag(buf, len);
                                len -= i;
                                if (!sendString(client_sock, "<RoutingData>\n")) {
					free(buf);
					return;
				}
                                if (!sendBuffer(client_sock, &(buf[i]), len)) {
					free(buf);
					return;
				}
                                if (!sendString(client_sock, "</RoutingData>\n")) {
					free(buf);
					return;
				}
                                free(buf);
                        }
                }
        }
	*/
        NodeRefList nl;
	
        kernel->getNodeStore()->retrieveNeighbors(nl);
        if (!nl.empty()) {
                if (!sendString(client_sock, "<NeighborInfo>\n"))
                        return;
                for (NodeRefList::iterator it = nl.begin(); it != nl.end(); it++) {
                        if (!sendString(client_sock, "<Neighbor>"))
                                return;
                        if (!sendString(client_sock, (*it)->getIdStr()))
                                return;
                        if (!sendString(client_sock, "</Neighbor>\n"))
                                return;
                }
                if (!sendString(client_sock, "</NeighborInfo>\n"))
                        return;
        }
	
	// Send the end of the root tag:
	sendString(client_sock, "</HaggleInfo>");
}

void DebugManager::onDumpDataStore(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	DataStoreDump *dump = static_cast <DataStoreDump *>(e->getData());
	
	for (List<SOCKET>::iterator it = client_sockets.begin(); it != client_sockets.end(); it++) {
		dumpTo(*it, dump);
		kernel->unregisterWatchable(*it);
		CLOSE_SOCKET(*it);
	}
	client_sockets.clear();
	
	delete dump;
}

void DebugManager::onShutdown()
{
	if (server_sock != INVALID_SOCKET) {
		kernel->unregisterWatchable(server_sock);
		CLOSE_SOCKET(server_sock);
	}
	if (!client_sockets.empty()) {
		for(List<SOCKET>::iterator it = client_sockets.begin(); it != client_sockets.end(); it++) {
			kernel->unregisterWatchable(*it);
			CLOSE_SOCKET(*it);
		}
	}
	
#if defined(OS_LINUX) || defined(OS_MACOSX)
	if (console != -1) {
		kernel->unregisterWatchable(console);
		//CLOSE_SOCKET(console);
	}
#endif
	
#ifdef DEBUG
	Event::unregisterType(debugEType);
#endif
	unregisterWithKernel();
}

#if defined(DEBUG_LEAKS) && defined(DEBUG)
void DebugManager::onDebugReport(Event *e)
{
	//kernel->getInterfaceStore()->print();
	
	LOG_ADD("%s: kernel event queue size=%lu\n", Timeval::now().getAsString().c_str(), kernel->size()); 
	kernel->getNodeStore()->print();
#ifdef DEBUG_DATASTORE
	kernel->getDataStore()->print();
#endif
	kernel->addEvent(new Event(debugEType, NULL, 20));
}
#endif

#define BUFSIZE 4096

void DebugManager::onWatchableEvent(const Watchable& wbl)
{
	int num = 0;

	if (!wbl.isValid())
		return;
	
	if (wbl == server_sock) {
		struct sockaddr cliaddr;
		socklen_t len;
		SOCKET client_sock;
		
		len = sizeof(cliaddr);
		client_sock = accept(server_sock, &cliaddr, &len);
	
		if (client_sock != INVALID_SOCKET) {
			HAGGLE_DBG("Registering client socket: %ld\n", client_sock);
			if (!kernel->registerWatchable(client_sock, this)) {
				CLOSE_SOCKET(client_sock);
				return;
			}
			if(client_sockets.empty())
				kernel->getDataStore()->dump(onDumpDataStoreCallback);
			client_sockets.push_back(client_sock);
		}else{
			HAGGLE_DBG("accept failed: %ld\n", client_sock);
		}
		num = 1;
	}
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WINDOWS_DESKTOP)
#if defined(DEBUG)
	else if (wbl == console) {
		char *raw = NULL;
                size_t rawLen;
		int res;
		unsigned char c;
		DebugCmdRef dbgCmdRef;
		
		res = getchar();
		c = res;

		if (res < 0) {
			fprintf(stderr, "Could not read character: %s\n", strerror(errno));
			return;
		}
		
		//fprintf(stderr,"Character %c pressed\n", c);

		switch (c) {
			case 'c':
				dbgCmdRef = DebugCmdRef(new DebugCmd(DBG_CMD_PRINT_CERTIFICATES), "PrintCertificatesDebugCmd");
				kernel->addEvent(new Event(dbgCmdRef));
				break;
#ifdef DEBUG_DATASTORE
			case 'd':
				kernel->getDataStore()->print();
				break;
#endif
			case 'i':
				// Interface list
				//dbgCmdRef = DebugCmdRef(new DebugCmd(DBG_CMD_PRINT_INTERNAL_STATE), "PrintInterfacesDebugCmd");
				//kernel->addEvent(new Event(dbgCmdRef));
				kernel->getInterfaceStore()->print();
				break;
#ifdef DEBUG_LEAKS
			case 'l':
				LeakMonitor::reportLeaks();
				break;
#endif
			case 'm':
				kernel->printRegisteredManagers();
				break;
			case 'n':
				kernel->getNodeStore()->print();
				break;
			case 'p':
				dbgCmdRef = DebugCmdRef(new DebugCmd(DBG_CMD_PRINT_PROTOCOLS), "PrintPotocolsDebugCmd");
				kernel->addEvent(new Event(dbgCmdRef));
				break;
			case 'r':
				dbgCmdRef = DebugCmdRef(new DebugCmd(DBG_CMD_PRINT_ROUTING_TABLE), "PrintRoutingTableDebugCmd");
				kernel->addEvent(new Event(dbgCmdRef));
				break;
			case 's':
				kernel->shutdown();
				break;		
#ifdef DEBUG
			case 't':				
				printf("===============================\n");
				Thread::registryPrint();
				printf("===============================\n");
				break;
#endif
			case 'b':
				kernel->getThisNode()->getDataObject()->getMetadata()->getRawAlloc(&raw, &rawLen);
				if (raw) {
					printf("======= Node description =======\n");
						printf("%s\n", raw);
					printf("================================\n");
					free(raw);
				}
				break;
			case 'h':
			default:
				printf("========== Console help ==========\n");
				printf("The keys listed below does the following:\n");
				printf("c: Certificate list\n");
				printf("b: Node description of \'this node\'\n");		
#ifdef DEBUG_DATASTORE
				printf("d: list data store tables\n");
#endif
				printf("i: Interface list\n");
#ifdef DEBUG_LEAKS
				printf("l: Leak report\n");
#endif
				printf("m: Manager list with registered sockets\n");
				printf("n: Neighbor list\n");
				printf("p: Running protocols\n");
				printf("r: Current routing table\n");
				printf("s: Generate shutdown event (also ctrl-c)\n");
				printf("t: Print thread registry (running threads)\n");
				printf("any other key: this help\n");
				printf("==================================\n");
				break;
		}
	}
#endif
#endif
}

void DebugManager::publicEvent(Event *e)
{
	if (!e)
		return;

	switch (e->getType()) {
                case EVENT_TYPE_NEIGHBOR_INTERFACE_UP:
                case EVENT_TYPE_NEIGHBOR_INTERFACE_DOWN:
                case EVENT_TYPE_LOCAL_INTERFACE_UP:
                case EVENT_TYPE_LOCAL_INTERFACE_DOWN:
                        {
                                InterfaceRef iface = e->getInterface();
                                HAGGLE_DBG("%s data=%s\n", 
                                           e->getName(),
                                           (iface ? "yes" : "no"));
                        }
                        break;
                        
                default:
                        HAGGLE_DBG("%s data=%s\n", e->getName(), e->hasData()? "Yes" : "No");
                        break;
	}
}

SOCKET openSocket(int port)
{
	int ret;
	const int optval = -1;
	struct sockaddr_in addr;
	SOCKET sock;

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sock == INVALID_SOCKET) {
		HAGGLE_ERR("Could not create TCP socket\n");
		return INVALID_SOCKET;
	}
	
	ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(int));
	
	if (ret == SOCKET_ERROR) {
		HAGGLE_ERR("setsockopt SO_REUSEADDR failed: %s\n", STRERROR(ERRNO));
	}
		
	ret = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&optval, sizeof(int));
	
	if (ret == SOCKET_ERROR) {
		HAGGLE_ERR("setsockopt SO_KEEPALIVE failed: %s\n", STRERROR(ERRNO));
	}
	
	memset(&addr, 0, sizeof(struct sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	ret = bind(sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));

	if (ret == SOCKET_ERROR) {
		HAGGLE_ERR("Could not bind debug TCP socket\n");
		return INVALID_SOCKET;
	}

	listen(sock, 1);

	return sock;
}
#endif
