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

#if defined(ENABLE_BLUETOOTH) && !defined(OS_MACOSX) && defined(WIDCOMM_BLUETOOTH)

#include <string.h>
#include <haggleutils.h>

#include "ConnectivityBluetooth.h" // For HAGGLE_BLUETOOTH_SDP_UUID
#include "ProtocolManager.h"
#include "ProtocolRFCOMMWIDCOMM.h"
#include "HaggleKernel.h"

enum {
	QMSG_TYPE_DATA,
	QMSG_TYPE_CONNECTION_SUCCESS,
	QMSG_TYPE_CONNECTION_ERROR,
};

struct QMsg {
	UINT8 type;
	char *data;
	size_t len;
};

// There should be only one instance of rfCommIf per Bluetooth interface
CRfCommIf ProtocolRFCOMM::rfCommIf;

RFCOMMConnection::RFCOMMConnection(ProtocolRFCOMM *_p) : p(_p)
{
	HAGGLE_DBG("New RFCOMMConnection for protocol %s\n", p ? p->getName() : "Unknown");
}

RFCOMMConnection::~RFCOMMConnection()
{
	HAGGLE_DBG("RFCOMMConnection destroyed\n");
}

void RFCOMMConnection::OnDataReceived(void *p_data, UINT16 len)
{
	if (p)
		p->OnDataReceived(p_data, len);
}
void RFCOMMConnection::OnEventReceived(UINT32 event_code)
{
	if (p)
		p->OnEventReceived(event_code);
}

bool ProtocolRFCOMM::init(bool autoAssignScn)		
{
	unsigned char uuid[] = HAGGLE_BLUETOOTH_SDP_UUID;
	GUID guid;
	UINT8 scn = autoAssignScn ? 0 : channel & 0xff;

	convertUUIDBytesToGUID((char *)uuid, &guid);

	if (!rfCommIf.AssignScnValue(&guid, scn)) {
		HAGGLE_ERR("Could not assign Bluetooth channel number %d\n", channel);
		return false;
	} 
		
	channel = rfCommIf.GetScn();

	if (isServer()) {
		if (!rfCommIf.SetSecurityLevel(L"Haggle", BTM_SEC_NONE, TRUE)) {
			HAGGLE_ERR("Could not set Bluetooth security level for RFCOMM server\n");
			return false;
		}
		HAGGLE_DBG("Created RFCOMM server on channel=%d\n", channel);
	} else {
		if (!rfCommIf.SetSecurityLevel(L"Haggle", BTM_SEC_NONE, FALSE)) {
			HAGGLE_ERR("Could not set Bluetooth security level for RFCOMM server\n");
			return false;
		}
		HAGGLE_DBG("Created RFCOMM client on channel=%d\n", channel);
	}
	return true;
}

ProtocolRFCOMM::ProtocolRFCOMM(RFCOMMConnection *_rfCommConn, const char *mac, const unsigned short _channel,
			       const InterfaceRef& _localIface, const short flags, ProtocolManager *m) :
	Protocol(PROT_TYPE_RFCOMM, "ProtocolRFCOMM", _localIface, NULL, flags, m), rfCommConn(_rfCommConn), channel(_channel)
{
	Address addr(AddressType_BTMAC, (unsigned char *) mac);

	peerIface = InterfaceRef(new Interface(IFTYPE_BLUETOOTH, mac, &addr, "Peer Bluetooth", IFFLAG_UP), "InterfacePeerRFCOMM");

	rfCommConn->setProtocol(this);

	if (!init(true)) {
		HAGGLE_ERR("Could not initialize RFCOMM protocol\n");
#if HAVE_EXCEPTION
		throw ProtocolException(-1, "Could not initialize RFCOMM protocol");
#else
		return;
#endif
	}
}

