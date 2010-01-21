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

#include "Protocol.h"

#if defined(OS_LINUX)
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#elif defined(OS_MACOSX)
#include <unistd.h>
#include <sys/uio.h>
#include <fcntl.h>
#else 
// OS_WINDOWS
#endif

// This include is for the HAGGLE_ATTR_CTRL_NAME attribute name. 
// TODO: remove dependency on libhaggle header
#include "../libhaggle/include/libhaggle/ipc.h"


/*
  -------------------------------------------------------------------------------
  Protocol code
*/
unsigned int Protocol::num = 0;

const char *Protocol::typestr[] ={
	"LOCAL",
	"UDP",
	"TCP",
	"RAW",
#ifdef OMNETPP
	"OMNET++",
#endif
#if defined(ENABLE_BLUETOOTH)
	"RFCOMM",
#endif
#if defined(ENABLE_MEDIA)
	"MEDIA",
#endif
	NULL
};

static inline char *create_name(const char *name, const unsigned long id, const int flags)
{
	static char tmpStr[40];
	
	snprintf(tmpStr, 40, "%s%s:%lu", name, flags & PROT_FLAG_SERVER ? "Server" : "Client", id);
	
	return tmpStr;
}

Protocol::Protocol(const ProtType_t _type, const string _name, const InterfaceRef& _localIface, 
		   const InterfaceRef& _peerIface, const int _flags, ProtocolManager *_m, size_t _bufferSize) : 
	ManagerModule<ProtocolManager>(_m, create_name(_name.c_str(), num + 1,  _flags)),
	isRegistered(false), type(_type), id(num++), error(PROT_ERROR_UNKNOWN), flags(_flags), 
	mode(PROT_MODE_IDLE), localIface(_localIface), peerIface(_peerIface), buffer(NULL),
	bufferSize(_bufferSize), bufferDataLen(0)
{
	HAGGLE_DBG("%s Buffer size is %lu\n", getName(), bufferSize);
}

bool Protocol::init()
{
	if (buffer)
		return false;
	
	buffer = new unsigned char[bufferSize];

	if (!buffer) {
		HAGGLE_ERR("Could not allocate buffer of size %lu\n", bufferSize);
		return false;
	}
	
	return init_derived();
}

Protocol::~Protocol()
{
	if (isRegistered) {
		HAGGLE_ERR("ERROR: deleting still registered protocol %s\n", getName());
	}
	
	HAGGLE_DBG("%s destroyed\n", getName());
	
	// If there is anything in the queue, these should be data objects, and if 
	// they are here, they have not been sent. So send an
	// EVENT_TYPE_DATAOBJECT_SEND_FAILURE for each of them:
	
	// Find the target (always the same):
	NodeRef target = getKernel()->getNodeStore()->retrieve(peerIface);
	
	// No need to do getQueue() more than once:
	Queue *q = getQueue();
	
	if (!q)
		return;

	// With all the data objects in the queue:
	while (!q->empty()) {
		QueueElement *qe = NULL;
		DataObjectRef dObj;
		
		// Get the first element. Don't delay:
		switch (q->retrieveTry(&qe)) {
			default:
				break;
			case QUEUE_ELEMENT:
				// Get data object:
				dObj = qe->getDataObject();

				if (dObj) {
					// Tell the rest of haggle that this data object was not 
					// sent:
					getKernel()->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND_FAILURE, dObj, target));
				}
				break;
		}
		// Delete the queue element:
		if (qe)
			delete qe;
	}
	
	if (buffer)
		delete[] buffer;
}

bool Protocol::hasIncomingData()
{
	return (waitForEvent() == PROT_EVENT_INCOMING_DATA);
}

ProtocolEvent Protocol::waitForEvent(Timeval *timeout, bool writeevent)
{
	Watch w;
	int ret;

	ret = w.wait(timeout);
	
	if (ret == Watch::TIMEOUT)
		return PROT_EVENT_TIMEOUT;
	else if (ret == Watch::FAILED)
		return PROT_EVENT_ERROR;

	return PROT_EVENT_INCOMING_DATA;
}

ProtocolEvent Protocol::waitForEvent(DataObjectRef& dObj, Timeval *timeout, bool writeevent)
{	
	QueueElement *qe = NULL;
	Queue *q = getQueue();

	if (!q)
		return PROT_EVENT_ERROR;

	QueueEvent_t qev = q->retrieve(&qe, timeout);

	switch (qev) {
	case QUEUE_TIMEOUT:
		return  PROT_EVENT_TIMEOUT;
	case QUEUE_ELEMENT:
		dObj = qe->getDataObject();
		delete qe;
		return PROT_EVENT_TXQ_NEW_DATAOBJECT;
	default:
		break;
	}
	return PROT_EVENT_ERROR;
}

void Protocol::closeConnection()
{
	return;
}

bool Protocol::isForInterface(const InterfaceRef& the_iface)
{
	if (!the_iface)
		return false;

	if (localIface && the_iface == localIface)
		return true;

	if (peerIface && the_iface == peerIface)
		return true;

	return false;
}

const unsigned long Protocol::getId() const 
{
	return id;
} 

const ProtType_t Protocol::getType() const 
{
	return type;
} 

void Protocol::handleInterfaceDown(const InterfaceRef& iface)
{
	if (localIface == iface)
		shutdown();
	else if (peerIface && peerIface == iface)
		shutdown();
}

