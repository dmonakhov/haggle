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


#include <libhaggle/haggle.h>
#include "luckyme.h"

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


unsigned long grid_size = 0;
char *filename = "\\luckyMeData.jpg";
char *single_source_name = NULL;
unsigned long create_data_interval = 30;
unsigned long attribute_pool_size = 100;
unsigned long num_dataobject_attributes = 3;
unsigned long variance_interest_attributes = 2;
unsigned long node_number = 0;
unsigned long repeatableSeed = 0;

#define APP_NAME "LuckyMe"

haggle_handle_t hh;

char hostname[128];
unsigned long num_dobj_received = 0;
unsigned long num_dobj_created = 0;
unsigned long num_neighbors = 0;
char **neighbors = NULL;

int called_haggle_shutdown = 0;
static int stop_now = 0;

#if defined(OS_WINDOWS_MOBILE)
DWORD WINAPI luckyme_run(void *);
static callback_function_t callback = NULL;

#define NOTIFY() { if (callback) { callback(EVENT_TYPE_STATUS_UPDATE); } }
#define NOTIFY_ERROR() { if (callback) { callback(EVENT_TYPE_ERROR); } }

static HANDLE luckyme_thread_handle = NULL;
static HANDLE haggle_start_thread_handle = NULL;
static HANDLE test_loop_event = NULL;
static HANDLE luckyme_start_event = NULL;
static int test_is_running = 0;

#else
#define NOTIFY()
#define NOTIFY_ERROR()
static int test_loop_event[2];
#endif

static double fac(int n)
{
	double t = 1;
	int i;
	
	for (i = n; i > 1; i--)
		t *= i;
	
	return t;
}

static const char *ulong_to_str(unsigned long n)
{
	static char buf[32];
	
	snprintf(buf, 32, "%lu", n);
	
	return buf;
}

static void luckyme_prng_init()
{
#ifdef OS_UNIX
	if (repeatableSeed) {
		return srandom(node_number);
	}
#endif
	prng_init();
}

static unsigned int luckyme_prng_uint32()
{
#ifdef OS_UNIX
	if (repeatableSeed) {
		return random();
	}
#endif
	return prng_uint32();
}


void luckyme_dataobject_set_createtime(struct dataobject *dobj)
{
#ifdef OS_UNIX
	if (repeatableSeed) {
		struct timeval ct;
		ct.tv_sec = num_dobj_created;
		ct.tv_usec = node_number;
		haggle_dataobject_set_createtime(dobj, &ct);
	} else {
		haggle_dataobject_set_createtime(dobj, NULL);
	}
#else
	haggle_dataobject_set_createtime(dobj, NULL);
#endif
}

#ifdef OS_WINDOWS_MOBILE

/* DLL entry point */
BOOL APIENTRY DllMain( HANDLE hModule, 
		      DWORD  ul_reason_for_call, 
		      LPVOID lpReserved)
{
	switch (ul_reason_for_call) {
		case DLL_PROCESS_ATTACH:
			test_loop_event = CreateEvent(NULL, FALSE, FALSE, NULL);
			luckyme_start_event = CreateEvent(NULL, FALSE, FALSE, NULL);
			break;
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
			break;
		case DLL_PROCESS_DETACH:
			CloseHandle(test_loop_event);
			CloseHandle(luckyme_start_event);
			break;
	}
	return TRUE;
}