ProtocolRFCOMM::ProtocolRFCOMM(const InterfaceRef& _localIface, const InterfaceRef& _peerIface, 
			       const unsigned short _channel, const short flags, ProtocolManager * m) : 
	Protocol(PROT_TYPE_RFCOMM, "ProtocolRFCOMM", _localIface, _peerIface, flags, m), 
		rfCommConn(NULL), channel(_channel)
{
	rfCommConn = new RFCOMMConnection(this);

	if (!init()) {
		HAGGLE_ERR("Could not initialize RFCOMM protocol\n");
#if HAVE_EXCEPTION
		throw ProtocolException(-1, "Could not initialize RFCOMM protocol");
#else
		return;
#endif
	}
}

ProtocolRFCOMM::~ProtocolRFCOMM()
{
	if (rfCommConn)
		delete rfCommConn;
}

#define MSG_QUEUE_MAX_DATALEN 2000 // What is a good value here? The Bluetooth mtu?

bool ProtocolRFCOMMClient::init()
{
	MSGQUEUEOPTIONS mqOpts = { sizeof(MSGQUEUEOPTIONS), MSGQUEUE_NOPRECOMMIT, 0, 
			sizeof(struct QMsg), FALSE };

	hWriteQ = CreateMsgQueue(NULL, &mqOpts);

	if (!hWriteQ) {
		HAGGLE_ERR("Could not create message queue\n");	
		return false;
	}

	mqOpts.bReadAccess = TRUE;

	hReadQ = OpenMsgQueue(GetCurrentProcess(), hWriteQ, &mqOpts);

	if (!hReadQ) {
		HAGGLE_ERR("Could not open read handle for message queue\n");	
		CloseMsgQueue(hWriteQ);
		hWriteQ = NULL;
		return false;
	}
	
	return true;
}

ProtocolRFCOMMClient::ProtocolRFCOMMClient(RFCOMMConnection *rfCommConn, BD_ADDR bdaddr, const unsigned short _channel, 
					   const InterfaceRef& _localIface, ProtocolManager *m) :
		ProtocolRFCOMM(rfCommConn, (char *)bdaddr, _channel, _localIface, PROT_FLAG_CLIENT, m), 
			hReadQ(NULL), hWriteQ(NULL), blockingTimeout(INFINITE)
{
	init();
}

ProtocolRFCOMMClient::ProtocolRFCOMMClient(const InterfaceRef& _localIface, const InterfaceRef& _peerIface, 
					   const unsigned short channel, ProtocolManager *m) :
		ProtocolRFCOMM(_localIface, _peerIface, channel, PROT_FLAG_CLIENT, m), 
			hReadQ(NULL), hWriteQ(NULL), blockingTimeout(INFINITE)
{
	init();
}


ProtocolRFCOMMClient::~ProtocolRFCOMMClient()
{
	HAGGLE_DBG("Destroying %s\n", getName());

	if (hReadQ)
		CloseMsgQueue(hReadQ);
	if (hWriteQ)
		CloseMsgQueue(hWriteQ);
}


bool ProtocolRFCOMMClient::setNonblock(bool block)
{
	if (block)
		blockingTimeout = INFINITE;
	else
		blockingTimeout = 0;

	return true;
}

void ProtocolRFCOMMClient::OnEventReceived(UINT32 event_code)
{
	struct QMsg msg;

	// Ignore connection errors if we are not yet connected.
	if (event_code & PORT_EV_CONNECT_ERR) {
		HAGGLE_ERR("Connection error event\n");

		msg.type = QMSG_TYPE_CONNECTION_ERROR;

		if (!WriteMsgQueue(hWriteQ, &msg, sizeof(struct QMsg), 500, 0)) {
			HAGGLE_ERR("Could not write connection error event to msg queue\n");
		}
	} 
	if (event_code & PORT_EV_CONNECTED) {
		HAGGLE_ERR("Client connection event\n");

		/*
			Only generate connection message once. It seems as
			if the stack generates this event multiple times for 
			each successful connection.
		*/
		if (!isConnected()) {
			setFlag(PROT_FLAG_CONNECTED);
			msg.type = QMSG_TYPE_CONNECTION_SUCCESS;

			if (!WriteMsgQueue(hWriteQ, &msg, sizeof(struct QMsg), 500, 0)) {
				HAGGLE_ERR("Could not write connection error event to msg queue\n");
			}
		}
		
		//HAGGLE_DBG("Successfully connected\n");
	} 
	if (event_code & PORT_EV_OVERRUN) {
		HAGGLE_DBG("PORT_EV_OVERRUN\n");
	}
	if (event_code & PORT_EV_ERR) {
		HAGGLE_ERR("Error event\n");
	} 

	//HAGGLE_DBG("Received event_code %u\n", event_code);
}