bool Protocol::isSender() 
{
	return false;
}

bool Protocol::isReceiver()
{
	return false;
}

bool Protocol::hasWatchable(const Watchable &wbl)
{
	return false;
}

void Protocol::handleWatchableEvent(const Watchable &wbl)
{
}


// Function to translate platform specific errors into something more
// useful

#if defined(OS_LINUX) || defined (OS_MACOSX)
ProtocolError Protocol::getProtocolError()
{
	switch (errno) {
	case EAGAIN:
		error = PROT_ERROR_WOULD_BLOCK;
		break;
	case EBADF:
		error = PROT_ERROR_BAD_HANDLE;
		break;
	case ECONNREFUSED:
		error = PROT_ERROR_CONNECTION_REFUSED;
		break;
	case EINTR:
		error = PROT_ERROR_INTERRUPTED;
		break;
	case EINVAL:
		error = PROT_ERROR_INVALID_ARGUMENT;
		break;
	case ENOMEM:
		error = PROT_ERROR_NO_MEMORY;
		break;
	case ENOTCONN:
		error = PROT_ERROR_NOT_CONNECTED;
		break;
	case ECONNRESET:
		error = PROT_ERROR_CONNECTION_RESET;
		break;
	case ENOTSOCK:
		error = PROT_ERROR_NOT_A_SOCKET;
		break;
	case ENOSPC:
		error = PROT_ERROR_NO_STORAGE_SPACE;
		break;
	default:
		error = PROT_ERROR_UNKNOWN;
		break;
	}

	return error;
}
const char *Protocol::getProtocolErrorStr()
{
	// We could append the system error string from strerror
	//return (error < _PROT_ERROR_MAX && error > _PROT_ERROR_MIN) ? errorStr[error] : "Bad error";
	return strerror(errno);
}
int Protocol::getFileError()
{
	return getProtocolError();
}
const char *Protocol::getFileErrorStr()
{
	// We could append the system error string from strerror
	//return (error < _PROT_ERROR_MAX && error > _PROT_ERROR_MIN) ? errorStr[error] : "Bad error";
	return strerror(errno);
}
#else

int Protocol::getFileError()
{
	switch (GetLastError()) {
	case ERROR_INVALID_HANDLE:
		error = PROT_ERROR_BAD_HANDLE;
		break;
	case ERROR_NOT_ENOUGH_MEMORY:
		error = PROT_ERROR_NO_MEMORY;
		break;
	case ERROR_OUTOFMEMORY:
		error = PROT_ERROR_NO_STORAGE_SPACE;
	default:
		error = PROT_ERROR_UNKNOWN;
		break;
	}

	return error;
}
const char *Protocol::getFileErrorStr()
{
	static char *errStr = NULL;
	LPVOID lpMsgBuf;

	if (errStr)
		delete [] errStr;

	DWORD len = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				  NULL,
				  GetLastError(),
				  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				  (LPTSTR) & lpMsgBuf,
				  0,
				  NULL);
	if (len) {
		errStr = new char[len + 1];
		sprintf(errStr, "%s", reinterpret_cast < TCHAR * >(lpMsgBuf));
		LocalFree(lpMsgBuf);
	}
	return errStr;
	//return (error < _PROT_ERROR_MAX && error > _PROT_ERROR_MIN) ? errorStr[error] : "Bad error";
}

ProtocolError Protocol::getProtocolError()
{
	// TODO: How can we differ between WSAGetLastError() and GetLastError()?

	switch (WSAGetLastError()) {
	case WSAEWOULDBLOCK:
	case WSAEINPROGRESS:
		error = PROT_ERROR_WOULD_BLOCK;
		break;
	case WSA_INVALID_HANDLE:
	case WSAEBADF:
		error = PROT_ERROR_BAD_HANDLE;
		break;
	case WSAECONNREFUSED:
		error = PROT_ERROR_CONNECTION_REFUSED;
		break;
	case WSAEINTR:
		error = PROT_ERROR_INTERRUPTED;
		break;
	case WSAEINVAL:
		error = PROT_ERROR_INVALID_ARGUMENT;
		break;
	case WSA_NOT_ENOUGH_MEMORY:
		error = PROT_ERROR_NO_MEMORY;
		break;
	case WSAENOTCONN:
		error = PROT_ERROR_NOT_CONNECTED;
		break;
	case WSAECONNRESET:
		error = PROT_ERROR_CONNECTION_RESET;
		break;
	case WSAENOTSOCK:
		error = PROT_ERROR_NOT_A_SOCKET;
		break;
	default:
		error = PROT_ERROR_UNKNOWN;
		break;
	}

	return error;
}

const char *Protocol::getProtocolErrorStr()
{
	static char *errStr = NULL;
	static const char *unknownErrStr = "Unknown error";
	LPVOID lpMsgBuf;

	DWORD len = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				  NULL,
				  WSAGetLastError(),
				  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				  (LPTSTR) & lpMsgBuf,
				  0,
				  NULL);
	if (len) {
		if (errStr && (strlen(errStr) < len)) {
			delete [] errStr;
			errStr = NULL;
		}
		if (errStr == NULL)
			errStr = new char[len + 1];

		sprintf(errStr, "%s", reinterpret_cast < TCHAR * >(lpMsgBuf));
		LocalFree(lpMsgBuf);

		return errStr;
	} 

	return unknownErrStr;
}
#endif


