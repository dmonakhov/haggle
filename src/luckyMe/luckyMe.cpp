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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <cerrno>
#include "regex.h"
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <strings.h>

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

#include <math.h>

#include <cstdlib>
#include <cstdio>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "../utils/utils.h"

// ---
#define NUM_CONNECTIONS 1
#define HTTP_PORT 8082

#define APP_NAME "LuckyMe"

#define RANDOM_ORDER 30			// attribute set
#define LUCK_INTERESTS 2		// variance of interests
#define LUCK_ATTRS 3			// number of attributes in data objects
#define SEND_INTERVAL 2

// ---

using namespace std;

string filepath = "/";
string serverpath;

SOCKET httpListenSock;
SOCKET httpCommSock;
haggle_handle_t haggleHandle;

unsigned int interests[RANDOM_ORDER];


// This mutex protects the ResultString and NeighborString
#ifdef OS_WINDOWS
typedef CRITICAL_SECTION mutex_t;
#elif defined(OS_UNIX)
#include <pthread.h>
typedef pthread_mutex_t mutex_t;
#endif
#include <libgen.h>


mutex_t mutex;

static void mutex_init(mutex_t *m)
{
#ifdef OS_WINDOWS
	InitializeCriticalSection(m);
#elif defined(OS_UNIX)
	pthread_mutex_init(m, NULL);
#endif
}
static void mutex_del(mutex_t *m)
{
	
#ifdef OS_WINDOWS
	DeleteCriticalSection(m);
#elif defined(OS_UNIX)
	pthread_mutex_destroy(m);
#endif
}
static void mutex_lock(mutex_t *m)
{
	
#ifdef OS_WINDOWS
	EnterCriticalSection(m);
#elif defined(OS_UNIX)
	pthread_mutex_lock(m);
#endif
}

static void mutex_unlock(mutex_t *m)
{
	
#ifdef OS_WINDOWS
	LeaveCriticalSection(m);
#elif defined(OS_UNIX)
	pthread_mutex_unlock(m);
#endif
}

stringstream ResultString;
stringstream NeighborString;
stringstream DebugString;

int countDataObject = 0;




double fac(int n)
{
    double t=1;
    for (int i=n;i>1;i--)
        t*=i;
    return t;
}



/* 
 we are interested in a number of random attributes.
 will we get some matches ?
 */
void createInterest() 
{
	unsigned int i = 0;
	char luckAttrValue[32];
	struct attributelist *attrList = haggle_attributelist_new();
	struct attribute *attr = 0;
	
	struct timeval t;
	gettimeofday(&t, NULL);
	srandom(t.tv_usec);

	for (i=0; i<RANDOM_ORDER; i++) {
		interests[i] = 0;
	}

	
	const unsigned long luck = random() % RANDOM_ORDER;
	unsigned int weight = 0;
	
	// use binomial distribution to approximate normal distribution (sum of weights = 100)
	// mean = np = luck, variance = np(1-p) = LUCK_INTERESTS
	double p = 0.5;
	unsigned int n = 4 * sqrt((double)LUCK_INTERESTS);
	unsigned int u = n * p;
	
	printf("luck=%ld \n", luck);
	printf("binomial distribution  n=%u, p=%f\n", n, p);
	
	unsigned int sum_weight = 0;
	unsigned int interest = 0;
	for (i = 0; i <= n; i++) {
		interest = (luck + i - u + RANDOM_ORDER) % RANDOM_ORDER;
		weight = 100 * fac(n)/(fac(n-i)*fac(i))*pow(p,i)*pow(1-p,n-i);
		sum_weight += weight;
		if (weight > 0) {
			printf("interest: %u, %d\n", interest, weight);
			interests[interest] = weight;
			sprintf(luckAttrValue, "%u", interest);
			attr = haggle_attribute_new_weighted(APP_NAME, luckAttrValue, weight);
			haggle_attributelist_add_attribute(attrList, attr);
		}
	}
	
	printf("sum weights %u\n", sum_weight);
	
	haggle_ipc_add_application_interests(haggleHandle, attrList);
}


/*
 we create a dataobject with a number of random attributes.
 at the moment the attributes are uniformly distributed.
 the intention is to build in some correlation to the own
 interests and/or the dataobjects that one received.
 */
void createDataobject() 
{
	int i = 0;
	char luckAttrValue[32];
	struct dataobject *dObj = haggle_dataobject_new();
	
	haggle_dataobject_add_attribute(dObj, "Picture", "lancaster");

	// todo: get three unique attributes
	for (i=0; i<LUCK_ATTRS; i++) {
		sprintf(luckAttrValue, "%ld", random() % RANDOM_ORDER);
//		haggle_dataobject_add_attribute(dObj, APP_NAME, luckAttrValue);
	}
	
	haggle_ipc_publish_dataobject(haggleHandle, dObj);
}


/* 
 we get a matching dataobject.
 maybe we should reward the creator or delegate?
 */
