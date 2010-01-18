/* Copyright 2008 Uppsala University
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


/*
LuckyMe challenges your luck. 
How many random dataobjects floating around in the Haggle network
do you attract? You are yourself contributing to the luck of other
players in the network! 

Besides of being a research application to evalute the search based
networking paradigm of Haggle, this code can be taken as a skeletton 
for many fun application or game scenarios.
Think creative, why not use puzzle parts as random objects? Make a 
competition with you buddies to collect as many objects as possible. 
*/

#include "luckyMe.h"
#include <libhaggle/haggle.h>
#include "haggleutils.h"
#if defined(OS_UNIX)

#include <signal.h>
#include <unistd.h>
#include <errno.h>

#define ERRNO errno

#elif defined(OS_WINDOWS)

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>
#include <winioctl.h>
#include <winerror.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#define ERRNO WSAGetLastError()
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif

#include <string.h>
#include <math.h>

#include <stdlib.h>
#include <stdio.h>

// --- needed in luckyGUI
#if defined(OS_WINDOWS_MOBILE)
unsigned int numberOfDOsReceived;
unsigned int numberOfDOsCreated;
unsigned int numberOfNeighbors;
static char **neighborName;

static bool notify_flag = true;

static updateCallbackFunction_t eventCallback = NULL;

char debugString[4096] = {0};

#define NOTIFY() { if (eventCallback) { eventCallback(EVENT_TYPE_STATUS_UPDATE); } }

static HANDLE luckyme_thread_handle = NULL;
static HANDLE haggle_start_thread_handle = NULL;
static HANDLE test_loop_event = NULL;
static bool test_is_running = false;
bool calledHaggleShutdown = false;

#else
#define NOTIFY()
#endif

// --- command line arguments
unsigned int gridSize = 0;
char *fileName = "\\luckyMeData.jpg";
char *singleSourceName = 0;
unsigned int createDataInterval = 120;
unsigned int attrPoolSize = 100;
unsigned int numDataObjectAttributes = 3;
unsigned int varianceInterestAttributes = 2;

// --- 
#define NUM_CONNECTIONS 1
#define HTTP_PORT 8082

#define APP_NAME "LuckyMe"

haggle_handle_t hh = NULL;
char hostname[128];
int nodeNumber;


#if defined(OS_MACOSX)
#include <stdlib.h>
#endif

#if defined(OS_WINDOWS)
static DWORD WINAPI luckyme_thread(void *);
#else
static int luckyme_thread();
#endif

double fac(int n)
{
	double t=1;
	for (int i=n;i>1;i--)
		t*=i;
	return t;
}


// --- luckyMe functions (create interest and data) 
/* 
we are interested in a number of random attributes.
*/
void createInterestBinomial() 
{
	unsigned int i = 0;
	char luckAttrValue[32];
	struct attributelist *attrList = haggle_attributelist_new();
	struct attribute *attr = 0;

	unsigned long luck;
	unsigned int weight = 0;

	// use binomial distribution to approximate normal distribution (sum of weights = 100)
	// mean = np = luck, variance = np(1-p) = LUCK_INTERESTS
	double p = 0.5;
	unsigned int n = (unsigned int) (4 * sqrt((double)varianceInterestAttributes));
	unsigned int u = (unsigned int) (n * p);

	luck = prng_uint32() % attrPoolSize;
	sprintf(debugString,
		"createInterestBinomial (luck=%ld)\n", luck);
	// printf("binomial distribution  n=%u, p=%f\n", n, p);

	unsigned int interest = 0;
	for (i = 0; i <= n; i++) {
		interest = (luck + i - u + attrPoolSize) % attrPoolSize;
		weight = (unsigned int) (100 * fac(n)/(fac(n-i)*fac(i))*pow(p,i)*pow(1-p,n-i));
		if (weight > 0) {
			sprintf(luckAttrValue, "%u", interest);
			attr = haggle_attribute_new_weighted(APP_NAME, luckAttrValue, weight);
			haggle_attributelist_add_attribute(attrList, attr);
			LIBHAGGLE_DBG("   %s=%s [%d]\n", APP_NAME, luckAttrValue, weight);
		}
	}

	haggle_ipc_add_application_interests(hh, attrList);
	
	haggle_attributelist_free(attrList);
}