#if defined(OS_WINDOWS_XP) && !defined(DEBUG)
// This is here to avoid a warning with catching the exception in the function below.
#pragma warning( push )
#pragma warning( disable: 4101 )
#endif
ProtocolEvent Protocol::startTxRx()
{
	if (isClient()) {
		if (isRunning()) {
			HAGGLE_DBG("%s Client thread already running!\n", getName());
			return PROT_EVENT_SUCCESS;
		}
		
		return start() ? PROT_EVENT_SUCCESS : PROT_EVENT_ERROR;
	}
	HAGGLE_DBG("%s Client flag not set\n", getName());
	return PROT_EVENT_ERROR;
}

#if defined(OS_WINDOWS_XP) && !defined(DEBUG)
#pragma warning( pop )
#endif

#define PROTOCOL_RECVSEND_TIMEOUT 20 // Seconds to wait for read or write status on a connection

/* Convenience function to send a single data object. Adds a data
   object to the queue and starts the thread.  This will only work if
   the protocol is in idle mode, i.e., it was just created. */
bool Protocol::sendDataObject(const DataObjectRef& dObj, const NodeRef& peer, const InterfaceRef& iface)
{
	Queue *q = getQueue();

	if (!q) {
		HAGGLE_ERR("No valid Queue for protocol %s\n", getName());
		return false;
	}
	if (isGarbage()) {
		HAGGLE_DBG("Send DataObject on garbage protocol %s, trying to recycle?\n", getName());
		return false;
	}
	
	// We do not have to care about the interface here since we assume the protocol
	// is already connected to a peer interface.
	QueueElement *qe = new QueueElement(dObj, peer, iface);

	if (!qe)
		return false;

	if (!q->insert(qe)) {
		delete qe;
		return false;
	}
	
	startTxRx();

	return true;
}

ProtocolEvent Protocol::getData(size_t *bytesRead)
{
	long readLen = 0;
	ProtocolEvent pEvent = PROT_EVENT_SUCCESS;
	Timeval waitTimeout = PROTOCOL_RECVSEND_TIMEOUT; // FIXME: Set suitable timeout
	int blockCount = 0;
	
        while (pEvent == PROT_EVENT_SUCCESS) {
                pEvent = waitForEvent(&waitTimeout);
                
                if (pEvent == PROT_EVENT_INCOMING_DATA) {
                        
                        readLen = bufferSize - bufferDataLen;
                        
                        pEvent = receiveData(buffer, readLen, 0, bytesRead);
                        
                        if (pEvent == PROT_EVENT_ERROR) {
                                switch (getProtocolError()) {
                                        case PROT_ERROR_BAD_HANDLE:
                                        case PROT_ERROR_NOT_CONNECTED:
                                        case PROT_ERROR_NOT_A_SOCKET:
                                        case PROT_ERROR_CONNECTION_RESET:
                                                return PROT_EVENT_ERROR_FATAL;
                                        case PROT_ERROR_WOULD_BLOCK:
                                                if (blockCount++ < PROT_BLOCK_TRY_MAX) {
                                                        // Set event to success so that the loop does not quit
                                                        pEvent = PROT_EVENT_SUCCESS;

                                                        /*
                                                          On Windows mobile, we repeatedly get a WSAEWOULDBLOCK error
                                                          here when runnig on over RFCOMM. The protocol probably reads 
                                                          faster than data arrives over the bluetooth channel.
                                                          Even if the socket is set to blocking mode by default,
                                                          we get WSAEWOULDBLOCK anyway. I guess, for now, we just
                                                          have to live with regular errors and just let the protocol
                                                          try again after a short sleep time. 

                                                          In order to avoid repeated debug messages here as data is
                                                          received, we only print the debug message once we reach
                                                          max block count in the case of Windows mobile.
                                                        */
#ifdef OS_WINDOWS_MOBILE
                                                        if (blockCount == PROT_BLOCK_TRY_MAX){
                                                                HAGGLE_DBG("Receive would block, try number %d/%d\n",
                                                                           blockCount, PROT_BLOCK_TRY_MAX);
                                                        }
#else
                                                        HAGGLE_DBG("Receive would block, try number %d/%d in %.3lf seconds\n",
                                                                   blockCount, PROT_BLOCK_TRY_MAX, 
                                                                   (double)PROT_BLOCK_SLEEP_TIME_MSECS / 1000);
#endif

                                                        cancelableSleep(PROT_BLOCK_SLEEP_TIME_MSECS);

                                                } else {
							// Make sure we exit when we have tried enough
                                                        pEvent = PROT_EVENT_ERROR_FATAL;
						
                                                }
                                                break;
                                        default:
                                                HAGGLE_ERR("Protocol error : %s\n", STRERROR(ERRNO));
                                }
                        } else if (pEvent == PROT_EVENT_PEER_CLOSED) {
                                HAGGLE_DBG("%s - peer closed connection\n", getName());
                        }

                        if (*bytesRead > 0) {
                                // This indicates a successful read
                                bufferDataLen += *bytesRead;
                                break;
                        }
                } else if (pEvent == PROT_EVENT_TIMEOUT) {
                        HAGGLE_DBG("Protocol %s timed out while getting data\n", getName());
                } 
        }
	return pEvent;
}

