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
#include <libcpphaggle/PlatformDetect.h>

#if !defined(OS_WINDOWS)

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libcpphaggle/Exception.h>
#include <haggleutils.h>

#include "ProtocolLOCAL.h"

ProtocolLOCAL::ProtocolLOCAL(const char *path, ProtocolManager * m) : 
	ProtocolSocket(PROT_TYPE_LOCAL, "ProtocolLOCAL", NULL, NULL,
		       PROT_FLAG_SERVER | PROT_FLAG_CLIENT, m)
{
	if (!openSocket(PF_LOCAL, SOCK_DGRAM, 0), true) {
#if HAVE_EXCEPTION
		throw ProtocolSocket::SocketException(-1, "Could not create Haggle control socket\n");
#else
                return;
#endif
	}

	if (strlen(path) > sizeof(un_addr.sun_path))
                return;

	memset(&un_addr, 0, sizeof(struct sockaddr_un));

	un_addr.sun_family = PF_LOCAL;
	strcpy(un_addr.sun_path, path);

	if (!bindSocket((struct sockaddr *) &un_addr, sizeof(struct sockaddr_un))) {
		unlink(path);
		closeSocket();
#if HAVE_EXCEPTION
		throw BindException(-1, "Could not bind Haggle control socket\n");
#else
                return;
#endif
	}

	Address addr(un_addr.sun_path);

	localIface = new Interface(IFTYPE_APPLICATION_LOCAL, un_addr.sun_path, &addr, "Application", IFFLAG_UP);

#if HAVE_EXCEPTION
	if (!localIface)
		throw ProtocolException(-1, "Could not create local Protocol interface");
#endif
}

ProtocolLOCAL::~ProtocolLOCAL()
{
	unlink(un_addr.sun_path);
}

ProtocolEvent ProtocolLOCAL::receiveDataObject()
{
	int len = 0;
	DataObject *dObj;
	struct sockaddr_un peer_addr;
	ProtocolEvent pEvent;

	pEvent = receiveData(buffer, bufferSize, &peer_addr, MSG_DONTWAIT, &len);

	if (pEvent != PROT_EVENT_SUCCESS)
		return pEvent;

// FIXME: fix this:
#if 0
        Interface peerIface(localIface->getType(), NULL, NULL, -1, "ApplicationInterface", IFFLAG_UP);
        dObj = new DataObject(buffer, len, &peerIface);
#else
	dObj = NULL;
#endif
        if (!dObj) {
		HAGGLE_DBG("%s:%lu Could not create data object\n", getName(), getId());
		return PROT_EVENT_ERROR;
	}
	
	
	// Generate first an incoming event to conform with the base Protocol class
	getKernel()->addEvent(new Event(EVENT_TYPE_DATAOBJECT_INCOMING, dObj));
	
	// Since there is no data following, we generate the received event immediately 
	// following the incoming one
	getKernel()->addEvent(new Event(EVENT_TYPE_DATAOBJECT_RECEIVED, dObj));

	return PROT_EVENT_SUCCESS;
}

int ProtocolLOCAL::sendSingleDataObject(const DataObjectRef& dObj, const InterfaceRef& _peerIface)
{
	int ret;

	if (peerIface)
		return -1;
	
	peerIface = _peerIface;

	/* Call send function */
	ret = sendDataObjectNow(dObj);

	peerIface = NULL;

	return ret;
}

ProtocolEvent ProtocolLOCAL::sendData(const void *buf, const int len, const int flags, int *bytes)
{
	if (!buf || !peerIface)
		return PROT_EVENT_ERROR;

	*bytes = sendTo(buf, len, flags, (struct sockaddr *) &un_addr, sizeof(un_addr));

	if (*bytes < 0) {
		*bytes = 0;
		return PROT_EVENT_ERROR;
	} else if (*bytes == 0)
		return PROT_EVENT_PEER_CLOSED;

	return PROT_EVENT_SUCCESS;
}

ProtocolEvent ProtocolLOCAL::receiveData(void *buf, const int buflen, struct sockaddr_un *addr, const int flags, int *bytes)
{
	socklen_t addrlen = sizeof(struct sockaddr_un);

	memset(addr, 0, sizeof(struct sockaddr_un));

	addr->sun_family = PF_LOCAL;

	*bytes = recvFrom(buf, buflen, flags, (struct sockaddr *) addr, &addrlen);
	
	if (*bytes < 0) {
		*bytes = 0;
		return PROT_EVENT_ERROR;
	} else if (*bytes == 0)
		return PROT_EVENT_PEER_CLOSED;

	return PROT_EVENT_SUCCESS;
}

#endif