/* 
we are interested in two attributes,
corresponding to row and column(+gridSize) in the grid. 
*/
void createInterestGrid() 
{
	unsigned int i = 0;
	char luckAttrValue[32];
	struct attributelist *attrList = haggle_attributelist_new();
	struct attribute *attr = 0;
	static const unsigned int numInterests = 2;
	unsigned int interests[numInterests];

	interests[0] = nodeNumber / gridSize;
	interests[1] = (nodeNumber % gridSize) + gridSize;

	sprintf(debugString, "createInterestGrid %d %d\n", interests[0], interests[1]);

	for (i = 0; i < numInterests; i++) {
		sprintf(luckAttrValue, "%d", interests[i]);
		attr = haggle_attribute_new_weighted(APP_NAME, luckAttrValue, 1);
		haggle_attributelist_add_attribute(attrList, attr);
		LIBHAGGLE_DBG("   %s=%s [%d]\n", APP_NAME, luckAttrValue, 1);
	}

	haggle_ipc_add_application_interests(hh, attrList);
	
	haggle_attributelist_free(attrList);
}


/*
we create a dataobject with a number of random attributes.
at the moment the attributes are uniformly distributed.
the intention is to build in some correlation to the own
interests and/or the dataobjects that one received.
*/
void createDataobjectRandom() 
{
	static unsigned int cntDataObjectNumber = 1;

	unsigned int i = 0;
	char luckAttrValue[32];

	struct dataobject *dObj = 0;

	if (fileName) {
		dObj = haggle_dataobject_new_from_file(fileName);

		if (!dObj)
			dObj = haggle_dataobject_new();
	} else {
		dObj = haggle_dataobject_new();
	}

	if (!dObj)
		return;

	printf("createDataobject %u\n", cntDataObjectNumber);
	// todo: get unique attributes
	for (i = 0; i < numDataObjectAttributes; i++) {
		sprintf(luckAttrValue, "%ld", prng_uint32() % attrPoolSize);
		haggle_dataobject_add_attribute(dObj, APP_NAME, luckAttrValue);
		LIBHAGGLE_DBG("   %s=%s\n", APP_NAME, luckAttrValue);
	}
	{
	timeval tv;
	gettimeofday(&tv, NULL);
	haggle_dataobject_set_createtime(dObj, &tv);
	}

	haggle_dataobject_set_createtime(dObj, NULL);
	haggle_ipc_publish_dataobject(hh, dObj);
	haggle_dataobject_free(dObj);

	cntDataObjectNumber++;
}


/*
we create a dataobject with a number of attributes.
all combinations from 1 attribute to 2*gridSize attributes generated.
*/
void createDataobjectGrid() 
{
	static unsigned int cntDataObjectNumber = 1;
	unsigned int maxDataObjectNumber = (1 << (2*gridSize));

	int i = 0;
	char luckAttrValue[128];

	if (cntDataObjectNumber > maxDataObjectNumber-1)
		stopTest();

	struct dataobject *dObj = 0;

	if (fileName) {
		dObj = haggle_dataobject_new_from_file(fileName);

		if (!dObj)
			dObj = haggle_dataobject_new();
	} else {
		dObj = haggle_dataobject_new();
	}
	if (!dObj)
		return;

	LIBHAGGLE_DBG("createDataobject %u\n", cntDataObjectNumber);

	for (i = 0; i < 6; i++) {
		if ((cntDataObjectNumber>>i) & 0x01) {
			sprintf(luckAttrValue, "%d", i);
			haggle_dataobject_add_attribute(dObj, APP_NAME, luckAttrValue);
			LIBHAGGLE_DBG("   %s=%s\n", APP_NAME, luckAttrValue);
		}
	}

	haggle_dataobject_set_createtime(dObj, NULL);
	haggle_ipc_publish_dataobject(hh, dObj);
	haggle_dataobject_free(dObj);
#if defined(OS_WINDOWS_MOBILE)
	numberOfDOsCreated++;
#endif

	cntDataObjectNumber++;
}