void Protocol::removeData(size_t len)
{
	size_t i;
	
	// Make sure there is something to do:
	if (len <= 0)
		return;
	
	// Will any bytes be left after the move?
	if (len >= bufferDataLen) {
		// No? Then we don't need to move any bytes.
		bufferDataLen = 0;
		return;
	}
	
	// Move the bytes left over to where they should be:
	for (i = 0; i < bufferDataLen-len; i++)
		buffer[i] = buffer[i+len];
	// Adjust bufferDataLen:
	bufferDataLen -= len;
}

const string Protocol::ctrlmsgToStr(struct ctrlmsg *m) const
{
        if (!m)
                return "Bad control message";

        // Return the right string based on type:
        switch (m->type) {
                case CTRLMSG_TYPE_ACK:
			return "ACK";
                case CTRLMSG_TYPE_ACCEPT:
                        return "ACCEPT";
                case CTRLMSG_TYPE_REJECT:
			return "REJECT";
		case CTRLMSG_TYPE_TERMINATE:
			return "TERMINATE";
		default:
		{
			char buf[30];
                        snprintf(buf, 30, "Unknown message type=%u", m->type);
			return buf;
		}
        }
        // Shouldn't be able to get here, but still...
        return "Bad control message";
}


ProtocolEvent Protocol::sendControlMessage(struct ctrlmsg *m)
{
	ProtocolEvent pEvent;
	size_t bytesRead;
	Timeval waitTimeout = PROTOCOL_RECVSEND_TIMEOUT; // FIXME: Set suitable timeout

	pEvent = waitForEvent(&waitTimeout, true);
	
	if (pEvent == PROT_EVENT_WRITEABLE) {
		pEvent = sendData(m, sizeof(struct ctrlmsg), 0, &bytesRead);
		
		if (pEvent == PROT_EVENT_SUCCESS) {
			HAGGLE_DBG("Sent %u bytes control message '%s'\n", bytesRead, ctrlmsgToStr(m).c_str());
		} else if (pEvent == PROT_EVENT_PEER_CLOSED) {
			HAGGLE_ERR("Could not send control message '%s': peer closed\n", ctrlmsgToStr(m).c_str());
		} else {
			switch (getProtocolError()) {
				case PROT_ERROR_BAD_HANDLE:
				case PROT_ERROR_NOT_CONNECTED:
				case PROT_ERROR_NOT_A_SOCKET:
				case PROT_ERROR_CONNECTION_RESET:
					pEvent = PROT_EVENT_ERROR_FATAL;
					break;
				default:
					break;
			}
		}
	} else if (pEvent == PROT_EVENT_TIMEOUT) {
		HAGGLE_DBG("Protocol timed out while waiting to send control message '%s'\n", ctrlmsgToStr(m).c_str());
	} else {
		HAGGLE_ERR("Protocol was not writeable when sending control message '%s'\n", ctrlmsgToStr(m).c_str());
	}

        return pEvent;
}

ProtocolEvent Protocol::receiveControlMessage(struct ctrlmsg *m)
{
	int blockCount = 0;
	ProtocolEvent pEvent = PROT_EVENT_SUCCESS;
	Timeval waitTimeout;
	
	// Repeat until we get a byte/"message" or permanently fail:
	while (pEvent == PROT_EVENT_SUCCESS) {
		waitTimeout = PROTOCOL_RECVSEND_TIMEOUT; // FIXME: Set suitable timeout
		// Wait for there to be some readable data:
		pEvent = waitForEvent(&waitTimeout);
		
		// Check return value:
		if (pEvent == PROT_EVENT_TIMEOUT) {
                        HAGGLE_DBG("Got a timeout while waiting for control message\n");
		} else if (pEvent == PROT_EVENT_INCOMING_DATA) {
			// Incoming data, this is what we've been waiting for:
			size_t bytesReceived;
			
			// Get the message:
			pEvent = receiveData(m, sizeof(struct ctrlmsg), 0, &bytesReceived);

			// Did we get it?
			if (pEvent == PROT_EVENT_SUCCESS) {
				if (bytesReceived != sizeof(struct ctrlmsg)) {
					pEvent = PROT_EVENT_ERROR;
					HAGGLE_ERR("Control message has bad size %lu, expected %lu\n", 
						bytesReceived, sizeof(struct ctrlmsg));
				} else {
					HAGGLE_DBG("Got control message '%s', %lu bytes\n", 
						ctrlmsgToStr(m).c_str(), bytesReceived);
				}
                                break;
                        } else if (pEvent == PROT_EVENT_ERROR) {
                                switch (getProtocolError()) {
                                        case PROT_ERROR_BAD_HANDLE:
                                        case PROT_ERROR_NOT_CONNECTED:
                                        case PROT_ERROR_NOT_A_SOCKET:
                                        case PROT_ERROR_CONNECTION_RESET:
                                                // These are fatal errors:
                                                pEvent = PROT_EVENT_ERROR_FATAL;
                                                
                                                // We didn't receive the message
                                                // Unknown error, assume fatal:
                                                HAGGLE_ERR("Protocol error : %s\n", getProtocolErrorStr());
                                                break;
                                        case PROT_ERROR_WOULD_BLOCK:
                                                // This is a non-fatal error, but after a number of retries we
                                                // give up.
                                                if (blockCount++ > PROT_BLOCK_TRY_MAX) {
                                                        
#ifdef OS_WINDOWS_MOBILE
                                                        // Windows mobile: this appears to happen a lot, so we reduce
                                                        // the amount of output:
                                                        HAGGLE_DBG("Receive would block, try number %d/%d\n",
                                                                   blockCount, PROT_BLOCK_TRY_MAX);
#endif
                                                        break;
                                                }
                                                
#ifndef OS_WINDOWS_MOBILE
                                                HAGGLE_DBG("Receive would block, try number %d/%d in %.3lf seconds\n",
                                                           blockCount, PROT_BLOCK_TRY_MAX, 
                                                           (double)PROT_BLOCK_SLEEP_TIME_MSECS / 1000);
#endif
                                                
                                                cancelableSleep(PROT_BLOCK_SLEEP_TIME_MSECS);
                                                pEvent = PROT_EVENT_SUCCESS;
                                                break;
                                        default:
                                                // Unknown error, assume fatal:
                                                HAGGLE_ERR("Protocol error : %s\n", getProtocolErrorStr());
                                                break;
                                }
			} else if (pEvent == PROT_EVENT_PEER_CLOSED) {
                                HAGGLE_DBG("%s Peer closed connection\n", getName());
                        }
		} 	
	}
	return pEvent;
}