void onDataobject(struct dataobject *dObj, void* nix)
{
	cerr << endl << "Received data object!!!" << endl;
	
	struct attribute *attr = NULL;
	int cnt = 0;
	unsigned int attr_value = 0;
	unsigned int sum = 0;
	const char* attr_name = 0;
	
	mutex_lock(&mutex);
	
	ResultString << "new luck object : "; 
	while ((attr = haggle_dataobject_get_attribute_n(dObj, cnt++))) {
		attr_name = haggle_attribute_get_name(attr);
		if (!strcmp(attr_name, APP_NAME)) {
			attr_value = atoi(haggle_attribute_get_value(attr));
			ResultString << attr_value << " ";
			printf("%u[%u] ", attr_value, interests[attr_value]);
		
			sum += interests[attr_value];
		}
	}
	ResultString << " - " << sum;
	ResultString << "<br/>";
	
	printf("luck: %u\n", sum);
	
	mutex_unlock(&mutex);
	
	haggle_dataobject_free(dObj);
}


void onNeighborUpdate(struct dataobject *dObj)
{
	list_t *pos;
	haggle_nodelist_t *nl;
	
	
	printf("Neighbor update event!\n");
	
	nl = haggle_nodelist_new_from_dataobject(dObj);
	
	if (!nl) {
		fprintf(stderr, "Could not create nodelist from data object\n");
		return;
	}
	
	mutex_lock(&mutex);
	
	NeighborString.str(""); 
	
	list_for_each(pos, &nl->nodes) {
		list_t *ppos;
		haggle_node_t *node = (haggle_node_t *)pos;
		
		printf("Neighbor: %s\n", haggle_node_get_name(node));
		
		NeighborString << "[" << haggle_node_get_name(node) << "] ";
		
		list_for_each(ppos, &node->interfaces) {
			haggle_interface_t *iface = (haggle_interface_t *)ppos;
			printf("\t%s : [%s]\n", haggle_interface_get_type_name(iface), 
			       haggle_interface_get_identifier_str(iface));
		}
	}
	
	mutex_unlock(&mutex);
	
	haggle_nodelist_free(nl);
	
	haggle_dataobject_free(dObj);
}

// -----

void sendResponse(const string filename) 
{
	stringstream Response;
	stringstream HTTPparams;
	
	ifstream FileStream(filename.c_str(), ios::binary);
	FileStream.seekg (0, ios::end);
	int filesize = FileStream.tellg();
	
	HTTPparams.clear();
	HTTPparams << "Accept-Ranges: bytes\r\n";
	
	Response << "HTTP/1.1 200 OK\r\n" << "Connection: Keep-Alive\r\n" << HTTPparams.str() << "Content-Length: " << filesize << "\r\n\r\n";
	send(httpCommSock, Response.str().c_str(), Response.str().length(), 0);
	FileStream.close();
	
	//	send_file(filename.c_str(), httpCommSock);
}


void http(SOCKET sock, char* buf, int num)
{
#define MAX_LENGTH 256
	stringstream Request;
	stringstream Response;
	
	DebugString.clear();
	
	if (sock == httpListenSock) {
		// cout << "got request: " << endl;
	} else if (sock == httpCommSock) {
		Request << buf;
		
		char line[MAX_LENGTH];
		
		while (Request.getline(line, MAX_LENGTH, '\r')) {
			if (strstr(line, "GET") || strstr(line, "POST")) {
				stringstream urlpath;
				
				/* we have to return the file requested in the url.
				 special cases: result.html and neighbour.html are dynamically generated */
				
				if (strstr(line, " /result.html HTTP")) {
					/* request for result.html > generate from ResultString */
					mutex_lock(&mutex);
					Response << "HTTP/1.1 200 OK\r\n" << "Cache-control: public\r\nContent-Length: " << ResultString.str().length() << "\r\n\r\n" << ResultString.str();
					mutex_unlock(&mutex);
					send(httpCommSock, Response.str().c_str(), Response.str().length(), 0);
				} else if (strstr(line, " /neighbor.html HTTP")) {
					/* request for neighbor.html > generate from NeighborString */
					mutex_lock(&mutex);
					Response << "HTTP/1.1 200 OK\r\n" << "Content-Length: " << NeighborString.str().length() << "\r\n\r\n" << NeighborString.str();
					mutex_unlock(&mutex);
					send(httpCommSock, Response.str().c_str(), Response.str().length(), 0);
				} else {
					sendResponse("index.html");
				}
			}
		}
		
		CLOSE_SOCKET(httpCommSock);
		httpCommSock = 0;
	}
}


// ----- Networking functions -----------------------------------

