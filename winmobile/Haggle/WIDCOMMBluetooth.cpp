#include "WIDCOMMBluetooth.h"

#if defined(WIDCOMM_BLUETOOTH)

#include <msgqueue.h>
#include <libcpphaggle/Exception.h>

WIDCOMMBluetooth *WIDCOMMBluetooth::stack = NULL;
WIDCOMMBluetooth::StackInitializer WIDCOMMBluetooth::stackInit;

WIDCOMMBluetooth::StackInitializer::StackInitializer()
{
	WIDCOMMBluetooth::init();
}
WIDCOMMBluetooth::StackInitializer::~StackInitializer()
{
	WIDCOMMBluetooth::cleanup();
}

WIDCOMMBluetooth::WIDCOMMBluetooth() : CBtIf(), mutex("WIDCOMM"), hMsgQ(NULL), hStackEvent(NULL), 
	hInquiryEvent(NULL), hDiscoveryEvent(NULL), inquiryCallback(NULL), discoveryCallback(NULL),
	inquiryData(NULL), discoveryData(NULL), isInInquiry(false), isInDiscovery(false)
{
	hStackEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	hInquiryEvent = CreateEvent(NULL, FALSE, FALSE, NULL);	
	hDiscoveryEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

WIDCOMMBluetooth::~WIDCOMMBluetooth()
{
	CloseHandle(hStackEvent);
	CloseHandle(hInquiryEvent);
	CloseHandle(hDiscoveryEvent);
	WIDCOMMSDK_ShutDown();
}

int WIDCOMMBluetooth::init()
{
	if (stack)
		return -1;

	stack = new WIDCOMMBluetooth();

	if (!stack)
		return -1;

	printf("Stack was initialized\n");

	return 0;
}

void WIDCOMMBluetooth::cleanup()
{
	if (stack) {
		delete stack;
		stack = NULL;
	}
}

HANDLE WIDCOMMBluetooth::setMsgQ(HANDLE h)
{
	if (!hMsgQ) {
		MSGQUEUEOPTIONS mqOpts = { sizeof(MSGQUEUEOPTIONS), MSGQUEUE_NOPRECOMMIT, 0, 
			sizeof(BTEVENT), FALSE };
		hMsgQ = OpenMsgQueue(GetCurrentProcess(), h, &mqOpts);
	}

	return hMsgQ;
}

bool WIDCOMMBluetooth::resetMsgQ(HANDLE h)
{
	if (hMsgQ && h == hMsgQ) {
		CloseMsgQueue(hMsgQ);
		hMsgQ = NULL;
		return true;
	}
	return false;
}

void WIDCOMMBluetooth::OnStackStatusChange(CBtIf::STACK_STATUS new_status)
{
	BTEVENT bte;

	memset(&bte, 0, sizeof(BTEVENT));

	printf("Stack status change event : status=%lu\n", new_status);

	switch (new_status) {
		case DEVST_DOWN:
			bte.dwEventId = BTE_STACK_DOWN;
			break;
		case DEVST_RELOADED:
			bte.dwEventId = BTE_STACK_UP;
			break;
		case DEVST_UNLOADED:
			bte.dwEventId = BTE_STACK_DOWN;
			break;
	}

	if (hMsgQ)
		WriteMsgQueue(hMsgQ, &bte, sizeof(BTEVENT), INFINITE, 0);

	/*
	fprintf(stderr, "Cleaning up Bluetooth stack\n");

	cleanup();

	fprintf(stderr, "Waiting a couple of secs before reinitializing Bluetooth stack\n");
	Sleep(2000);

	if (init() < 0) {
		fprintf(stderr, "Could not initialize Bluetooth stack again\n");
	}
	*/
}

void WIDCOMMBluetooth::OnDeviceResponded(BD_ADDR bda, DEV_CLASS devClass, BD_NAME bdName, BOOL bConnected)
{
	struct RemoteDevice rd;
	memcpy(rd.bda, bda, sizeof(BD_ADDR));
	memcpy(rd.devClass, devClass, sizeof(DEV_CLASS));
	rd.name = (char *)bdName;
	rd.bConnected = (bConnected == TRUE) ? true : false;

	mutex.lock();
	
	/*
		Check first if the device is already in the cache. For some reason, the stack 
		sometimes calls	this handler function several times for the same device.

	*/
	for (List<struct RemoteDevice>::iterator it = inquiryCache.begin(); it != inquiryCache.end(); ++it) {
		if (memcmp((*it).bda, bda, sizeof(BD_ADDR)) == 0) {
			rd = *it;
			mutex.unlock();
			return;
		}
	}
	inquiryCache.push_back(rd);
	inquiryResult++;

	mutex.unlock();

	if (inquiryCallback)
		inquiryCallback(&rd, inquiryData);
}

void WIDCOMMBluetooth::OnInquiryComplete(BOOL success, short num_responses)
{
	isInInquiry = false;

	if (success == FALSE)
		inquiryResult = -1;

	SetEvent(hInquiryEvent);
}

int WIDCOMMBluetooth::_doInquiry(widcomm_inquiry_callback_t callback, void *data, bool async)
{
	if (isInInquiry || !IsDeviceReady())
		return -1;

	isInInquiry = true;
	inquiryCallback = callback;
	inquiryData = data;
	inquiryResult = 0;

	// Make sure the event is not set
	ResetEvent(hInquiryEvent);

	mutex.lock();
	inquiryCache.clear();
	mutex.unlock();

	if (StartInquiry() == FALSE) {
		isInInquiry = false;
		return -1;
	}

	// Once we successfully started the inquiry, the isInInquiry boolean
	// will be reset to 'false' by the OnInquiryComplete callback
	if (!async) {
		if (WaitForSingleObject(hInquiryEvent, INFINITE) == WAIT_FAILED) {
			return -1;
		}
		return inquiryResult;
	}
	return 0;
}

int WIDCOMMBluetooth::doInquiry(widcomm_inquiry_callback_t callback, void *data)
{
	return stack->_doInquiry(callback, data, false);
}

int WIDCOMMBluetooth::doInquiryAsync(widcomm_inquiry_callback_t callback, void *data)
{
	return stack->_doInquiry(callback, data, true);
}

void WIDCOMMBluetooth::stopInquiry()
{
	stack->StopInquiry();
	stack->isInInquiry = false;
}

void WIDCOMMBluetooth::OnDiscoveryComplete()
{
	struct RemoteDevice rd;
	bool recordFound = false;
	BD_ADDR bdaddr;
	UINT16 num_records;
	CSdpDiscoveryRec *records;

	if (GetLastDiscoveryResult(bdaddr, &num_records) != DISCOVERY_RESULT_SUCCESS)
		goto out;

	if (num_records == 0)
		goto out;

	mutex.lock();

	for (List<struct RemoteDevice>::iterator it = inquiryCache.begin(); it != inquiryCache.end(); ++it) {
		if (memcmp((*it).bda, bdaddr, sizeof(BD_ADDR)) == 0) {
			rd = *it;
			recordFound = true;
		}
	}

	mutex.unlock();

	if (!recordFound)
		goto out;

	records = new CSdpDiscoveryRec[num_records];

	if (!records)
		goto out;

	int ret = ReadDiscoveryRecords(rd.bda, (int)num_records, records);

	if (ret && discoveryCallback)
		discoveryCallback(&rd, records, ret, discoveryData);

	delete [] records;

out:
	discoveryResult = (int)num_records;
	isInDiscovery = false;
	SetEvent(hDiscoveryEvent);
}

int WIDCOMMBluetooth::_doDiscovery(const RemoteDevice *rd, GUID *guid, widcomm_discovery_callback_t callback, void *data, bool async)
{
	BD_ADDR bdaddr;

	if (!rd || isInDiscovery || !IsDeviceReady())
		return -1;

	isInDiscovery = true;
	discoveryCallback = callback;
	discoveryData = data;
	discoveryResult = 0;

	// Make sure the event is not set
	ResetEvent(hDiscoveryEvent);

	memcpy(bdaddr, rd->bda, sizeof(BD_ADDR));

	if (StartDiscovery(bdaddr, guid) == FALSE) {
		isInDiscovery = false;
		return -1;
	}

	// Once we successfully started the inquiry, the isInDiscovery boolean
	// will be reset to 'false' by the OnDiscoveryComplete callback
	if (!async) {
		if (WaitForSingleObject(hDiscoveryEvent, INFINITE) == WAIT_FAILED) {
			return -1;
		}
		return discoveryResult;
	}

	return 0;
}

int WIDCOMMBluetooth::doDiscoveryAsync(const RemoteDevice *rd, GUID *guid, widcomm_discovery_callback_t callback, void *data)
{
	return stack->_doDiscovery(rd, guid, callback, data, true);
}

int WIDCOMMBluetooth::doDiscovery(const RemoteDevice *rd, GUID *guid, widcomm_discovery_callback_t callback, void *data)
{
	return stack->_doDiscovery(rd, guid, callback, data, false);
}

int WIDCOMMBluetooth::readLocalDeviceAddress(char *mac)
{
	DEV_VER_INFO dvi;

	if (stack->GetLocalDeviceVersionInfo(&dvi) == FALSE)
		return -1;

	memcpy(mac, dvi.bd_addr, sizeof(BD_ADDR));

	return 0;
}

int WIDCOMMBluetooth::readLocalDeviceName(char *name, int len)
{
	BD_NAME bdName;

	if (stack->GetLocalDeviceName(&bdName) == FALSE)
		return -1;

	strncpy(name, (char *)bdName, len);

	return strlen(name);
}

bool WIDCOMMBluetooth::_enumerateRemoteDevicesStart()
{
	if (isInInquiry)
		return false;

	inquiryCacheIterator = inquiryCache.begin();
	return true;
}
const RemoteDevice *WIDCOMMBluetooth::_getNextRemoteDevice()
{
	if (isInInquiry)
		return NULL;

	if (inquiryCacheIterator == inquiryCache.end())
		return NULL;

	RemoteDevice *rd = &(*inquiryCacheIterator);

	inquiryCacheIterator++;

	return rd;
}

bool WIDCOMMBluetooth::enumerateRemoteDevicesStart()
{
	return stack->_enumerateRemoteDevicesStart();
}

const RemoteDevice *WIDCOMMBluetooth::getNextRemoteDevice()
{
	return stack->_getNextRemoteDevice();
}

HANDLE RequestBluetoothNotifications(DWORD flags, HANDLE hMsgQ)
{ 
	return WIDCOMMBluetooth::stack->setMsgQ(hMsgQ);
}

BOOL StopBluetoothNotifications(HANDLE h)
{
	return WIDCOMMBluetooth::stack->resetMsgQ(h);
}

int BthSetMode(DWORD dwMode)
{
	return -1;
}

#endif /* WIDCOMM_BLUETOOTH */