ProtocolEvent Protocol::receiveDataObject()
{
	size_t bytesRead = 0, totBytesRead = 0, totBytesPut = 0, bytesRemaining;
	Timeval t_start, t_end;
	Metadata *md = NULL;
	ProtocolEvent pEvent;
	DataObjectRef dObj;
        struct ctrlmsg m;

	HAGGLE_DBG("%s receiving data object\n", getName());

        dObj = new DataObject(localIface, peerIface, getKernel()->getStoragePath());

        if (!dObj) {
		HAGGLE_ERR("Could not create pending data object\n");
		return PROT_EVENT_ERROR;
	}

	t_start.setNow();
	bytesRemaining = DATAOBJECT_METADATA_PENDING;
	
	do {
		//HAGGLE_DBG("Reading data\n");
		pEvent = getData(&bytesRead);
		//HAGGLE_DBG("Read %lu bytes data\n", bytesRead);
                if (pEvent == PROT_EVENT_PEER_CLOSED) {
			HAGGLE_DBG("Peer %s closed connection\n", (peerIface ? peerIface->getIdentifierStr() : "Unknown"));
			return pEvent;
		}
		
		if (bufferDataLen > 0) {
			ssize_t bytesPut = 0;
			
			totBytesRead += bytesRead;

			bytesPut = dObj->putData(buffer, bufferDataLen, &bytesRemaining);

			if (bytesPut < 0) {
				HAGGLE_ERR("%s Error on put data! [BytesPut=%lu totBytesPut=%lu totBytesRead=%lu bytesRemaining=%lu]\n", getName(), bytesPut, totBytesPut, totBytesRead, bytesRemaining);
				return PROT_EVENT_ERROR;
			}

			removeData(bytesPut);
			totBytesPut += bytesPut;

			/*
			Did the data object just create it's metadata and still has
			data to receive?
			*/
			if (bytesRemaining != DATAOBJECT_METADATA_PENDING && md == NULL) {
				// Yep.
				md = dObj->getMetadata();
				// Send "Incoming data object event."
				if (md) {
                                        // Save the data object ID in the control message header.
                                        memcpy(m.dobj_id, dObj->getId(), DATAOBJECT_ID_LEN);

					// FIXME: check if we want to stop receiving data objects 
					// from this neighbor
					if (false) {
						// FIXME: disconnect from the neighbor and shut down
						// the protocol.
					}
					// Check if we already have this data object (FIXME: or are 
					// otherwise not willing to accept it).
					if (getKernel()->getThisNode()->getBloomfilter()->has(dObj)) {
						// Reject the data object:
                                                m.type = CTRLMSG_TYPE_REJECT;
                                                HAGGLE_DBG("Sending REJECT control message to peer %s\n", 
                                                           peerIface ? peerIface->getIdentifierStr() : "unknown");
						pEvent = sendControlMessage(&m);
                                                HAGGLE_DBG("%s receive DONE after rejecting data object\n", getName());
                                                return pEvent;
					} else {
                                                m.type = CTRLMSG_TYPE_ACCEPT;
						// Tell the other side to continue sending the data object:
                                                
                                                HAGGLE_DBG("Sending ACCEPT control message to peer %s\n", 
                                                           peerIface ? peerIface->getIdentifierStr() : "unknown");
                                                pEvent = sendControlMessage(&m);
					}
					
					getKernel()->addEvent(new Event(EVENT_TYPE_DATAOBJECT_INCOMING, dObj));

					HAGGLE_DBG("%s: Metadata header received [BytesPut=%lu totBytesPut=%lu totBytesRead=%lu bytesRemaining=%lu]\n", getName(), bytesPut, totBytesPut, totBytesRead, bytesRemaining);
				}
			}				
		}
		//HAGGLE_DBG("bytesRead=%lu bytesRemaining=%lu\n", bytesRead, bytesRemaining);
	} while (bytesRemaining && pEvent == PROT_EVENT_SUCCESS);

        if (pEvent != PROT_EVENT_SUCCESS)
                return pEvent;

	HAGGLE_DBG("totBytesPut=%lu totBytesRead=%lu bytesRemaining=%lu\n", 
				totBytesPut, totBytesRead, bytesRemaining);
	t_end.setNow();
	t_end -= t_start;

	dObj->setRxTime((long)t_end.getTimeAsMilliSeconds());

	HAGGLE_DBG("%ld bytes data received in %.3lf seconds, average speed %.2lf kB/s\n", 
		   totBytesRead, t_end.getTimeAsSecondsDouble(),
		   ((double) totBytesRead) / dObj->getRxTime());
	
	// Send ACK message back:	
	HAGGLE_DBG("Sending ACK control message to peer %s\n", peerIface ? peerIface->getIdentifierStr() : "unknown");
        m.type = CTRLMSG_TYPE_ACK;

	sendControlMessage(&m);

	dObj->setReceiveTime(Timeval::now());

	HAGGLE_DBG("Received data object [%s] from interface %s\n", 
		dObj->getIdStr(), peerIface ? peerIface->getIdentifierStr() : "undefined");

	getKernel()->addEvent(new Event(EVENT_TYPE_DATAOBJECT_RECEIVED, dObj));
       
	return pEvent;
}