int openTcpSock(int port) 
{
	SOCKET s;
	int reuse_addr = 1;  /* Used so we can re-bind to our port
	 while a previous connection is still
	 in TIME_WAIT state. */
	struct sockaddr_in server_address; /* bind info structure */
	struct protoent* tcp_level;
	
	
	/* Obtain a file descriptor for our "listening" socket */
	s = socket(AF_INET, SOCK_STREAM, 0);
	
	if (!s) {
		fprintf(stderr, "socket error\n");
		return -1;
	}
	/* So that we can re-bind to it without TIME_WAIT problems */
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_addr, sizeof(reuse_addr));
	
	/* set tcp_nodelay */
	if ((tcp_level = getprotobyname("TCP"))) {
		setsockopt(s, tcp_level->p_proto, TCP_NODELAY, (char *)&reuse_addr, sizeof(reuse_addr));
	}
	
	/* Get the address information, and bind it to the socket */
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(port);
	
	if (bind(s, (struct sockaddr *) &server_address, sizeof(server_address)) != 0) {
		fprintf(stderr, "bind error : %d\n", ERRNO);
		CLOSE_SOCKET(s);
		return -1;
	}
	
	/* Set up queue for incoming connections. */
	listen(s,1);
	
	return s;
}


void build_select_list(unsigned int* nfds, fd_set* readfds) {
	
	if (*nfds < (unsigned int)httpListenSock + 1) 
		*nfds = httpListenSock + 1;
	
	FD_SET(httpListenSock, readfds);
	
	if (httpCommSock != 0) {
		if (*nfds < (unsigned int)httpCommSock + 1) { *nfds = (unsigned int)httpCommSock + 1; }
		FD_SET(httpCommSock, readfds);
	}
}


int handle_new_connection(SOCKET sock) {
	int connection;		/* Socket file descriptor for incoming connections */
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	
	connection = accept(sock, (struct sockaddr*)&addr, &len);
	
	if (connection < 0) {
		fprintf(stderr, "accept error : %d\n", ERRNO);
		return connection;
	}
	if (sock == httpListenSock && httpCommSock == 0) {
		httpCommSock = connection;
		http(sock, NULL, 0);
		connection = -1;
	}
	if (connection != -1) {
		/* No room left in the queue! */
		CLOSE_SOCKET(connection);
		return connection;
	}
	return 0;
}


void deal_with_data(SOCKET sock) {
#define BUFLEN 1024
	static char buf[BUFLEN];     /* Buffer for socket reads */
	int num = 0;
	
	if ((num = recv(sock, buf, BUFLEN, 0)) == 0) {
		printf("Connection lost\n");
		CLOSE_SOCKET(sock);
		sock = 0;
	} else {
		/* We got some data, so we answer to it */
		buf[num] = 0x00;
		http(sock, buf, num);
	}
}

void read_socks(fd_set* sock) {
	/* test for socket */
	if (FD_ISSET(httpListenSock, sock)) {
		handle_new_connection(httpListenSock);
	}
	/* Now check connectlist for available data */
	if (FD_ISSET(httpCommSock, sock)) {
		deal_with_data(httpCommSock);
	}
}

void closeConnections(int i) {
	printf("\nshutting down...\n");
	haggle_handle_free(haggleHandle);
	
	if (httpCommSock) 
		CLOSE_SOCKET(httpCommSock);
	if (httpListenSock) 
		CLOSE_SOCKET(httpListenSock);
	
	exit(0);
}

void eventLoop() {
	fd_set readfds;
	int result = 0;
	struct timeval timeout;
	static volatile int StopNow = 0;
	
	while (!StopNow) {
		unsigned int nfds = 0;
		timeout.tv_sec = SEND_INTERVAL;
		timeout.tv_usec = 0;
		
		FD_ZERO(&readfds);
		build_select_list(&nfds, &readfds);
		
		result = select(nfds, &readfds, NULL, NULL, &timeout);
		if (result < 0) {
			if (ERRNO != EINTR) StopNow = 1;
		} else  if (result > 0) {
			read_socks(&readfds);
		} else {
			createDataobject();
		}
	}
}

#ifdef OS_WINDOWS_MOBILE
int wmain()
{	
#else
	int main (int argc, char *argv[]) 
	{
		
		signal(SIGINT,  closeConnections);      // SIGINT is what you get for a Ctrl-C
#endif
		mutex_init(&mutex);
		
		// libhaggle will initialize winsock for us
		if(haggle_handle_get(APP_NAME, &haggleHandle) != HAGGLE_NO_ERROR) {
			goto done;
		}
		
		haggle_ipc_register_event_interest(haggleHandle, LIBHAGGLE_EVENT_NEW_DATAOBJECT, onDataobject);
		
		httpListenSock = openTcpSock(HTTP_PORT);
		
		if (!httpListenSock)
			goto done;
		
		haggle_event_loop_run_async(haggleHandle);
		
		httpCommSock = 0;
		
		createInterest();
		
		eventLoop();
		
	done:
		mutex_del(&mutex);
		
		closeConnections(0);
		
		return 1;
	}
	
	
	
	
	
