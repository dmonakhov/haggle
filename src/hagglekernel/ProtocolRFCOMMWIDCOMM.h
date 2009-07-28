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
#ifndef _PROTOCOLRFCOMMWIDCOMM_H
#define _PROTOCOLRFCOMMWIDCOMM_H

#include <libcpphaggle/Platform.h>

#if defined(ENABLE_BLUETOOTH) && defined(WIDCOMM_BLUETOOTH)

#include <BtSdkCE.h>
#include "Interface.h"
#include "DataObject.h"
#include "Protocol.h"
#include "ProtocolRFCOMM.h"

class ProtocolRFCOMM;

/**
	RFCOMMConnection is the handle for a rfcomm connection.
	One instance of this class is associated with each
	instance of ProtocolRFCOMM.

	The only thing it does is to relay any incoming events
	to the currently associated ProtocolRFCOMM instance.
*/
class RFCOMMConnection : public CRfCommPort {
	ProtocolRFCOMM *p;
public:
	void setProtocol(ProtocolRFCOMM *_p) { p = _p; } 
	RFCOMMConnection(ProtocolRFCOMM *_p = NULL);
	~RFCOMMConnection();
	void OnDataReceived(void *p_data, UINT16 len);
	void OnEventReceived(UINT32 event_code);
};

/** */
class ProtocolRFCOMM : public Protocol
{
	friend class RFCOMMConnection;
protected:
	static CRfCommIf rfCommIf;
	RFCOMMConnection *rfCommConn;
        unsigned short channel;
	bool init(bool autoAssignScn = false);

	virtual void OnDataReceived(void *p_data, UINT16 len) {}
	virtual void OnEventReceived(UINT32 event_code) {}

        ProtocolRFCOMM(RFCOMMConnection *rfCommConn, const char *_mac, const unsigned short _channel, 
		const InterfaceRef& _localIface, const short flags = PROT_FLAG_CLIENT, 
		ProtocolManager *m = NULL);

        ProtocolRFCOMM(const InterfaceRef& _localIface, const InterfaceRef& _peerIface, 
		const unsigned short channel = RFCOMM_DEFAULT_CHANNEL, 
		const short flags = PROT_FLAG_CLIENT, ProtocolManager *m = NULL);

        virtual ~ProtocolRFCOMM();
};

/** */
class ProtocolRFCOMMClient : public ProtocolRFCOMM
{
        friend class ProtocolRFCOMMServer;
	HANDLE hReadQ, hWriteQ;
	DWORD blockingTimeout;
	bool init();
	void OnDataReceived(void *p_data, UINT16 len);
	void OnEventReceived(UINT32 event_code);
	//ProtocolEvent waitForConnection();
public:
	ProtocolRFCOMMClient(RFCOMMConnection *rfcommConn, BD_ADDR bdaddr, const unsigned short _channel, 
		const InterfaceRef& _localIface, ProtocolManager *m = NULL);
	ProtocolRFCOMMClient(const InterfaceRef& _localIface, const InterfaceRef& _peerIface, 
		const unsigned short channel = RFCOMM_DEFAULT_CHANNEL, ProtocolManager *m = NULL);
	~ProtocolRFCOMMClient();
	bool setNonblock(bool block = false);
        ProtocolEvent connectToPeer();
	void closeConnection();
   
	/**
           Functions that are overridden from class Protocol.
	*/
	ProtocolEvent waitForEvent(Timeval *timeout = NULL, bool writeevent = false);
	ProtocolEvent waitForEvent(DataObjectRef &dObj, Timeval *timeout = NULL, bool writeevent = false);
	
	ProtocolEvent receiveData(void *buf, size_t len, const int flags, size_t *bytes);
	ProtocolEvent sendData(const void *buf, size_t len, const int flags, size_t *bytes);
};

/** */
class ProtocolRFCOMMSender : public ProtocolRFCOMMClient
{
public:
	ProtocolRFCOMMSender(const InterfaceRef& _localIface, 
		const InterfaceRef& _peerIface, 
		const unsigned short channel = RFCOMM_DEFAULT_CHANNEL, 
		ProtocolManager *m = NULL) :
	ProtocolRFCOMMClient(_localIface, _peerIface, channel, m) {}
	virtual bool isSender() { return true; }
};

/** */
class ProtocolRFCOMMReceiver : public ProtocolRFCOMMClient
{
	friend class ProtocolRFCOMMServer;
	ProtocolRFCOMMReceiver(RFCOMMConnection *rfCommConn, BD_ADDR bdaddr, 
		const unsigned short _channel, const InterfaceRef& _localIface, 
		ProtocolManager *m = NULL) : ProtocolRFCOMMClient(rfCommConn, bdaddr, _channel, _localIface, m) {}	
public:
	virtual bool isReceiver() { return true; }
};

/** */
class ProtocolRFCOMMServer : public ProtocolRFCOMM
{
	friend class ProtocolRFCOMM;
	friend class RFCOMMConnection;
	HANDLE connectionEvent;
	BD_ADDR clientBdAddr;

	void OnEventReceived(UINT32 event_code);

	bool setListen(int dummy = 0);
public:
        ProtocolRFCOMMServer(const InterfaceRef& localIface = NULL, ProtocolManager *m = NULL,
                             unsigned short channel = RFCOMM_DEFAULT_CHANNEL);
        ~ProtocolRFCOMMServer();
	bool hasWatchable(const Watchable &wbl);
	void handleWatchableEvent(const Watchable &wbl);

        ProtocolEvent acceptClient();
};

#endif
#endif /* _PROTOCOLRFCOMMWIDCOMMM_H */
