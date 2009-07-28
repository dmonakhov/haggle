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
#ifndef _PROTOCOLLOCAL_H
#define _PROTOCOLLOCAL_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/

class ProtocolLOCAL;

#include "ProtocolSocket.h"

/** */
class ProtocolLOCAL : public ProtocolSocket
{
private:
        struct sockaddr_un un_addr;

	ProtocolEvent receiveDataObject();
        ProtocolEvent sendData(const void *buf, const int buflen, const int flags, int *bytes);
        ProtocolEvent receiveData(void *buf, const int buflen, struct sockaddr_un *addr, const int flags, int *bytes);
public:
        ProtocolLOCAL(const char *_path, ProtocolManager *m);
        ~ProtocolLOCAL();
        int sendSingleDataObject(const DataObjectRef& dObj, const InterfaceRef& _peerIface);
};

#endif /* PROTOCOLLOCAL_H */