void ProtocolRFCOMMClient::OnDataReceived(void *p_data, UINT16 len)
{
	struct QMsg msg;

	msg.type = QMSG_TYPE_DATA;
	msg.data = new char[len];
	msg.len = len;

	memcpy(msg.data, p_data, len);

	BOOL res = WriteMsgQueue(hWriteQ, &msg, sizeof(struct QMsg), 500, 0);

	if (res == FALSE) {
		delete [] msg.data;
		HAGGLE_ERR("Could not write %u bytes data to message queue\n");
	}
}

ProtocolEvent ProtocolRFCOMMClient::connectToPeer()
{
	Address	*addr;
	BD_ADDR bdaddr;
	
	if (!peerIface)
		return PROT_EVENT_ERROR;

	addr = peerIface->getAddressByType(AddressType_BTMAC);

	if (!addr)
		return PROT_EVENT_ERROR;

	memcpy(bdaddr, addr->getRaw(), sizeof(BD_ADDR));

	CRfCommPort::PORT_RETURN_CODE ret = rfCommConn->OpenClient((UINT8)channel&0xff, bdaddr);

	if (ret != CRfCommPort::SUCCESS) {
		HAGGLE_DBG("%s Connection failed to [%s] channel=%u\n", 
			   getName(), addr->getAddrStr(), channel);
		return PROT_EVENT_ERROR;
	}
	/*
		Check for a connection msg on the message queue.
	*/
	DWORD bytes_read, flags = 0;
	struct QMsg msg;

	if (!ReadMsgQueue(hReadQ, &msg, sizeof(struct QMsg), &bytes_read, 5000, &flags)) {
		if (GetLastError() == ERROR_TIMEOUT) {
			HAGGLE_DBG("%s Connection attempt to [%s] channel=%u timed out\n", 
				getName(), addr->getAddrStr(), channel);
			return PROT_EVENT_TIMEOUT;
		}

		HAGGLE_ERR("Could not read msg queue for connection result\n");
		return PROT_EVENT_ERROR;
	}

	if (msg.type != QMSG_TYPE_CONNECTION_SUCCESS)
		return PROT_EVENT_ERROR;

	HAGGLE_DBG("%s Connected to [%s] channel=%u\n", 
		   getName(), addr->getAddrStr(), channel);

	return PROT_EVENT_SUCCESS;
}

void ProtocolRFCOMMClient::closeConnection()
{
	CRfCommPort::PORT_RETURN_CODE retcode;

	unSetFlag(PROT_FLAG_CONNECTED);

	setMode(PROT_MODE_DONE);

	retcode = rfCommConn->Close();

	if (retcode != CRfCommPort::SUCCESS && retcode != CRfCommPort::NOT_OPENED) {
		HAGGLE_ERR("Could not close connection to [%s]\n", 
			peerIface->getAddressByType(AddressType_BTMAC)->getAddrStr());
	}
}

ProtocolEvent ProtocolRFCOMMClient::receiveData(void *buf, size_t len, const int flags, size_t *bytes)
{
	DWORD bytes_read, qflags = 0;
	struct QMsg msg;
	*bytes = 0;
	
	BOOL ret = ReadMsgQueue(hReadQ, &msg, sizeof(struct QMsg), &bytes_read, blockingTimeout, &qflags);

	if (ret == FALSE) {
		HAGGLE_ERR("Could not read data from message queue, err=%d\n", GetLastError());
		if (GetLastError() == ERROR_PIPE_NOT_CONNECTED) {
			
			HAGGLE_ERR("message queue ERROR_PIPE_NOT_CONNECTED\n");
			return PROT_EVENT_PEER_CLOSED;
		}
		return PROT_EVENT_ERROR;
	} 

	if (msg.type == QMSG_TYPE_CONNECTION_SUCCESS) {
		// We should not really get this message type here since
		// it is already handled in connectToPeer(). However, lets
		// handle the type here anyway just in case and return
		// success with zero bytes read.
		return PROT_EVENT_SUCCESS;
	} else if (msg.type == QMSG_TYPE_CONNECTION_ERROR) {
		HAGGLE_ERR("Got CONNECTION_ERROR event - peer closed?\n");
		return PROT_EVENT_PEER_CLOSED;
	} else if (msg.type != QMSG_TYPE_DATA)
		return PROT_EVENT_ERROR;

	if (msg.len < msg.len) {
		delete [] msg.data;
		return PROT_EVENT_ERROR;
	}
	*bytes = msg.len;
	memcpy(buf, msg.data, msg.len);

	delete [] msg.data;

	return PROT_EVENT_SUCCESS;
}