/* 
we get a matching dataobject.
maybe we should reward the creator or delegate?
*/
int onDataobject(haggle_event_t *e, void* nix)
{
#if defined(OS_WINDOWS_MOBILE)
	numberOfDOsReceived++;
#endif
	if (eventCallback)
		eventCallback(EVENT_TYPE_NEW_DATAOBJECT);

	return 0;
}


int onNeighborUpdate(haggle_event_t *e, void* nix)
{
#if defined(OS_WINDOWS_MOBILE)
	haggle_nodelist_t *nl = e->neighbors;
	unsigned int new_number_of_neighbors, i;
	char **newNeighborName;

	new_number_of_neighbors = haggle_nodelist_size(nl);

	if (new_number_of_neighbors > 0) {
		newNeighborName = (char **) malloc(sizeof(char *)*new_number_of_neighbors);
		if (newNeighborName == NULL) {
			new_number_of_neighbors = 0;
		} else {
			for (i = 0; i < new_number_of_neighbors; i++) {
				haggle_node_t *n;
				const char *nodestr;
				n = haggle_nodelist_get_node_n(nl, i);
				nodestr = haggle_node_get_name(n);
				newNeighborName[i] = (char *)
					malloc(sizeof(char) * (strlen(nodestr)+1));
				if (newNeighborName[i] != NULL) {
					strcpy(newNeighborName[i], nodestr);
				}
			}
		}
	} else {
		newNeighborName = NULL;
	}

	char **oldNeighborName;
	unsigned int oldNeighborCount;
	oldNeighborCount = numberOfNeighbors;
	oldNeighborName = neighborName;

	if (numberOfNeighbors > new_number_of_neighbors) {
		numberOfNeighbors = new_number_of_neighbors;
		neighborName = newNeighborName;
	} else {
		neighborName = newNeighborName;
		numberOfNeighbors = new_number_of_neighbors;
	}

	for (i = 0; i < oldNeighborCount; i++)
		free(oldNeighborName[i]);

	free(oldNeighborName);
#endif
	if (eventCallback)
		eventCallback(EVENT_TYPE_NEIGHBOR_UPDATE);

	return 0;
}

static bool haggleIsShuttingDown = false;

int setCallback(updateCallbackFunction_t callback)
{
	if (eventCallback != NULL)
		return 0;

	eventCallback = callback;

	return 1;
}

int onShutdown(haggle_event_t *e, void* nix)
{
	haggleIsShuttingDown = true;

	LIBHAGGLE_DBG("Got shutdown event\n");

	if (hh && calledHaggleShutdown) {
		if (haggle_event_loop_is_running(hh)) {
			LIBHAGGLE_DBG("Stopping event loop\n");
			haggle_event_loop_stop(hh);
		}
	}

	if (eventCallback)
		eventCallback(EVENT_TYPE_SHUTDOWN);

	return 0;
}

int isHaggleShuttingDown()
{
	return haggleIsShuttingDown ? 1 : 0;
} 
// ----- USAGE (parse arguments)

#if defined(OS_UNIX)

static void PrintUsage()
{	
	fprintf(stderr, 
		"Usage: ./%s [-A num] [-d num] [-i num] [-t interval] [-g gridSize] [-s hostname] [-f path]\n", 
		APP_NAME);
	fprintf(stderr, "          -A attribute pool (default %u)\n", attrPoolSize);
	fprintf(stderr, "          -d number of attributes per data object (default %u)\n", numDataObjectAttributes);
	fprintf(stderr, "          -i interest variance (default %u)\n", varianceInterestAttributes);
	fprintf(stderr, "          -g use grid topology (gridSize x gridSize, overwrites -Adi, default off)\n");
	fprintf(stderr, "          -t interval to create data objects [s] (default %u)\n", createDataInterval);
	fprintf(stderr, "          -s singe source (create data objects only on node 'name', default off)\n");
	fprintf(stderr, "          -f data file to be sent (default off)\n");
}