int luckyme_test_start()
{
	DWORD ret;
	test_is_running = 1;
	num_dobj_received = 0;
	
	LIBHAGGLE_DBG("Checking test_loop_event\n");

	ret = WaitForSingleObject(test_loop_event, 0);
	
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

int luckyme_test_stop()
{
	DWORD ret;
	test_is_running = 0;
	
	ret = WaitForSingleObject(test_loop_event, 0);
	
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

int luckyme_is_running(void)
{
	if (luckyme_thread_handle != NULL && hh && haggle_event_loop_is_running(hh))
		return 1;
	
	return 0;
}

int luckyme_is_test_running()
{
	return test_is_running ? 1 : 0;
}

unsigned long luckyme_get_num_dataobjects_received(void)
{
	return num_dobj_received;
}

unsigned long luckyme_get_num_dataobjects_created(void)
{
	return num_dobj_created;
}

unsigned long luckyme_get_num_neighbors(void)
{
	return num_neighbors;
}

char *luckyme_get_neighbor(unsigned int i)
{
	if (i < num_neighbors) {
		return neighbors[i];
	}
	return NULL;
}

int luckyme_haggle_start(void)
{
	if (haggle_daemon_pid(NULL) == HAGGLE_DAEMON_RUNNING) {
		return 0;
	}
	
	return haggle_daemon_spawn(NULL) == HAGGLE_NO_ERROR;
}


int luckyme_haggle_stop(void)
{
	if (haggle_daemon_pid(NULL) == HAGGLE_DAEMON_RUNNING) {
		if (!hh)
			return -1;

		haggle_ipc_shutdown(hh);

		return 1;
	}
	return 0;
}

int luckyme_start(void)
{
	DWORD ret;

	if (luckyme_is_running())
		return 0;
	
	/* Set values we use on Windows mobile */
	attribute_pool_size = 1;
	num_dataobject_attributes = 1;
	variance_interest_attributes = 0;
		
	luckyme_thread_handle = CreateThread(NULL, 0, luckyme_run, (void *)NULL, 0, 0);
	
	if (luckyme_thread_handle == NULL) {
		LIBHAGGLE_ERR("Could not start luckyme thread\n");
		return -1;
	}

	ret = WaitForSingleObject(luckyme_start_event, 90000);
	
	switch (ret) {
		case WAIT_TIMEOUT:
			LIBHAGGLE_ERR("Timeout while waiting for luckyme thread to start\n");
			return -1;
			break;
		case WAIT_FAILED:
			LIBHAGGLE_ERR("Wait error while waiting for luckyme thread to start\n");
			return -1;
			break;
		case WAIT_OBJECT_0:
			break;
	}
	return 1;
}

int luckyme_stop(int stop_haggle)
{
	int retval = 0;

	if (luckyme_is_running()) {
		DWORD ret;

		stop_now = 1;
		
		if (luckyme_test_stop() < 0)
			return -1;
		
		if (haggle_daemon_pid(NULL) == HAGGLE_DAEMON_RUNNING && stop_haggle) {
			called_haggle_shutdown = 1;
			if (luckyme_haggle_stop())
				retval++;
		}

		/* 
		  Check again if test is running since Haggle shutdown event
		  could have shut us down.
		  */
		if (luckyme_is_running()) {
			// Wait for 10 seconds
			ret = WaitForSingleObject(luckyme_thread_handle, 10000);

			if (ret == WAIT_OBJECT_0) {
				/* Is this really necessary here or will the handle be closed by the event loop? */
				CloseHandle(luckyme_thread_handle);
				retval++;
			} else if (ret == WAIT_FAILED) {
			} else if (ret == WAIT_TIMEOUT) {
				LIBHAGGLE_DBG("Wait timeout when stopping test...\n");
				return -1;
			} else {
				// Should not happen
			}
		} else {
			retval++;
		}
	}
	return retval;
}

int set_callback(callback_function_t _callback)
{
	if (callback != NULL)
		return 0;
	
	callback = _callback;
	
	return 1;
}

#endif /* OS_WINDOWS_MOBILE */

// --- luckyMe functions (create interest and data) 

/* 
 we are interested in a number of random attributes.
 */
int create_interest_binomial() 
{
	int i = 0;
	struct attributelist *al;	
	const unsigned long luck = luckyme_prng_uint32() % attribute_pool_size;
	// use binomial distribution to approximate normal distribution (sum of weights = 100)
	// mean = np = luck, variance = np(1-p) = LUCK_INTERESTS
	double p = 0.5;
	unsigned long n = (unsigned long)(4 * sqrt((double)variance_interest_attributes));
	unsigned long u = (unsigned long)(n * p);
	
	LIBHAGGLE_DBG("create interest (luck=%ld)\n", luck);
	// printf("binomial distribution  n=%u, p=%f\n", n, p);
	
	al = haggle_attributelist_new();
	
	if (!al)
		return -1;
	
	for (i = 0; (unsigned long)i <= n; i++) {
		unsigned long interest = (luck + i - u + attribute_pool_size) % attribute_pool_size;
		unsigned long weight = (unsigned long)(100 * fac(n)/(fac(n-i)*fac(i))*pow(p,i)*pow(1-p,n-i));
		if (weight > 0) {
			struct attribute *attr = haggle_attribute_new_weighted(APP_NAME, ulong_to_str(interest), weight);
			haggle_attributelist_add_attribute(al, attr);
			LIBHAGGLE_DBG("   %s=%s:%lu\n", haggle_attribute_get_name(attr), haggle_attribute_get_value(attr), haggle_attribute_get_weight(attr));
		}
	}
	
	haggle_ipc_add_application_interests(hh, al);
	
	haggle_attributelist_free(al);
	
	return (i + 1);
}


/* 
 we are interested in two attributes,
 corresponding to row and column(+gridSize) in the grid. 
 */
int create_interest_grid() 
{
	int i = 0;
	struct attributelist *al;
#define NUM_INTERESTS (2)
	unsigned int interests[NUM_INTERESTS];
	
	interests[0] = node_number / grid_size;
	interests[1] = (node_number % grid_size) + grid_size;
	
	LIBHAGGLE_DBG("create interes\n");
	
	al = haggle_attributelist_new();
	
	if (!al)
		return -1;
	
	for (i = 0; i < NUM_INTERESTS; i++) {
		struct attribute *attr = haggle_attribute_new_weighted(APP_NAME, ulong_to_str(interests[i]), 1);
		haggle_attributelist_add_attribute(al, attr);
		LIBHAGGLE_DBG("   %s=%s:%lu\n", haggle_attribute_get_name(attr), haggle_attribute_get_value(attr), haggle_attribute_get_weight(attr));

	}
	
	haggle_ipc_add_application_interests(hh, al);
	
	haggle_attributelist_free(al);

	return i;
}


/*
 we create a dataobject with a number of random attributes.
 at the moment the attributes are uniformly distributed.
 the intention is to build in some correlation to the own
 interests and/or the dataobjects that one received.
 */
int create_dataobject_random() 
{
	unsigned int i = 0;
	struct dataobject *dobj = NULL;
	
	if (filename) {
		dobj = haggle_dataobject_new_from_file(filename);
		
		if (!dobj)
			dobj = haggle_dataobject_new();
	} else {
		dobj = haggle_dataobject_new();
	}
	
	if (!dobj)
		return -1;
	
	LIBHAGGLE_DBG("create data object %lu\n", num_dobj_created);
	
	// todo: get unique attributes
	for (i = 0; i < num_dataobject_attributes; i++) {
		unsigned long value = luckyme_prng_uint32() % attribute_pool_size;
		haggle_dataobject_add_attribute(dobj, APP_NAME, ulong_to_str(value));
		printf("   %s=%s\n", APP_NAME, ulong_to_str(value));
	}

	luckyme_dataobject_set_createtime(dobj);
	haggle_ipc_publish_dataobject(hh, dobj);
	haggle_dataobject_free(dobj);
	
	num_dobj_created++;
	
	return 1;
}


/*
 we create a dataobject with a number of attributes.
 all combinations from 1 attribute to 2*gridSize attributes generated.
 */
int create_dataobject_grid() 
{
	struct dataobject *dobj = NULL;
	unsigned int max_dataobject_number = (1 << (2*grid_size));	
	int i = 0;
	
	if (num_dobj_created > max_dataobject_number-1) {
		//shutdown(0);
	}
	
	if (filename) {
		dobj = haggle_dataobject_new_from_file(filename);

		if (!dobj)
			dobj = haggle_dataobject_new();
	} else {
		dobj = haggle_dataobject_new();
	}
	
	if (!dobj)
		return -1;
	
	LIBHAGGLE_DBG("create data object %lu\n", num_dobj_created);
	
	for (i = 0; i < 6; i++) {
		if ((num_dobj_created>>i) & 0x01) {
			haggle_dataobject_add_attribute(dobj, APP_NAME, ulong_to_str((unsigned long)i));
			LIBHAGGLE_DBG("   %s=%s\n", APP_NAME, ulong_to_str((unsigned long)i));
		}
	}
	
	luckyme_dataobject_set_createtime(dobj);
	haggle_ipc_publish_dataobject(hh, dobj);
	haggle_dataobject_free(dobj);
	
	num_dobj_created++;
	
	return 1;
}


int on_dataobject(haggle_event_t *e, void* nix)
{
	char *xml;

	num_dobj_received++;

#if defined(OS_WINDOWS_MOBILE)
	if (callback)
		callback(EVENT_TYPE_NEW_DATAOBJECT);
#endif

	xml = (char *)haggle_dataobject_get_raw(e->dobj);

	if (xml) {
		LIBHAGGLE_DBG("Received data object:\n%s\n", xml);
		free(xml);
	}
	
	return 0;
}

static void neighbor_list_clear()
{
	unsigned long i;

	if (num_neighbors) {
		for (i = 0; i < num_neighbors; i++) {
			free(neighbors[i]);
		}
		free(neighbors);
	}
	num_neighbors = 0;
	neighbors = NULL;
}

int on_neighbor_update(haggle_event_t *e, void* nix)
{
	haggle_nodelist_t *nl = e->neighbors;
	unsigned long i;
	
	neighbor_list_clear();
	
	num_neighbors = haggle_nodelist_size(nl);

	LIBHAGGLE_DBG("number of neighbors is %lu\n", num_neighbors);
	
	if (num_neighbors > 0) {
		neighbors = (char **)malloc(sizeof(char *) * num_neighbors);
		
		if (!neighbors) {
			num_neighbors = 0;
		} else {
			list_t *pos;
			i = 0;
			list_for_each(pos, &nl->nodes) {
				haggle_node_t *n = (haggle_node_t *)pos;
				neighbors[i] = (char *)malloc(strlen(haggle_node_get_name(n)) + 1);
				
				if (neighbors[i]) {
					strcpy(neighbors[i], haggle_node_get_name(n));
				}
				i++;
			}
		}
	} 
#if defined(OS_WINDOWS_MOBILE)
	if (callback)
		callback(EVENT_TYPE_NEIGHBOR_UPDATE);
#endif
	
	return 0;
}

int on_shutdown(haggle_event_t *e, void* nix)
{
#if defined(OS_WINDOWS_MOBILE)
	
	LIBHAGGLE_DBG("Got shutdown event\n");
	
	if (hh && called_haggle_shutdown) {
		if (haggle_event_loop_is_running(hh)) {
			LIBHAGGLE_DBG("Stopping event loop\n");
			haggle_event_loop_stop(hh);
		}
	}
	
	if (callback)
		callback(EVENT_TYPE_SHUTDOWN);
	
#else
	ssize_t ret;
	stop_now = 1;
	ret = write(test_loop_event[1], "x", 1);
#endif
	return 0;
}

int on_interests(haggle_event_t *e, void* nix)
{
	LIBHAGGLE_DBG("Received application interests event\n");

	if (haggle_attributelist_get_attribute_by_name(e->interests, APP_NAME) != NULL) {
		list_t *pos;

		LIBHAGGLE_DBG("Checking existing interests\n");

		// We already have some interests, so we don't create any new ones.
		
		// In the future, we might want to delete the old interests, and
		// create new ones... depending on the circumstances.
		// If so, that code would fit here. 
		
		list_for_each(pos, &e->interests->attributes) {
			struct attribute *attr = (struct attribute *)pos;
			LIBHAGGLE_DBG("interest: %s=%s:%lu\n", 
			       haggle_attribute_get_name(attr), 
			       haggle_attribute_get_value(attr), 
			       haggle_attribute_get_weight(attr));
		}
	} else {
		LIBHAGGLE_DBG("No existing interests, generating new ones\n");

		// No old interests: Create new interests.
		if (grid_size > 0) {
			create_interest_grid();
		} else {
			create_interest_binomial();
		}
	}
	
	return 0;
}



// ----- USAGE (parse arguments)

#if defined(OS_UNIX)

static void print_usage()
{	
	fprintf(stderr, 
		"Usage: ./%s [-A num] [-d num] [-i num] [-t interval] [-g gridSize] [-s hostname] [-f path]\n", 
		APP_NAME);
	fprintf(stderr, "          -A attribute pool (default %lu)\n", attribute_pool_size);
	fprintf(stderr, "          -d number of attributes per data object (default %lu)\n", num_dataobject_attributes);
	fprintf(stderr, "          -i interest variance (default %lu)\n", variance_interest_attributes);
	fprintf(stderr, "          -g use grid topology (gridSize x gridSize, overwrites -Adi, default off)\n");
	fprintf(stderr, "          -t interval to create data objects [s] (default %lu)\n", create_data_interval);
	fprintf(stderr, "          -r repeatable experiments (constant seed, incremental createtime, default off)\n");
	fprintf(stderr, "          -s singe source (create data objects only on node 'name', default off)\n");
	fprintf(stderr, "          -f data file to be sent (default off)\n");
}

static void parse_commandline(int argc, char **argv)
// Parses our command line arguments into the global variables 
// listed above.
{
	int ch;
	
	// Parse command line options using getopt.
	
	do {
		ch = getopt(argc, argv, "A:d:i:t:g:rs:f:");
		if (ch != -1) {
			switch (ch) {
				case 'A':
					attribute_pool_size = strtoul(optarg, NULL, 10);
					break;
				case 'd':
					num_dataobject_attributes = strtoul(optarg, NULL, 10);
					break;
				case 'i':
					variance_interest_attributes = strtoul(optarg, NULL, 10);
					break;
				case 't':
					create_data_interval = strtoul(optarg, NULL, 10);
					break;
				case 'g':
					grid_size = strtoul(optarg, NULL, 10);
					attribute_pool_size = 2 * grid_size;
					break;
				case 'r':
					repeatableSeed = 1;
					break;
				case 's':
					single_source_name = optarg;
					break;
				case 'f':
					filename = optarg;
					break;
				default:
					print_usage();
					exit(1);
					break;
			}
		}
	} while (ch != -1);
	
	// Check for any left over command line arguments.
	
	if (optind != argc) {
		print_usage();
		fprintf(stderr, "%s: Unexpected argument '%s'\n", argv[0], argv[optind]);
		exit(1);
	}
	
}

#endif // OS_UNIX

void test_loop() {
	int result = 0;
		
#if defined(OS_WINDOWS_MOBILE)
	// Signal that we are running
	SetEvent(luckyme_start_event);
#endif
	while (!stop_now) {
#if defined(OS_WINDOWS)
		DWORD timeout = test_is_running ? create_data_interval * 1000 : INFINITE;
		
		DWORD ret = WaitForSingleObject(test_loop_event, timeout);
		
		switch (ret) {
			case WAIT_OBJECT_0:
				// Set result 1, i.e., create no data object.
				// We just check whether the test is running or not, or
				// if we should exit
				result = 1;
				break;
			case WAIT_FAILED:
				stop_now = 1;
				result = -1;
				LIBHAGGLE_DBG("Wait failed!!\n");
				break;
			case WAIT_TIMEOUT:
				result = 0;
				break;
		}
#else
		struct timeval timeout;
		unsigned int nfds = test_loop_event[0] + 1;
		fd_set readfds;
		timeout.tv_sec = (long)create_data_interval;
		timeout.tv_usec = 0;
		
		FD_ZERO(&readfds);
		FD_SET(test_loop_event[0], &readfds);
		
		result = select(nfds, &readfds, NULL, NULL, &timeout);
#endif
		if (result < 0) {
			if (ERRNO != EINTR) {
				LIBHAGGLE_ERR("Stopping due to error!\n");
				stop_now = 1;
			}
		} else if (result == 0) {
			if (single_source_name == NULL || strcmp(hostname, single_source_name) == 0) {
				LIBHAGGLE_DBG("Creating data object\n");
				if (grid_size > 0) {
					create_dataobject_grid();
				} else {
					create_dataobject_random();
				}
#if defined(OS_WINDOWS_MOBILE)
				if (callback)
					callback(EVENT_TYPE_DATA_OBJECT_GENERATED);
#endif
			}
		}
	}
	LIBHAGGLE_DBG("test loop done!\n");
}


void on_event_loop_start(void *arg)
{	
	haggle_handle_t hh = (haggle_handle_t)arg;

	luckyme_prng_init();
	/* retreive interests: */
	if (haggle_ipc_get_application_interests_async(hh) != HAGGLE_NO_ERROR) {
		LIBHAGGLE_DBG("Could not request interests\n");
	}
}

void on_event_loop_stop(void *arg)
{
	haggle_handle_t hh = (haggle_handle_t)arg;

	if (hh) {
		LIBHAGGLE_DBG("Freeing Haggle handle\n");
		haggle_handle_free(hh);
		hh = NULL;
	}
	
	// Clean up neighbor list
	neighbor_list_clear();
}

#if defined(OS_WINDOWS)
DWORD WINAPI luckyme_run(void *arg)
#else
int luckyme_run()
#endif
{
	int ret = 0;
	unsigned int retry = 3;
	
#if defined(OS_UNIX)
	ret = pipe(test_loop_event);

	if (ret == -1) {
		LIBHAGGLE_ERR("Could not open pipe\n");
		return -1;
	}
#endif
	stop_now = 0;
	// get hostname (used to set interest)
	gethostname(hostname, 128);
	node_number = atoi(&(hostname[5]));

	// reset random number generator
	luckyme_prng_init();
		
	do {
		ret = haggle_handle_get(APP_NAME, &hh);
		
		// Busy?
		if (ret == HAGGLE_BUSY_ERROR) {
			// Unregister and try again.
			LIBHAGGLE_DBG("Application is already registered.\n");
			haggle_unregister(APP_NAME);
		}
#ifdef OS_WINDOWS_MOBILE
		Sleep(5000);
#endif
	} while (ret != HAGGLE_NO_ERROR && retry-- != 0);
	
	if (ret != HAGGLE_NO_ERROR || hh == NULL) {
		LIBHAGGLE_ERR("Could not get Haggle handle\n");
		goto out_error;
	}
	
	ret = haggle_event_loop_register_callbacks(hh, on_event_loop_start, on_event_loop_stop, hh);
	
	if (ret != HAGGLE_NO_ERROR) {
		LIBHAGGLE_ERR("Could not register start and stop callbacks\n");
		goto out_error;
	}
	// register callback for new data objects
	ret = haggle_ipc_register_event_interest(hh, LIBHAGGLE_EVENT_SHUTDOWN, on_shutdown);

	if (ret != HAGGLE_NO_ERROR) {
		LIBHAGGLE_ERR("Could not register shutdown event interest\n");
		goto out_error;
	}

	ret = haggle_ipc_register_event_interest(hh, LIBHAGGLE_EVENT_NEIGHBOR_UPDATE, on_neighbor_update);

	if (ret != HAGGLE_NO_ERROR) {
		LIBHAGGLE_ERR("Could not register neighbor update event interest\n");
		goto out_error;
	}

	ret = haggle_ipc_register_event_interest(hh, LIBHAGGLE_EVENT_NEW_DATAOBJECT, on_dataobject);

	if (ret != HAGGLE_NO_ERROR) {
		LIBHAGGLE_ERR("Could not register new data object event interest\n");
		goto out_error;
	}

	ret = haggle_ipc_register_event_interest(hh, LIBHAGGLE_EVENT_INTEREST_LIST, on_interests);

	if (ret != HAGGLE_NO_ERROR) {
		LIBHAGGLE_ERR("Could not register interest list event interest\n");
		goto out_error;
	}
	
	if (haggle_event_loop_run_async(hh) != HAGGLE_NO_ERROR) {
		LIBHAGGLE_ERR("Could not start event loop\n");
		goto out_error;
	}
		
	NOTIFY();
	
	test_loop();
	
	// Join with libhaggle thread
	if (haggle_daemon_pid(NULL) == HAGGLE_DAEMON_RUNNING && called_haggle_shutdown) {
		// if we called shutdown, wait to free the haggle handle until
		// we get the shutdown callback
		LIBHAGGLE_DBG("Deferring event loop stop until shutdown event\n");
	} else {
		if (haggle_event_loop_is_running(hh)) {
			LIBHAGGLE_DBG("Stopping event loop\n");
			haggle_event_loop_stop(hh);
		}
	}
	
	return 0;
	
out_error:
	NOTIFY_ERROR();

	if (hh)
		haggle_handle_free(hh);
	
	return ret;
}

#if defined(OS_UNIX)
void signal_handler()
{
	ssize_t ret;
	stop_now = 1;
	ret = write(test_loop_event[1], "x", 1);
}

int main(int argc, char **argv)
{
	signal(SIGINT, signal_handler);      // SIGINT is what you get for a Ctrl-C
	parse_commandline(argc, argv);
	
	return luckyme_run();
}
#elif defined(OS_WINDOWS_MOBILE) && defined(CONSOLE)
int wmain()
{
	test_is_running = 1;
	test_loop_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	luckyme_run(NULL);
	CloseHandle(test_loop_event);
}
#endif