ProtocolEvent ProtocolRFCOMMClient::sendData(const void *buf, size_t len, const int flags, size_t *bytes)
{
	UINT16 bytes_written;
	UINT16 to_write = len;

	*bytes = 0;

	CRfCommPort::PORT_RETURN_CODE ret = rfCommConn->Write((void *)buf, to_write, &bytes_written);

	if (ret == CRfCommPort::PEER_CONNECTION_FAILED)
		return PROT_EVENT_PEER_CLOSED;
	else if (ret != CRfCommPort::SUCCESS)
		return PROT_EVENT_ERROR;

	//HAGGLE_DBG("Wrote %u bytes data out of %lu requested\n", bytes_written, len);

	*bytes = (size_t)bytes_written;
	
	return PROT_EVENT_SUCCESS;
}

ProtocolEvent ProtocolRFCOMMClient::waitForEvent(Timeval *timeout, bool writeevent)
{
	Watch w;
	int ret, index;

	/* 
		Haven't found a way to check writable status. Therefore,
		if we are checking for writeability, we assume that
		we can write.
	*/

	if (writeevent)
		return PROT_EVENT_WRITEABLE;

	index = w.add(hReadQ, WATCH_STATE_READ);
	
	ret = w.wait(timeout);

	if (ret == Watch::TIMEOUT) {
		return PROT_EVENT_TIMEOUT;
	} else if (ret == Watch::FAILED)
		return PROT_EVENT_ERROR;
	else if (ret == Watch::ABANDONED)
		return PROT_EVENT_SHOULD_EXIT;

	if (w.isReadable(index))
		return PROT_EVENT_INCOMING_DATA;

	return PROT_EVENT_ERROR;
}