// Parses our command line arguments into the global variables 
// listed above.
static void ParseArguments(int argc, char **argv)
{
	int ch;

	// Parse command line options using getopt.

	do {
		ch = getopt(argc, argv, "A:d:i:t:g:s:f:");
		if (ch != -1) {
			switch (ch) {
		case 'A':
			attrPoolSize = atoi(optarg);
			break;
		case 'd':
			numDataObjectAttributes = atoi(optarg);
			break;
		case 'i':
			varianceInterestAttributes = atoi(optarg);
			break;
		case 't':
			createDataInterval = atoi(optarg);
			break;
		case 'g':
			gridSize = atoi(optarg);
			attrPoolSize = 2*gridSize;
			break;
		case 's':
			singleSourceName = optarg;
			break;
		case 'f':
			fileName = optarg;
			break;
		default:
			PrintUsage();
			exit(1);
			break;
			}
		}
	} while (ch != -1);

	// Check for any left over command line arguments.

	if (optind != argc) {
		PrintUsage();
		fprintf(stderr, "%s: Unexpected argument '%s'\n", argv[0], argv[optind]);
		exit(1);
	}

}

#endif // OS_UNIX

static bool StopNow;

void testLoop() {
	int result = 0;

#if !defined(OS_WINDOWS)
	fd_set readfds;
	struct timeval timeout;

	// Sleep N minutes before the test starts. 
	unsigned long i;
	DWORD start_delay = 10 * 60;

	for (i = 0; i < start_delay && !StopNow; i++)
		Sleep(1000);

	if (StopNow)
		return;
#endif

	while (!StopNow) {
#if defined(OS_WINDOWS)
		DWORD timeout = test_is_running ? createDataInterval * 1000 : INFINITE;
		
		DWORD ret = WaitForSingleObject(test_loop_event, timeout);

		ResetEvent(test_loop_event);

		switch (ret) {
			case WAIT_OBJECT_0:
				// Set result 1, i.e., create no data object.
				// We just check whether the test is running or not, or
				// if we should exit
				result = 1;
				break;
			case WAIT_FAILED:
				StopNow = true;
				result = -1;
				sprintf(debugString, "Stopped test due to wait failure\n");
				break;
			case WAIT_TIMEOUT:
				result = 0;
				break;
		}
#else
		unsigned int nfds = 0;
		timeout.tv_sec = createDataInterval;
		timeout.tv_usec = 0;

		FD_ZERO(&readfds);

		result = select(nfds, &readfds, NULL, NULL, &timeout);
#endif
		if (result < 0) {
			if (ERRNO != EINTR) {
				LIBHAGGLE_ERR("Stopping due to error!\n");
				StopNow = 1;
			}
		} else if (result == 0) {
			if (singleSourceName == NULL || strcmp(hostname, singleSourceName) == 0) {
				LIBHAGGLE_DBG("Creating data object\n");
				if (gridSize > 0) {
					createDataobjectGrid();
				} else {
					createDataobjectRandom();
				}
				eventCallback(EVENT_TYPE_DATA_OBJECT_GENERATED);
			}
		}
	}
}

int onInterests(haggle_event_t *e, void* nix)
{
	prng_init();

	if (haggle_attributelist_get_attribute_by_name(e->interests, APP_NAME) != NULL) {
		list_t *pos;
		// We already have some interests, so we don't create any new ones.
		
		// In the future, we might want to delete the old interests, and
		// create new ones... depending on the circumstances.
		// If so, that code would fit here. 
		
		list_for_each(pos, &e->interests->attributes) {
			struct attribute *attr = (struct attribute *)pos;
			printf("interest: %s=%s:%lu\n", 
			       haggle_attribute_get_name(attr), 
			       haggle_attribute_get_value(attr), 
			       haggle_attribute_get_weight(attr));
		}
	} else {		
		// No old interests: Create new interests.
		if (gridSize > 0) {
			createInterestGrid();
		} else {
			createInterestBinomial();
		}
	}
	return 0;
}

