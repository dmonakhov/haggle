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

#if defined(OS_UNIX)

#include <signal.h>
#include <unistd.h>
#include <cerrno>

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

#include <cstdlib>
#include <cstdio>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "../utils/utils.h"


// --- command line arguments
unsigned int gridSize = 0;
char *fileName = 0;
char *singleSourceName = 0;
unsigned int createDataInterval = 10;

unsigned int attrPoolSize = 100;
unsigned int numDataObjectAttributes = 3;
unsigned int varianceInterestAttributes = 2;

// --- 
#define NUM_CONNECTIONS 1
#define HTTP_PORT 8082

#define APP_NAME "LuckyMe"

// ---
using namespace std;
haggle_handle_t haggleHandle;

char hostname[128];
int nodeNumber;

void shutdown(int i);


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
	
	const unsigned long luck = random() % attrPoolSize;
	unsigned int weight = 0;
	
	// use binomial distribution to approximate normal distribution (sum of weights = 100)
	// mean = np = luck, variance = np(1-p) = LUCK_INTERESTS
	double p = 0.5;
	unsigned int n = 4 * sqrt((double)varianceInterestAttributes);
	unsigned int u = n * p;

	printf("createInterest (luck=%ld)\n", luck);
	// printf("binomial distribution  n=%u, p=%f\n", n, p);

	unsigned int interest = 0;
	for (i = 0; i <= n; i++) {
		interest = (luck + i - u + attrPoolSize) % attrPoolSize;
		weight = 100 * fac(n)/(fac(n-i)*fac(i))*pow(p,i)*pow(1-p,n-i);
		if (weight > 0) {
			sprintf(luckAttrValue, "%u", interest);
			attr = haggle_attribute_new_weighted(APP_NAME, luckAttrValue, weight);
			haggle_attributelist_add_attribute(attrList, attr);
			printf("   %s=%s [%d]\n", APP_NAME, luckAttrValue, weight);
		}
	}
	
	haggle_ipc_add_application_interests(haggleHandle, attrList);
	
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
	
	printf("createInterest\n");
	for (i=0; i<numInterests; i++) {
		sprintf(luckAttrValue, "%d", interests[i]);
		attr = haggle_attribute_new_weighted(APP_NAME, luckAttrValue, 1);
		haggle_attributelist_add_attribute(attrList, attr);
		printf("   %s=%s [%d]\n", APP_NAME, luckAttrValue, 1);
	}
	
	haggle_ipc_add_application_interests(haggleHandle, attrList);
	
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
	} else {
		dObj = haggle_dataobject_new();
	}
	
	printf("createDataobject %u\n", cntDataObjectNumber);
	// todo: get unique attributes
	for (i=0; i<numDataObjectAttributes; i++) {
		sprintf(luckAttrValue, "%ld", random() % attrPoolSize);
		haggle_dataobject_add_attribute(dObj, APP_NAME, luckAttrValue);
		printf("   %s=%s\n", APP_NAME, luckAttrValue);
	}
	
	haggle_dataobject_set_createtime(dObj, NULL);
	haggle_ipc_publish_dataobject(haggleHandle, dObj);
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

	if (cntDataObjectNumber > maxDataObjectNumber-1) shutdown(0);
	
	struct dataobject *dObj = 0;
	if (fileName) {
		dObj = haggle_dataobject_new_from_file(fileName);
	} else {
		dObj = haggle_dataobject_new();
	}
	
	printf("createDataobject %u\n", cntDataObjectNumber);
	for (i=0; i<6; i++) {
		if ((cntDataObjectNumber>>i) & 0x01) {
			sprintf(luckAttrValue, "%d", i);
			haggle_dataobject_add_attribute(dObj, APP_NAME, luckAttrValue);
			printf("   %s=%s\n", APP_NAME, luckAttrValue);
		}
	}
	
	haggle_dataobject_set_createtime(dObj, NULL);
	haggle_ipc_publish_dataobject(haggleHandle, dObj);
	haggle_dataobject_free(dObj);

	cntDataObjectNumber++;
}


/* 
 we get a matching dataobject.
 maybe we should reward the creator or delegate?
 */
int onDataobject(haggle_event_t *e, void* nix)
{
	return 0;
}


int onNeighborUpdate(haggle_event_t *e, void* nix)
{
	return 0;
}

int onShutdown(haggle_event_t *e, void* nix)
{
	shutdown(0);
	
	return 0;
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

static void ParseArguments(int argc, char **argv)
// Parses our command line arguments into the global variables 
// listed above.
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


void shutdown(int i) {
	printf("\nshutting down...\n");
	haggle_handle_free(haggleHandle);
	
	exit(i);
}

void eventLoop() {
	fd_set readfds;
	int result = 0;
	struct timeval timeout;
	static volatile int StopNow = 0;
	
	while (!StopNow) {
		unsigned int nfds = 0;
		timeout.tv_sec = createDataInterval;
		timeout.tv_usec = 0;
		
		FD_ZERO(&readfds);
		
		result = select(nfds, &readfds, NULL, NULL, &timeout);
		if (result < 0) {
			if (ERRNO != EINTR) StopNow = 1;
		} else {
			if (singleSourceName == NULL || strcmp(hostname, singleSourceName) == 0) {
				if (gridSize > 0) {
					createDataobjectGrid();
				} else {
					createDataobjectRandom();
				}
			}
		}
	}
}

int onInterests(haggle_event_t *e, void* nix)
{
	if (haggle_attributelist_get_attribute_by_name(e->interests, APP_NAME) != NULL) {
		// We already have some interests, so we don't create any new ones.
		
		// In the future, we might want to delete the old interests, and
		// create new ones... depending on the circumstances.
		// If so, that code would fit here. 
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
int wmain()
#else
int main (int argc, char *argv[]) 
#endif
{	
#ifdef OS_UNIX
	signal(SIGINT,  shutdown);      // SIGINT is what you get for a Ctrl-C
#endif
	ParseArguments(argc, argv);
	
	// get hostname (used to set interest)
	gethostname(hostname, 128);
	nodeNumber = atoi(&(hostname[5]));
	
	// register with haggle
	if(haggle_handle_get(APP_NAME, &haggleHandle) != HAGGLE_NO_ERROR) {
		printf("could not obtain Haggle handle\n");
		goto done;
	}
	
	// register callback for new data objects
	// haggle_ipc_register_event_interest(haggleHandle, LIBHAGGLE_EVENT_NEW_DATAOBJECT, onDataobject);
	haggle_ipc_register_event_interest(haggleHandle, LIBHAGGLE_EVENT_SHUTDOWN, onShutdown);
	haggle_ipc_register_event_interest(haggleHandle, LIBHAGGLE_EVENT_INTEREST_LIST, onInterests);
	
	// reset random number generator
	struct timeval t;
	gettimeofday(&t, NULL);
	srandom(t.tv_usec);
	
	// start Haggle eventloop
	haggle_event_loop_run_async(haggleHandle);
	
	// retreive interests:
	haggle_ipc_get_application_interests_async(haggleHandle);
	
	// start luckyMe eventloop 
	// (data objects are created in intervals from within this eventloop)
	eventLoop();
	
done:
	shutdown(0);
	
	return 1;
}