ProtocolEvent Protocol::sendDataObjectNow(const DataObjectRef& dObj)
{
	int blockCount = 0;
	unsigned long totBytesSent = 0;
	ProtocolEvent pEvent = PROT_EVENT_SUCCESS;
	Timeval t_start = Timeval::now();
	Timeval waitTimeout;
	bool hasSentHeader = false;
	ssize_t len;
        struct ctrlmsg m;

	HAGGLE_DBG("%s : Sending data object to interface \'%s\'\n", 
			getName(), (peerIface ? peerIface->getIdentifierStr() : "n/a"));
	
	DataObjectDataRetrieverRef retriever = dObj->getDataObjectDataRetriever();

	if (!retriever || !retriever->isValid()) {
		HAGGLE_ERR("%s unable to start reading data\n", getName());
		return PROT_EVENT_ERROR;
	}
	// Repeat until the data object is completely sent:
	do {
		// Get the data:
		len = retriever->retrieve(buffer, bufferSize, !hasSentHeader);
		
		if (len < 0) {
			HAGGLE_ERR("Could not retrieve data from data object\n");
			pEvent = PROT_EVENT_ERROR;
		} else if (len > 0) {
			size_t totBytes = 0;
                        
                        do {
				size_t bytesSent = 0;
				waitTimeout = PROTOCOL_RECVSEND_TIMEOUT; // FIXME: Set suitable timeout

				pEvent = waitForEvent(&waitTimeout, true);
				
				if (pEvent == PROT_EVENT_TIMEOUT) {
					HAGGLE_DBG("Protocol timed out while waiting to write data\n");
					break;
								  
				} else if (pEvent != PROT_EVENT_WRITEABLE) {
					HAGGLE_ERR("Protocol was not writeable\n");
                                        break;
				}
				
				pEvent = sendData(buffer + totBytes, len - totBytes, 0, &bytesSent);

				if (pEvent == PROT_EVENT_ERROR) {
					switch (getProtocolError()) {
					case PROT_ERROR_BAD_HANDLE:
					case PROT_ERROR_NOT_CONNECTED:
					case PROT_ERROR_NOT_A_SOCKET:
					case PROT_ERROR_CONNECTION_RESET:
						pEvent = PROT_EVENT_ERROR_FATAL;
						break;
					case PROT_ERROR_WOULD_BLOCK:
						if (blockCount++ > PROT_BLOCK_TRY_MAX)
                                                        break;

						// Set event to success so that the loop does not quit

						HAGGLE_DBG("Sending would block, try number %d/%d in %.3lf seconds\n",
							blockCount, PROT_BLOCK_TRY_MAX, 
							(double)PROT_BLOCK_SLEEP_TIME_MSECS / 1000);

						cancelableSleep(PROT_BLOCK_SLEEP_TIME_MSECS);

						pEvent = PROT_EVENT_SUCCESS;
						break;
					default:
						HAGGLE_ERR("Protocol error : %s\n", getProtocolErrorStr());
						break;
					}
				} else {
					// Reset the block count since we successfully sent some data
					blockCount = 0;						
					totBytes += bytesSent;
					//HAGGLE_DBG("Sent %lu bytes data on channel\n", bytesSent);
				}
			} while ((len - totBytes) && pEvent == PROT_EVENT_SUCCESS);

			totBytesSent += totBytes;
		}
		
		// If we've just finished sending the header:
		if (len == 0 && !hasSentHeader && pEvent == PROT_EVENT_SUCCESS) {
			// We are sending to a local application: done after sending the 
			// header:
			if (peerIface->getType() == IFTYPE_APPLICATION_PORT)
                                return pEvent;
			
			hasSentHeader = true;
			
                        HAGGLE_DBG("Getting accept/reject control message\n");
			// Get the accept/reject "message":
			pEvent = receiveControlMessage(&m);

			// Did we get it?                        
			if (pEvent == PROT_EVENT_SUCCESS) {
                                HAGGLE_DBG("Received control message '%s'\n", ctrlmsgToStr(&m).c_str());
				// Yes, check it:
				if (m.type == CTRLMSG_TYPE_ACCEPT) {
					// ACCEPT message. Keep on going.
                                        // This is to make the while loop start over, in case there was
                                        // more of the data object to send.
                                        len = 1;
                                        HAGGLE_DBG("%s Got ACCEPT control message, continue sending\n", getName());
				} else if (m.type == CTRLMSG_TYPE_REJECT) {
					// Reject message. Stop sending this data object:
                                        HAGGLE_DBG("%s Got REJECT control message, stop sending\n", getName());
                                        return PROT_EVENT_REJECT;
				} else if (m.type == CTRLMSG_TYPE_TERMINATE) {
					// Terminate message. Stop sending this data object, and all queued ones:
                                        HAGGLE_DBG("%s Got TERMINATE control message, purging queue\n", getName());
                                        return PROT_EVENT_TERMINATE;
				}
			} else {
                                HAGGLE_ERR("Did not receive accept/reject control message\n");
                        }
		}
	} while (len > 0 && pEvent == PROT_EVENT_SUCCESS);
	
	if (pEvent != PROT_EVENT_SUCCESS) {
		HAGGLE_ERR("%s : Send - %s\n", 
			   getName(), pEvent == PROT_EVENT_PEER_CLOSED ? "Peer closed" : "Error");
                return pEvent;
	}
#ifdef DEBUG
        Timeval tx_time = Timeval::now() - t_start;

        HAGGLE_DBG("%s Sent %lu bytes data in %.3lf seconds, average speed = %.2lf kB/s\n", 
                   getName(), totBytesSent, tx_time.getTimeAsSecondsDouble(), 
                   (double)totBytesSent / (1000*tx_time.getTimeAsSecondsDouble()));
#endif
        
        HAGGLE_DBG("Waiting %d seconds for ACK from %s\n", 
                   PROTOCOL_RECVSEND_TIMEOUT, peerIface ? peerIface->getIdentifierStr() : "Unknown");
        
        pEvent = receiveControlMessage(&m);

        if (pEvent == PROT_EVENT_SUCCESS) {
                if (m.type == CTRLMSG_TYPE_ACK) {
                        HAGGLE_DBG("Received '%s'\n", ctrlmsgToStr(&m).c_str());
                } else {
                        HAGGLE_ERR("Control message malformed: expected 'ACK', got '%s'\n", ctrlmsgToStr(&m).c_str());
                        pEvent = PROT_EVENT_ERROR;
                }
        } else {
                HAGGLE_ERR("ERROR: Did not receive ACK control message... Assuming data object was not received.\n");
        }
	return pEvent;
}