#ifdef OS_WINDOWS_MOBILE
static int termCode;
int getTermCode(void)
{
	return termCode;
}
/* DLL entry point */
BOOL APIENTRY DllMain( HANDLE hModule, 
		      DWORD  ul_reason_for_call, 
		      LPVOID lpReserved)
{
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		test_loop_event = CreateEvent(NULL, FALSE, FALSE, NULL);
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		CloseHandle(test_loop_event);
		break;
	}
	return TRUE;
}

LUCKYME_API int isNotified(void)
{
	return notify_flag?1:0;
}

LUCKYME_API void resetNotification(void)
{
	notify_flag = false;
}


int startTest()
{
	test_is_running = true;
	numberOfDOsReceived = 0;

	LIBHAGGLE_DBG("Checking test_loop_event\n");
	DWORD ret = WaitForSingleObject(test_loop_event, 0);

	switch (ret) {
		case WAIT_TIMEOUT:
			LIBHAGGLE_DBG("Setting test_loop_event\n");
			SetEvent(test_loop_event);
			break;
		case WAIT_OBJECT_0:
			return 0;
		case WAIT_FAILED:
			return -1;
	}
	return 1;
}

int stopTest()
{
	test_is_running = false;

	DWORD ret = WaitForSingleObject(test_loop_event, 0);

	switch (ret) {
		case WAIT_TIMEOUT:
			SetEvent(test_loop_event);
			break;
		case WAIT_OBJECT_0:
			return 0;
		case WAIT_FAILED:
			return -1;
	}
	return 1;
}

bool isHaggleRunning()
{
	return (haggle_daemon_pid(NULL) == HAGGLE_DAEMON_RUNNING);
}

int isLuckyMeRunning(void)
{
	if (luckyme_thread_handle != NULL && hh && haggle_event_loop_is_running(hh))
		return 1;

	return 0;
}

int isTestRunning()
{
	return test_is_running ? 1 : 0;
}

unsigned int getNumberOfDOsReceived(void)
{
	if (!isLuckyMeRunning())
		return 0;

	return numberOfDOsReceived;
}

unsigned int getNumberOfDOsCreated(void)
{
	if (!isLuckyMeRunning())
		return 0;

	return numberOfDOsCreated;
}

unsigned int getNumberOfNeighbors(void)
{
	if (!isLuckyMeRunning())
		return 0;

	return numberOfNeighbors;
}

char *getNeighborName(unsigned int n)
{
	if (n >= 0 && n < getNumberOfNeighbors()) {
		return neighborName[n];
	}
	return NULL;
}

char *getLuckyMeDebugString(void)
{
	return debugString;
}

int startHaggle(void)
{
	if (haggle_daemon_pid(NULL) == HAGGLE_DAEMON_RUNNING) {
		return 0;
	}

	return haggle_daemon_spawn(NULL) == HAGGLE_NO_ERROR;
}


int stopHaggle(void)
{
	if (haggle_daemon_pid(NULL) == HAGGLE_DAEMON_RUNNING) {
			haggle_ipc_shutdown(hh);
			return 1;
	}
	return 0;
}

int startLuckyMe(void)
{
	if (isLuckyMeRunning())
		return 0;

	luckyme_thread_handle = CreateThread(NULL, 0, luckyme_thread, (void *)NULL, 0, 0);

	if (luckyme_thread_handle == NULL) {
		LIBHAGGLE_ERR("Could not start luckyme thread\n");
		return -1;
	}
	return 1;
}

int stopLuckyMe(int stopAlsoHaggle)
{
	if (isLuckyMeRunning()) {

		StopNow = true;

		if (stopTest() < 0)
			return -1;

		if (isHaggleRunning() && stopAlsoHaggle) {
			calledHaggleShutdown = true;
			stopHaggle();
		}

		DWORD ret = WaitForSingleObject(luckyme_thread_handle, INFINITE);

		if (ret == WAIT_OBJECT_0) {
			/* Is this really necessary here or will the handle be closed by the event loop? */
			CloseHandle(luckyme_thread_handle);
			sprintf(debugString, "LuckyMe stopped successfully\n");
		} else if (ret == WAIT_FAILED) {
			sprintf(debugString, "Could not stop test... wait FAILED\n");
		} else if (ret == WAIT_TIMEOUT) {
			sprintf(debugString, "Could not stop test... wait TIMEOUT\n");
		} else {
			// Should not happen
		}
	}
	return 0;
}