ProtocolEvent ProtocolRFCOMMClient::waitForEvent(DataObjectRef &dObj, Timeval *timeout, bool writeevent)
{
	QueueElement *qe = NULL;
	
	if (writeevent)
		timeout = NULL;

	QueueEvent_t qev = getQueue()->retrieve(&qe, hReadQ, timeout, false);

	switch (qev) {
	case QUEUE_TIMEOUT:
		if (writeevent)
			return PROT_EVENT_WRITEABLE;
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

ProtocolRFCOMMServer::ProtocolRFCOMMServer(const InterfaceRef& localIface, ProtocolManager *m,
					   unsigned short channel) :
	ProtocolRFCOMM(localIface, NULL, channel, PROT_FLAG_SERVER, m)
{
	connectionEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (!setListen()) {
		HAGGLE_ERR("Could not listen on channel %d\n", channel);
		CloseHandle(connectionEvent);
		connectionEvent = NULL;
#if HAVE_EXCEPTION
		throw ProtocolException(-1, "Could not listen on channel");
#else
		return;
#endif
	}
	
	getKernel()->registerWatchable(connectionEvent, m);
}

ProtocolRFCOMMServer::~ProtocolRFCOMMServer()
{
	HAGGLE_DBG("Destroying %s\n", getName());
	
	getKernel()->unregisterWatchable(connectionEvent);
	
	CloseHandle(connectionEvent);

	if (rfCommConn)
		rfCommConn->Close();
}

bool ProtocolRFCOMMServer::hasWatchable(const Watchable &wbl)
{
	return wbl == connectionEvent;
}

void ProtocolRFCOMMServer::handleWatchableEvent(const Watchable &wbl)
{
	if (wbl != connectionEvent) {
		HAGGLE_ERR("ERROR! : %s does not belong to Protocol %s\n", wbl.getStr(), getName());
		return;
	}

	HAGGLE_DBG("Connection attempt\n");
	acceptClient();
}

bool ProtocolRFCOMMServer::setListen(int dummy)
{
	CRfCommPort::PORT_RETURN_CODE retcode;

	retcode = rfCommConn->OpenServer(channel & 0xff);

	if (retcode != CRfCommPort::SUCCESS)  {
		HAGGLE_ERR("Could not open server port %u, err=%d\n", channel, retcode);

		switch (retcode) {
		case CRfCommPort::ALREADY_OPENED:
			HAGGLE_ERR("port already opened\n");
			break;
		case CRfCommPort::INVALID_PARAMETER:
			HAGGLE_ERR("invalid parameter\n");
			break;
		case CRfCommPort::UNKNOWN_ERROR:
			HAGGLE_ERR("unknown error\n");
			break;
		case CRfCommPort::NOT_OPENED:
			HAGGLE_ERR("not opened\n");
			break;
		}
		return false;
	}

	setMode(PROT_MODE_LISTENING);

	HAGGLE_DBG("Listening for connections on channel %u\n", channel);

	return true;
}

ProtocolEvent ProtocolRFCOMMServer::acceptClient()
{
	if (getMode() != PROT_MODE_LISTENING) {
		HAGGLE_ERR("%s: cannot accept connection on non-listening protocol\n", getName());
		return PROT_EVENT_ERROR;
	}

	ProtocolRFCOMMClient *p = new ProtocolRFCOMMReceiver(rfCommConn, clientBdAddr, 
		(unsigned short)channel, this->getLocalInterface(), getManager());

	if (!p)
		return PROT_EVENT_ERROR;

	p->setFlag(PROT_FLAG_CONNECTED);

	HAGGLE_DBG("Accepted client %s, starting client thread\n", 
			p->getPeerInterface()->getAddressByType(AddressType_BTMAC)->getAddrStr());
	
	p->registerWithManager();

	ProtocolEvent pev = p->startTxRx();
	
	rfCommConn = new RFCOMMConnection(this);

	if (!rfCommConn) {
		delete p;
		return PROT_EVENT_ERROR;
	}

	if (!setListen()) {
		setMode(PROT_MODE_IDLE);
		HAGGLE_ERR("Could not set listen\n");
	}
	
	HAGGLE_DBG("Started new RFCOMM server\n");

	return pev;
}

void ProtocolRFCOMMServer::OnEventReceived(UINT32 event_code)
{
	if (event_code & PORT_EV_CONNECT_ERR) {
		HAGGLE_ERR("Connection Error - close...\n");
	} 
	if (event_code & PORT_EV_CONNECTED) {
		BD_ADDR bdaddr;
		HAGGLE_DBG("Connection Event\n");

		if (rfCommConn->IsConnected(&bdaddr)) {
			Watch w;
			HAGGLE_DBG("Server: is connected\n");
			/* 
				Since the stack seems to generate multiple connection
				events for each connection, we check if this is a new
				or old connection event. Otherwise we risk signalling
				a new connection multiple times, which confuses the
				server.
			*/
			w.add(connectionEvent);
			Timeval timeout = 0;

			if (w.wait(&timeout) == Watch::TIMEOUT) {
				memcpy(clientBdAddr, bdaddr, sizeof(BD_ADDR));
				SetEvent(connectionEvent);
			}
		} else {
			HAGGLE_ERR("Not connected\n");
		}
	} 
	if (event_code & PORT_EV_OVERRUN) {
		HAGGLE_DBG("PORT_EV_OVERRUN\n");
	}
	if (event_code & PORT_EV_ERR) {
		HAGGLE_ERR("Error event\n");
	} 
	HAGGLE_DBG("Received event_code %u\n", event_code);
}

#endif /* ENABLE_BLUETOOTH && !OS_MACOSX */