bool Protocol::run()
{
	ProtocolEvent pEvent;
	int numConnectTry = 0;
	setMode(PROT_MODE_IDLE);
	int numerr = 0;
	Queue *q = getQueue();
	NodeRef peerNode;

	if (!q) {
		HAGGLE_ERR("Could not get a Queue for protocol %s\n", getName());
		setMode(PROT_MODE_DONE);
		return false;
	}
	
	// Cache the peer node here. If the node goes away, it may be taken out of the
	// node store before the protocol quits, and then we cannot retrieve it for the
	// send result event.
	peerNode = getManager()->getKernel()->getNodeStore()->retrieve(peerIface);

	while (!isDone() && !shouldExit()) {

		while (!isConnected()) {
                        
			if (connectToPeer() == PROT_EVENT_SUCCESS) {
				// The connected flag should probably be set in connectToPeer,
				// but set it here for safety
				HAGGLE_DBG("%s successfully connected\n", getName());

				if (!peerNode) {
					// Just in case we didn't have the node in the store before.
					peerNode = getManager()->getKernel()->getNodeStore()->retrieve(peerIface);
				}
			} else {
				numConnectTry++;
				HAGGLE_DBG("%s connection failure %d/%d\n", 
					   getName(), numConnectTry, 
					   PROT_CONNECTION_ATTEMPTS);

				if (numConnectTry == PROT_CONNECTION_ATTEMPTS) {
					HAGGLE_DBG("%s failed to connect...\n", getName());
					q->close();
					setMode(PROT_MODE_DONE);
				} else {
					unsigned int sleep_secs = RANDOM_INT(20) + 5;

					HAGGLE_DBG("%s sleeping %u secs\n", getName(), sleep_secs);

					cancelableSleep(sleep_secs * 1000);

				}
			}
                        // Check to make sure we were not cancelled
                        // before we start doing work
                        if (isDone() || shouldExit())
                                goto done;
		}

		Timeval t_start = Timeval::now();
		Timeval timeout(PROT_WAIT_TIME_BEFORE_DONE);
		DataObjectRef dObj;

		pEvent = waitForEvent(dObj, &timeout);
		
		timeout = Timeval::now() - t_start;

		switch (pEvent) {
			case PROT_EVENT_TIMEOUT:
				// Timeout expired:
				setMode(PROT_MODE_DONE);
			break;
			case PROT_EVENT_TXQ_NEW_DATAOBJECT:
				// Data object to send:
				if (!dObj) {
					// Something is wrong here. TODO: better error handling than continue?
					HAGGLE_ERR("%s No data object in queue despite indicated. Something is WRONG!\n", 
						   getName(), (peerNode ? peerNode->getName().c_str() : "n/a"));
					
					break;
				}
				HAGGLE_DBG("%s Data object retrieved from queue, sending to [%s]\n", 
					   getName(), (peerNode ? peerNode->getName().c_str() : "n/a"));
				
				pEvent = sendDataObjectNow(dObj);
				
				if (pEvent == PROT_EVENT_SUCCESS || pEvent == PROT_EVENT_REJECT) {
					// Treat reject as SUCCESS, since it probably means the peer already has the
					// data object and we should therefore not try to send it again.
					getKernel()->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL, dObj, peerNode, (pEvent == PROT_EVENT_REJECT)?1:0));
				} else {
					// Send success/fail event with this data object
					switch (pEvent) {
						case PROT_EVENT_TERMINATE:
							// TODO: What to do here?
							// We should stop sending completely, but if we just
							// close the connection we might just connect and start
							// sending again. We need a way to signal that we should 
							// not try to send to this peer again -- at least not
							// until next time he is our neighbor. For now, treat
							// the same way as if the peer closed the connection.
						case PROT_EVENT_PEER_CLOSED:
							HAGGLE_DBG("%s Peer [%s] closed its end of the connection...\n", 
								getName(), (peerNode ? peerNode->getName().c_str() : "n/a"));
							q->close();
							setMode(PROT_MODE_DONE);
							closeConnection();
							break;
						case PROT_EVENT_ERROR:
							HAGGLE_ERR("%s Data object send failed...\n", getName());
							break;
						case PROT_EVENT_ERROR_FATAL:
							HAGGLE_ERR("%s Data object send fatal error!\n", getName());
							q->close();
							setMode(PROT_MODE_DONE);
							break;
						default:
							q->close();
							setMode(PROT_MODE_DONE);
					}
					getKernel()->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND_FAILURE, dObj, peerNode));
				}
				break;
			case PROT_EVENT_INCOMING_DATA:
				// Data object to receive:
				HAGGLE_DBG("%s Incoming data object from [%s]\n", getName(), (peerNode ? peerNode->getName().c_str() : "n/a"));
				
				pEvent = receiveDataObject();	

				switch (pEvent) {
					case PROT_EVENT_SUCCESS:
						HAGGLE_DBG("%s Data object successfully received from [%s]\n", 
							getName(), (peerNode ? peerNode->getName().c_str() : "n/a"));
						break;
					case PROT_EVENT_PEER_CLOSED:
						HAGGLE_DBG("%s Peer [%s] closed its end of the connection...\n", 
							getName(), (peerNode ? peerNode->getName().c_str() : "n/a"));
						q->close();
						setMode(PROT_MODE_DONE);
						closeConnection();
						break;
					case PROT_EVENT_ERROR:
						HAGGLE_ERR("%s Data object receive failed... error num %d\n", getName(), numerr);
						if (numerr++ > 3 || shouldExit()) {
							q->close();
							closeConnection();
							setMode(PROT_MODE_DONE);
							HAGGLE_DBG("%s Reached max errors=%d - Cancelling protocol!\n", 
								getName(), numerr);
						}
						continue;
					case PROT_EVENT_ERROR_FATAL:
						HAGGLE_ERR("%s Data object receive fatal error!\n", getName());
						q->close();
						setMode(PROT_MODE_DONE);
						break;
					default:
						q->close();
						setMode(PROT_MODE_DONE);
				}
				break;
			case PROT_EVENT_TXQ_EMPTY:
				HAGGLE_ERR("%s - Queue was empty\n", getName());
				break;
			case PROT_EVENT_ERROR:
				HAGGLE_ERR("Error num %d in protocol %s\n", numerr, getName());

				if (numerr++ > 3 || shouldExit()) {
					q->close();
					closeConnection();
					setMode(PROT_MODE_DONE);
					HAGGLE_DBG("%s Reached max errors=%d - Cancelling protocol!\n", 
						getName(), numerr);
				}
				continue;
                        case PROT_EVENT_SHOULD_EXIT:
                                setMode(PROT_MODE_DONE);
                                break;
			default:
				HAGGLE_ERR("%s: Unknown protocol event!\n", getName());
				break;
		}
                // Reset error
		numerr = 0;
	}
      done:
	HAGGLE_DBG("%s client DONE!\n", getName());
	if (isConnected())
		closeConnection();

        setMode(PROT_MODE_DONE);
	
	return false;
}

void Protocol::registerWithManager()
{
	if (isRegistered) {
		HAGGLE_ERR("Trying to register %s more than once\n", getName());
		return;
	}
	
	isRegistered = true;
	getKernel()->addEvent(new Event(getManager()->add_protocol_event, this));
}


void Protocol::unregisterWithManager()
{
	if (!isRegistered) {
		HAGGLE_ERR("Trying to unregister protocol %s twice\n", getName());
		return;
	}
	isRegistered = false;

	HAGGLE_DBG("Protocol %s sends unregister event\n", getName()); 

	getKernel()->addEvent(new Event(getManager()->delete_protocol_event, this));
}

void Protocol::shutdown()
{
	HAGGLE_DBG("Shutting down protocol: %s\n", getName());

	hookShutdown();
	
	if (isRunning())
		cancel();
	else if (isRegistered)
		unregisterWithManager();
}

void Protocol::cleanup()
{
	HAGGLE_DBG("Cleaning up protocol: %s\n", getName());
	
	hookCleanup();
	
	closeConnection();
	HAGGLE_DBG("%s protocol is now garbage!\n", getName());
	setMode(PROT_MODE_GARBAGE);
	unregisterWithManager();
}