#endif /* OS_WINDOWS_MOBILE */


void on_event_loop_stop(void *arg)
{
	if (hh) {
		LIBHAGGLE_DBG("Freeing Haggle handle\n");
		haggle_handle_free(hh);
		hh = NULL;
	}
}

#if defined(OS_WINDOWS)
DWORD WINAPI luckyme_thread(void *)
#else
int luckyme_thread()
#endif
{
	int ret = 0;

	attrPoolSize = 1;
	numDataObjectAttributes = 1;
	varianceInterestAttributes = 0;
#if defined(OS_WINDOWS_MOBILE)
	numberOfDOsReceived = 0;
	numberOfDOsCreated = 0;
	numberOfNeighbors = 0;
	neighborName = NULL;
#endif
	
	if (hh)
		return -1;

	// reset random number generator
	prng_init();
	
	StopNow = false;
	// get hostname (used to set interest)
	gethostname(hostname, 128);
	nodeNumber = atoi(&(hostname[5]));

	// register with haggle
	unsigned int retry = 3;

	do {
		ret = haggle_handle_get(APP_NAME, &hh);

		// Busy?
		if (ret == HAGGLE_BUSY_ERROR) {
			// Unregister and try again.
			haggle_unregister(APP_NAME);
		}
		retry--;
#ifdef OS_WINDOWS_MOBILE
		Sleep(5000);
#endif
	} while (ret != HAGGLE_NO_ERROR && retry >= 0);

	if (ret != HAGGLE_NO_ERROR || hh == NULL) {
		//printf("could not obtain Haggle handle\n");
#ifdef OS_WINDOWS_MOBILE
		termCode = ret;
#endif
		goto out_error;
	}
	haggle_event_loop_register_callbacks(hh, NULL, on_event_loop_stop, hh);

	// register callback for new data objects
	haggle_ipc_register_event_interest(hh, LIBHAGGLE_EVENT_NEIGHBOR_UPDATE, onNeighborUpdate);
	haggle_ipc_register_event_interest(hh, LIBHAGGLE_EVENT_NEW_DATAOBJECT, onDataobject);
	haggle_ipc_register_event_interest(hh, LIBHAGGLE_EVENT_SHUTDOWN, onShutdown);
	haggle_ipc_register_event_interest(hh, LIBHAGGLE_EVENT_INTEREST_LIST, onInterests);

	if (haggle_event_loop_run_async(hh) != HAGGLE_NO_ERROR)
		goto out_error;
	
	// retreive interests:
	if (haggle_ipc_get_application_interests_async(hh) != HAGGLE_NO_ERROR)
		goto out_error;

	NOTIFY();

	testLoop();
	
	// Join with libhaggle thread
	if (isHaggleRunning() && calledHaggleShutdown) {
		// if we called shutdown, wait to free the haggle handle until
		// we get the shutdown callback
		LIBHAGGLE_DBG("Deferring event loop stop and freeing of Haggle handle until shutdown event\n");
	} else {
		if (haggle_event_loop_is_running(hh)) {
			LIBHAGGLE_DBG("Stopping event loop\n");
			haggle_event_loop_stop(hh);
		}
	
		LIBHAGGLE_DBG("Freeing Haggle handle\n");
		haggle_handle_free(hh);
	}
	// Clean up neighbor list
	if (neighborName != NULL) {
		unsigned int i;
		for (i = 0; i < numberOfNeighbors; i++)
			free(neighborName[i]);
		free(neighborName);
	}
	numberOfNeighbors = 0;
	neighborName = NULL;

	return 0;

out_error:
	if (hh)
		haggle_handle_free(hh);

	return ret;
}

#if defined(OS_UNIX)
void signal_handler()
{
	StopNow = true;
	stopTest();
}

int main (int argc, char *argv[]) 
{
	signal(SIGINT,  signal_handler);      // SIGINT is what you get for a Ctrl-C
	ParseArguments(argc, argv);

	return luckyme_thread();
}
#endif /* OS_UNIX */
