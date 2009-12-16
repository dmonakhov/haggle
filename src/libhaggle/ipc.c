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

#if defined(WIN32) || defined(WINCE)
#define CLOSE_SOCKET(s) closesocket(s)
#include <winsock2.h>

typedef HANDLE thread_handle_t;
typedef DWORD thread_handle_attr_t;
typedef DWORD thread_handle_id_t;
#define cleanup_ret_t void
#define start_ret_t DWORD WINAPI

#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

typedef pthread_t thread_handle_t;
typedef unsigned int thread_handle_id_t;
typedef pthread_attr_t thread_handle_attr_t;
#define cleanup_ret_t void
#define start_ret_t void*

#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define LIBHAGGLE_INTERNAL
#include <libhaggle/platform.h>
#include <libhaggle/haggle.h>

#define PID_FILE platform_get_path(PLATFORM_PATH_PRIVATE, "/haggle.pid")

#ifdef USE_UNIX_APPLICATION_SOCKET
#define HAGGLE_UNIX_SOCK_PATH "/tmp/haggle.sock"
#else
#define HAGGLE_SERVICE_DEFAULT_PORT 8787
#endif /* USE_UNIX_APPLICATION_SOCKET */

#include "debug.h"
#include "sha1.h"
#include "base64.h"

#define BUFLEN 50000 /* What would be a suitable max size */
static char buffer[BUFLEN];

#define ID_LEN SHA1_DIGEST_LENGTH
#define ID_BASE64_LEN ((((ID_LEN) + 2) / 3) * 4 + 1)

struct handler_data {
	void *arg;
	haggle_event_handler_t handler;
};

struct haggle_handle {
	list_t l;
	SOCKET sock;
#if defined(OS_UNIX)
        int signal[2];
#elif defined(OS_WINDOWS)
        HANDLE signal;
#endif
	int session_id;
	int num_handlers;
	int event_loop_running;
	thread_handle_t th;
	char id[ID_LEN];
	char id_base64[ID_BASE64_LEN];
        haggle_event_loop_start_t start;
        haggle_event_loop_stop_t stop;
        void *arg; // argument to pass to start and stop
	struct handler_data handlers[_LIBHAGGLE_NUM_EVENTS];
};

struct sockaddr_in haggle_addr;

HAGGLE_API char *haggle_directory = NULL;

HAGGLE_API int libhaggle_errno = 0;

static int num_handles = 0;

LIST(haggle_handles);

/*
	A function that sends a data object to Haggle.

	The caller may optionally block and wait for a reply with a timeout given
	in milli seconds. 

	In the case of a reply, the reply data object is returned through the
	given data object "pointer to pointer", which can be NULL otherwise. The
	timeout can be set to IO_REPLY_BLOCK for no timeout (i.e., block until
	a reply is received), or IO_REPLY_NON_BLOCK in case the function should
	try to receive a reply without waiting for it. With a positive timeout
	value, the function will block for the specified time, or until a reply
	is received.

	If no reply is asked for, the caller should set the reply data object
	pointer to NULL and set the timeout to IO_NO_REPLY.

*/
static int haggle_ipc_send_dataobject(struct haggle_handle *h, haggle_dobj_t *dobj, 
				      haggle_dobj_t **dobj_reply, long msecs_timeout);

/*
	A generic send function that takes a list of attributes and adds to a new data object
	which it subsequently sends to Haggle. It will not add any additional attributes.
*/
static int haggle_ipc_generate_and_send_control_dataobject(haggle_handle_t hh, haggle_attrlist_t *al);


/*
	Internal helper function.
*/
static int haggle_ipc_send_control_dataobject(haggle_handle_t hh, struct dataobject *dobj, 
                                              haggle_dobj_t **dobj_reply, long msecs_timeout);


enum {
        EVENT_LOOP_ERROR = -1,
        EVENT_LOOP_TIMEOUT = 0,
        EVENT_LOOP_SHOULD_EXIT = 1,
        EVENT_LOOP_SOCKET_READABLE = 2,
};

#if defined(OS_WINDOWS)

// This function is defined in platform.c, but we do not want to expose it in a header file
extern wchar_t *strtowstr(const char *str);

typedef int socklen_t;

/* DLL entry point */
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved)
{
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		libhaggle_debug_init();
		break;
	case DLL_THREAD_ATTACH:
		libhaggle_debug_init();
		break;
	case DLL_THREAD_DETACH:
		libhaggle_debug_fini();
		break;
	case DLL_PROCESS_DETACH:
		libhaggle_debug_fini();
		break;
	}
	return TRUE;
}

int wait_for_event(haggle_handle_t hh, struct timeval *tv)
{
	DWORD timeout = tv ? (tv->tv_sec * 1000) + tv->tv_usec / 1000 : INFINITE; 
	WSAEVENT eventArr[2]; 
	DWORD waitres;
	WSAEVENT socketEvent = WSACreateEvent();

	eventArr[0] = hh->signal;
	eventArr[1] = socketEvent;

	WSAEventSelect(hh->sock, socketEvent, FD_READ);

	LIBHAGGLE_DBG("In wait_for_read\n");
	waitres = WSAWaitForMultipleEvents(2, eventArr, FALSE, timeout, FALSE);

	if (waitres >= WSA_WAIT_EVENT_0 && waitres < (DWORD)(WSA_WAIT_EVENT_0 + 2)) {
		int i = (unsigned int)(waitres - WSA_WAIT_EVENT_0);

		LIBHAGGLE_DBG("Got WSAEvent i=%d\n", i);

		if (i == 0) {
			return EVENT_LOOP_SHOULD_EXIT;
		} else {
			WSANETWORKEVENTS netEvents;
			DWORD res;

			// This call will automatically reset the Event as well
			res = WSAEnumNetworkEvents(hh->sock, socketEvent, &netEvents);

			if (res == 0) {
				if (netEvents.lNetworkEvents & FD_READ) {
					// Socket is readable
					if (netEvents.iErrorCode[FD_READ_BIT] != 0) {
						LIBHAGGLE_ERR("Read error\n");
						return EVENT_LOOP_ERROR;
					}
					LIBHAGGLE_DBG("FD_READ on socket %d\n", hh->sock);
					return EVENT_LOOP_SOCKET_READABLE;
				} 
			} else {
				// Error occurred... do something to handle.
				LIBHAGGLE_DBG("WSAEnumNetworkEvents ERROR\n");
				return EVENT_LOOP_ERROR;
			}
		}
	
	} else if (waitres == WSA_WAIT_TIMEOUT) {
		return EVENT_LOOP_TIMEOUT;	
	} else if(waitres == WSA_WAIT_FAILED) {
		LIBHAGGLE_DBG("WSA_WAIT_FAILED Error=%d\n", WSAGetLastError());
		return EVENT_LOOP_ERROR;
	} 
	return EVENT_LOOP_ERROR;
}


int event_loop_signal_is_raised(haggle_handle_t hh)
{
        if (!hh || !hh->signal)
                return -1;

        return WaitForSingleObject(hh->signal, 0) == WAIT_OBJECT_0 ? 1 : 0;
}

void event_loop_signal_raise(haggle_handle_t hh)
{
        if (!hh || !hh->signal)
                return;
        
        SetEvent(hh->signal);
}


void event_loop_signal_lower(haggle_handle_t hh)
{
    if (!hh || !hh->signal)
                return;
        
        ResetEvent(hh->signal);
}
#elif defined(OS_UNIX)
#include <fcntl.h>

int wait_for_event(haggle_handle_t hh, struct timeval *tv)
{
        fd_set readfds;
        int maxfd = 0, ret = 0;

        FD_ZERO(&readfds);
        FD_SET(hh->signal[0], &readfds);
        FD_SET(hh->sock, &readfds);
        
        if (hh->signal[0] > hh->sock)
                maxfd = hh->signal[0];
        else
                maxfd = hh->sock;
        
        ret = select(maxfd + 1, &readfds, NULL, NULL, tv);

        if (ret < 0) {
                return EVENT_LOOP_ERROR;
        } else if (ret == 0) {
                return EVENT_LOOP_TIMEOUT;
        } else if (FD_ISSET(hh->signal[0], &readfds)) {
                return EVENT_LOOP_SHOULD_EXIT;
        } else if (FD_ISSET(hh->sock, &readfds)) {
                return EVENT_LOOP_SOCKET_READABLE;
        }
        return EVENT_LOOP_ERROR;
}


int event_loop_signal_is_raised(haggle_handle_t hh)
{
        char c;

        if (!hh || !hh->signal[0])
                return -1;

        return recv(hh->signal[0], &c, 1, MSG_PEEK) ? 1 : 0;
}

void event_loop_signal_raise(haggle_handle_t hh)
{
        char c = '1';
        int res;

        if (!hh || !hh->signal[1])
                return;
        
        res = write(hh->signal[1], &c, 1);
}


void event_loop_signal_lower(haggle_handle_t hh)
{
        char c;
        int res;

        if (!hh || !hh->signal[0])
                return;
        
        fcntl(hh->signal[0], F_SETFD, O_NONBLOCK);
        res = read(hh->signal[0], &c, 1);
        fcntl(hh->signal[0], F_SETFD, ~O_NONBLOCK);
}
#endif

static char *intTostr(int n) 
{
	static char intStr[5];
	
	sprintf(intStr, "%d", n);

	return intStr;
}

int haggle_get_error()
{
	return libhaggle_errno;
}

static void haggle_set_socket_error()
{
#if defined(WIN32) && defined(WINCE)
	libhaggle_errno = WSAGetLastError();
#else
	libhaggle_errno = errno;
#endif
}

#if defined(OS_WINDOWS)
typedef DWORD pid_t;
#endif

/**
   Check if the Haggle daemon is running and return its pid.
   Return 0 if Haggle is not running, -1 on error, or 1 if
   there is a valid pid of a running Haggle instance.
 */
int haggle_daemon_pid(unsigned long *pid)
{
#define PIDBUFLEN 200
        char buf[PIDBUFLEN];
        size_t ret;
        FILE *fp;
	unsigned long _pid;
#if defined(OS_WINDOWS)
        HANDLE p;
#endif
	int old_instance_is_running = 0;

	if (pid)
	        *pid = 0;

        fp = fopen(PID_FILE, "r");

        if (!fp) {
                /* The Pid file does not exist --> Haggle not running. */
                return HAGGLE_DAEMON_NOT_RUNNING;
        }

        memset(buf, 0, PIDBUFLEN);

        /* Read process id from pid file. */
        ret = fread(buf, 1, PIDBUFLEN, fp);

        fclose(fp);

        if (ret == 0)
                return HAGGLE_DAEMON_ERROR;

        _pid = strtoul(buf, NULL, 10);
        
	if (pid)
	        *pid = _pid;

        /* Check whether there is a process matching the pid */
#if defined(OS_LINUX)
        /* On Linux, do not use kill to figure out whether there is a
        process with the matching PID. This is because we run Haggle
        with root privileges on Android, and kill() can only signal a
        process running as the same user. Therefore, an application
        running with non-root privileges would not be able to use
        kill(). */

        /* Check /proc file system for a process with the matching
         * Pid */
        snprintf(buf, PIDBUFLEN, "/proc/%ld/cmdline", _pid);

        fp = fopen(buf, "r");

        if (fp != NULL) {
                size_t nitems = fread(buf, 1, PIDBUFLEN, fp);
                if (nitems && strstr(buf, "haggle") != NULL) 
                        old_instance_is_running = 1;
                fclose(fp);
	}
       
#elif defined(OS_UNIX)
        old_instance_is_running = (kill(_pid, 0) != -1);
#elif defined(OS_WINDOWS)
	
        p = OpenProcess(0, FALSE, _pid);
        old_instance_is_running = (p != NULL);

        if (p != NULL)
                CloseHandle(p);
#endif
        /* If there was a process, return its pid */
        if (old_instance_is_running)
                return HAGGLE_DAEMON_RUNNING;
        
        /* No process matching the pid --> Haggle is not running and
         * previously quit without cleaning up (e.g., Haggle crashed,
         * or the phone ran out of battery, etc.)
         */
        return HAGGLE_DAEMON_CRASHED;
}

static int spawn_daemon_internal(const char *daemonpath)
{
	int ret = HAGGLE_ERROR;
	int status;
	SOCKET sock;
	fd_set readfds;
	struct timeval tv;
	int time_left;
	int maxfd = 0;
	
#if defined(OS_WINDOWS)
	if (num_handles == 0) {
		int iResult = 0;
		WSADATA wsaData;

		// Make sure Winsock is initialized
		iResult = WSAStartup(MAKEWORD(2,2), &wsaData);

		if (iResult != 0) {
			ret = HAGGLE_WSA_ERROR;
			goto fail_setup;
		}
	}
#endif
	sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock == INVALID_SOCKET) {
		ret = HAGGLE_SOCKET_ERROR;
		haggle_set_socket_error();
		goto fail_sock;
	}
	
	haggle_addr.sin_family = AF_INET;
	haggle_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	haggle_addr.sin_port = htons(8788);

	if (bind(sock, (struct sockaddr *) &haggle_addr, sizeof(haggle_addr)) == SOCKET_ERROR) {
		ret = HAGGLE_SOCKET_ERROR;
		haggle_set_socket_error();
		goto fail_start;
	}
#if defined(OS_UNIX)
#define PATH_LEN 200
	char cmd[PATH_LEN];
	
	if (!daemonpath)
	{
		ret = HAGGLE_ERROR;
		goto fail_start;
	}

	snprintf(cmd, PATH_LEN, "%s -d", daemonpath);
	
	LIBHAGGLE_DBG("Trying to spawn daemon using %s\n", daemonpath);

	if (system(cmd) != 0) {
		LIBHAGGLE_ERR("could not start Haggle daemon\n");
		ret = HAGGLE_ERROR;
		goto fail_start;
	}

#elif defined(OS_WINDOWS)
	{
	PROCESS_INFORMATION pi;
	int ret;
#if defined(OS_WINDOWS_MOBILE) || defined(OS_WINDOWS_VISTA)
	wchar_t *path = strtowstr(daemonpath);
#else
	const char *path = daemonpath;
#endif
	if (!path)
		return HAGGLE_ERROR;

	ret = CreateProcess(path, L"", NULL, NULL, 0, 0, NULL, NULL, NULL, &pi);

#if defined(OS_WINDOWS_MOBILE) || defined(OS_WINDOWS_VISTA)
	free(path);	
#endif
	if (ret == 0) {
		LIBHAGGLE_ERR("Could not create process\n");
		ret = HAGGLE_ERROR;
		goto fail_start;
	}
	}
#endif
	
	// Wait for 60 seconds max.
	time_left = 60;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	do {
		FD_ZERO(&readfds);
		FD_SET(sock, &readfds);
		
		maxfd = sock;
		
		status = select(maxfd + 1, &readfds, NULL, NULL, &tv);

		if (status < 0) {
			ret = HAGGLE_SOCKET_ERROR;
			haggle_set_socket_error();
		} else if (ret == 0) {
			// Timeout!
			
			// FIXME: call a callback function to provide feedback.
			time_left--;
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			if (time_left == 0)
				ret = HAGGLE_DAEMON_ERROR;
		} else if (FD_ISSET(sock, &readfds)) {
			// FIXME: should probably check that the message is a correct data 
			// object first...
			ret = HAGGLE_NO_ERROR;
		}
	} while(ret == HAGGLE_ERROR);
	
fail_start:
	CLOSE_SOCKET(sock);
fail_sock:
#if defined(WIN32) || defined(WINCE)
	if (num_handles == 0) {
		WSACleanup();
	}
fail_setup:;
#endif
	return ret;
}

int haggle_daemon_spawn(const char *daemonpath)
{
        int i = 0;

        if (haggle_daemon_pid(NULL) != 0)
                return HAGGLE_NO_ERROR;

        if (daemonpath) {
                return spawn_daemon_internal(daemonpath);
        }

	while (1) {
		/*
		Need to put the definition of haggle_paths here. Otherwise,
		the call to haggle_daemon_pid() will overwrite the static 
		memory used to get the platform path.
		*/
#if defined(OS_WINDOWS)
		const char *haggle_paths[] = { platform_get_path(PLATFORM_PATH_PROGRAM, "\\Haggle.exe"), NULL };
#elif defined(OS_ANDROID)
                const char *haggle_paths[] = { platform_get_path(PLATFORM_PATH_PROGRAM, "/haggle"), NULL };
#elif defined(OS_UNIX)
		// Do some qualified guessing
		const char *haggle_paths[] = { "./haggle", "./bin/haggle", "/bin/haggle", "/usr/bin/haggle", "/usr/local/bin/haggle", "/opt/bin/haggle", "/opt/local/bin/haggle", NULL };
#endif
		FILE *fp = fopen(haggle_paths[i], "r");

		if (fp) {
			fclose(fp);
			return spawn_daemon_internal(haggle_paths[i]);
		}

		if (!haggle_paths[++i])
			break;
	}

	return HAGGLE_ERROR;
}

int haggle_handle_get_internal(const char *name, haggle_handle_t *handle, int ignore_busy_signal)
{
	int ret;
	struct haggle_handle *hh = NULL;
	struct dataobject *dobj, *dobj_reply;
	struct attribute *attr;
	SHA1_CTX ctxt;

#ifdef USE_UNIX_APPLICATION_SOCKET
#define AF_ADDRESS_FAMILY AF_UNIX
	struct sockaddr_un haggle_addr;
	socklen_t addrlen = sizeof(struct sockaddr_un);
#else
#define AF_ADDRESS_FAMILY AF_INET
	/* struct sockaddr_in local_addr; */
	/* unsigned long addrlen = sizeof(struct sockaddr_in); */
#endif

#if !defined(OS_MACOSX_IPHONE)
        if (haggle_daemon_pid(NULL) == 0)
                return HAGGLE_DAEMON_ERROR;
#endif

#if defined(OS_WINDOWS)
	if (num_handles == 0) {
		int iResult = 0;
		WSADATA wsaData;

		// Make sure Winsock is initialized
		iResult = WSAStartup(MAKEWORD(2,2), &wsaData);

		if (iResult != 0) {
			return HAGGLE_WSA_ERROR;
		}
	}
#endif
	hh = (struct haggle_handle *)malloc(sizeof(struct haggle_handle));

	if (!hh)
		return HAGGLE_ALLOC_ERROR;

	memset(hh, 0, sizeof(struct haggle_handle));

	hh->num_handlers = 0;
	INIT_LIST(&hh->l);

	hh->sock = socket(AF_ADDRESS_FAMILY, SOCK_DGRAM, 0);

	if (hh->sock == INVALID_SOCKET) {
		free(hh);
		haggle_set_socket_error();
		LIBHAGGLE_ERR("Could not open haggle socket\n");
		return HAGGLE_SOCKET_ERROR;
	}
	
#if defined(OS_WINDOWS)
        hh->signal = CreateEvent(NULL, TRUE, FALSE, NULL);
#elif defined(OS_UNIX)
        ret = pipe(hh->signal);
        
        if (ret != 0) {
		free(hh);
		return HAGGLE_ERROR;
        }
#endif
	// Generate the application's SHA1 hash
	SHA1_Init(&ctxt);

	SHA1_Update(&ctxt, (unsigned char *)name, strlen(name));
	
	SHA1_Final((unsigned char *)hh->id, &ctxt);

	// Compute the base64 representation
	base64_encode(hh->id, ID_LEN, hh->id_base64, ID_BASE64_LEN);

#ifdef USE_UNIX_APPLICATION_SOCKET
	haggle_addr.sun_family = AF_UNIX;
	strcpy(haggle_addr.sun_path, HAGGLE_UNIX_SOCK_PATH);
#else
	haggle_addr.sin_family = AF_INET;
	haggle_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	haggle_addr.sin_port = htons(HAGGLE_SERVICE_DEFAULT_PORT);
#endif

	dobj = haggle_dataobject_new();

	if (!dobj) {
		CLOSE_SOCKET(hh->sock);
		free(hh);
                //LIBHAGGLE_ERR("could not allocate data object\n");
		return HAGGLE_ALLOC_ERROR;
	}

	haggle_dataobject_add_attribute(dobj, HAGGLE_ATTR_CONTROL_NAME, HAGGLE_ATTR_REGISTRATION_REQUEST_VALUE);
	haggle_dataobject_add_attribute(dobj, HAGGLE_ATTR_APPLICATION_NAME_NAME, name);

	ret = haggle_ipc_send_control_dataobject(hh, dobj, &dobj_reply, 10000);

	haggle_dataobject_free(dobj);

	if (ret <= 0) {
		CLOSE_SOCKET(hh->sock);
		free(hh);
		LIBHAGGLE_ERR("Could not register with haggle daemon\n");
		if (ret < 0)
			return ret;
		else
			return HAGGLE_INTERNAL_ERROR;
	}
	
	attr = haggle_dataobject_get_attribute_by_name_value(dobj_reply, HAGGLE_ATTR_CONTROL_NAME, 
							     HAGGLE_ATTR_REGISTRATION_REPLY_VALUE);
	
	if (!attr) {
		attr = haggle_dataobject_get_attribute_by_name_value(dobj_reply, HAGGLE_ATTR_CONTROL_NAME, 
									HAGGLE_ATTR_REGISTRATION_REPLY_REGISTERED_VALUE);
		if (!attr) {
			LIBHAGGLE_ERR("Bad registration reply\n");

			CLOSE_SOCKET(hh->sock);
			free(hh);
			haggle_dataobject_free(dobj_reply);
			return HAGGLE_REGISTRATION_ERROR;
		}
		
		if (ignore_busy_signal == 0) {
			LIBHAGGLE_ERR("Unable to get haggle handle - already registered!\n");
			
			CLOSE_SOCKET(hh->sock);
			free(hh);
			haggle_dataobject_free(dobj_reply);
			return HAGGLE_BUSY_ERROR;
		}
	}
	
	attr = haggle_dataobject_get_attribute_by_name(dobj_reply, HAGGLE_ATTR_HAGGLE_DIRECTORY_NAME);
	
	if (!attr) {
		LIBHAGGLE_ERR("Bad registration reply\n");
		
		CLOSE_SOCKET(hh->sock);
		free(hh);
		haggle_dataobject_free(dobj_reply);
		return HAGGLE_REGISTRATION_ERROR;
	}
	
	if (!haggle_directory) {
		haggle_directory = malloc(strlen(haggle_attribute_get_value(attr)) + 1);
		if (!haggle_directory) {
			LIBHAGGLE_ERR("Unable to allocate memory\n");
			
			CLOSE_SOCKET(hh->sock);
			free(hh);
			haggle_dataobject_free(dobj_reply);
			return HAGGLE_INTERNAL_ERROR;
		}
		strcpy(haggle_directory, haggle_attribute_get_value(attr));
	}
	
	if (ignore_busy_signal == 0) {
		attr = haggle_dataobject_get_attribute_by_name(dobj_reply, HAGGLE_ATTR_SESSION_ID_NAME);

		if (!attr) {
			LIBHAGGLE_ERR("Bad registration reply\n");

			CLOSE_SOCKET(hh->sock);
			free(hh);
			haggle_dataobject_free(dobj_reply);
			return HAGGLE_REGISTRATION_ERROR;
		}

		hh->session_id = atoi(attr->value);
	}

	num_handles++;

	list_add(&hh->l, &haggle_handles);

	haggle_dataobject_free(dobj_reply);

	//if (shutdown_handler) {
	//	haggle_ipc_register_event_interest(h->id, LIBHAGGLE_EVENT_HAGGLE_SHUTDOWN, shutdown_handler);
	//}
	
	*handle = hh;
	return HAGGLE_NO_ERROR;
}

int haggle_handle_get(const char *name, haggle_handle_t *handle)
{
	return haggle_handle_get_internal(name, handle, 0);
}

void haggle_handle_free(haggle_handle_t hh)
{
	struct dataobject *dobj;
	int ret;

	if (!hh) {
		return;
	}
	
	if (hh->event_loop_running)
		haggle_event_loop_stop(hh);

	/* Notify daemon that we are no longer here */
	dobj = haggle_dataobject_new();

	if (!dobj) {
		CLOSE_SOCKET(hh->sock);
		free(hh);
		return;
	}
			
	haggle_dataobject_add_attribute(dobj, HAGGLE_ATTR_CONTROL_NAME, HAGGLE_ATTR_DEREGISTRATION_NOTICE_VALUE);

	ret = haggle_ipc_send_control_dataobject(hh, dobj, NULL, IO_NO_REPLY);

	if (ret < 0) {
		LIBHAGGLE_ERR("failed to send deregistration\n");
	}
		
	haggle_dataobject_free(dobj);

	num_handles--;
	list_detach(&hh->l);
	CLOSE_SOCKET(hh->sock);
#if defined(OS_WINDOWS)
        CloseHandle(hh->signal);
#elif defined(OS_UNIX)
        close(hh->signal[0]);
        close(hh->signal[1]);
#endif
	free(hh);
	hh = NULL;
#ifdef DEBUG
	haggle_dataobject_leak_report_print();
#endif


#if defined(WIN32) || defined(WINCE)
	if (num_handles == 0) {
		WSACleanup();
	}
#endif
}

int haggle_unregister(const char *name)
{
	haggle_handle_t	hh = NULL;
	
	haggle_handle_get_internal(name, &hh, 1);

	if (hh) {
		haggle_handle_free(hh);
		return HAGGLE_NO_ERROR;
	} 
        
        return HAGGLE_INTERNAL_ERROR;
}

int haggle_handle_get_session_id(haggle_handle_t hh)
{
	return hh->session_id;
}

typedef enum {
	INTEREST_OP_ADD,
	INTEREST_OP_REMOVE
} interest_op_t;


static int haggle_ipc_add_or_remove_interests(haggle_handle_t hh, interest_op_t op, haggle_attrlist_t *al)
{
	if (!hh || !al)
		return HAGGLE_PARAM_ERROR;

	if (op == INTEREST_OP_ADD)
		haggle_attributelist_add_attribute(al, haggle_attribute_new(HAGGLE_ATTR_CONTROL_NAME, HAGGLE_ATTR_ADD_INTEREST_VALUE));
	else
		haggle_attributelist_add_attribute(al, haggle_attribute_new(HAGGLE_ATTR_CONTROL_NAME, HAGGLE_ATTR_REMOVE_INTEREST_VALUE));
        

	return haggle_ipc_generate_and_send_control_dataobject(hh, al);
}

int haggle_ipc_add_application_interests(haggle_handle_t hh, const haggle_attrlist_t *al)
{
	
	return haggle_ipc_add_or_remove_interests(hh, INTEREST_OP_ADD, haggle_attributelist_copy(al));
}

int haggle_ipc_remove_application_interests(haggle_handle_t hh, const haggle_attrlist_t *al)
{
	return haggle_ipc_add_or_remove_interests(hh, INTEREST_OP_REMOVE, haggle_attributelist_copy(al));
}

int haggle_ipc_add_or_remove_application_interest_weighted(haggle_handle_t hh, interest_op_t op, const char *name, const char *value, const unsigned long weight)
{
	haggle_attrlist_t *al;
	
	al = haggle_attributelist_new_from_attribute(haggle_attribute_new_weighted(name, value, weight));

	if (!al)
		return HAGGLE_INTERNAL_ERROR;

	if (op == INTEREST_OP_ADD)
		haggle_attributelist_add_attribute(al, haggle_attribute_new(HAGGLE_ATTR_CONTROL_NAME, HAGGLE_ATTR_ADD_INTEREST_VALUE));
	else
		haggle_attributelist_add_attribute(al, haggle_attribute_new(HAGGLE_ATTR_CONTROL_NAME, HAGGLE_ATTR_REMOVE_INTEREST_VALUE));
        

	return haggle_ipc_generate_and_send_control_dataobject(hh, al);
}

int haggle_ipc_add_application_interest_weighted(haggle_handle_t hh, const char *name, const char *value, const unsigned long weight)
{
	return haggle_ipc_add_or_remove_application_interest_weighted(hh, INTEREST_OP_ADD, name, value, weight);
}

int haggle_ipc_add_application_interest(haggle_handle_t hh, const char *name, const char *value)
{
	return haggle_ipc_add_or_remove_application_interest_weighted(hh, INTEREST_OP_ADD, name, value, 1);
}


int haggle_ipc_remove_application_interest(haggle_handle_t hh, const char *name, const char *value)
{
	return haggle_ipc_add_or_remove_application_interest_weighted(hh, INTEREST_OP_REMOVE, name, value, 1);
}


int haggle_ipc_get_application_interests_async(haggle_handle_t hh)
{
	return haggle_ipc_generate_and_send_control_dataobject(hh, haggle_attributelist_new_from_attribute(haggle_attribute_new(HAGGLE_ATTR_CONTROL_NAME, HAGGLE_ATTR_GET_INTERESTS_VALUE)));
}

HAGGLE_API int haggle_ipc_get_data_objects_async(haggle_handle_t hh)
{
	return haggle_ipc_generate_and_send_control_dataobject(hh, haggle_attributelist_new_from_attribute(haggle_attribute_new(HAGGLE_ATTR_CONTROL_NAME, HAGGLE_ATTR_GET_DATAOBJECTS_VALUE)));
}

int haggle_ipc_shutdown(haggle_handle_t hh)
{
	return haggle_ipc_generate_and_send_control_dataobject(hh, haggle_attributelist_new_from_attribute(haggle_attribute_new(HAGGLE_ATTR_CONTROL_NAME, HAGGLE_ATTR_SHUTDOWN_VALUE)));
}

int STDCALL haggle_ipc_register_event_interest(haggle_handle_t hh, const int eventId, haggle_event_handler_t handler)
{
	return haggle_ipc_register_event_interest_with_arg(hh, eventId, handler, NULL);
}

int STDCALL haggle_ipc_register_event_interest_with_arg(haggle_handle_t hh, const int eventId, haggle_event_handler_t handler, void *arg)
{
	struct dataobject *dobj;
	int ret;

	if (eventId < 0 || eventId >= _LIBHAGGLE_NUM_EVENTS || handler == NULL)
		return HAGGLE_PARAM_ERROR;
		
	if (!hh)
		return HAGGLE_PARAM_ERROR;

	hh->handlers[eventId].arg = arg;
	hh->handlers[eventId].handler = handler;
	hh->num_handlers++;
	
	dobj = haggle_dataobject_new();
	
	if (!dobj)
		return HAGGLE_ALLOC_ERROR;
	
	haggle_dataobject_add_attribute(dobj, HAGGLE_ATTR_CONTROL_NAME, HAGGLE_ATTR_REGISTER_EVENT_INTEREST_VALUE);
	haggle_dataobject_add_attribute(dobj, HAGGLE_ATTR_EVENT_INTEREST_NAME, intTostr(eventId));
	
	ret = haggle_ipc_send_control_dataobject(hh, dobj, NULL, IO_NO_REPLY);
	
	haggle_dataobject_free(dobj);

	return ret;
}

/* This send function is currently internal, so we need to delete the
 * data object after a send */
int haggle_ipc_send_control_dataobject(haggle_handle_t hh, haggle_dobj_t *dobj, 
                                       haggle_dobj_t **dobj_reply, long msecs_timeout)
{
	if (!hh) {
		libhaggle_errno = LIBHAGGLE_ERR_BAD_HANDLE;
		return HAGGLE_PARAM_ERROR;
	}
	
	/* Control data objects are non-persistent, i.e., should not
	 * be added to the data store */
        haggle_dataobject_unset_flags(dobj, DATAOBJECT_FLAG_PERSISTENT);

	/* Make sure we always add the application id to control messages */
	haggle_dataobject_add_attribute(dobj, HAGGLE_ATTR_APPLICATION_ID_NAME, hh->id_base64);

	return haggle_ipc_send_dataobject(hh, dobj, dobj_reply, msecs_timeout);
}

int haggle_ipc_publish_dataobject(haggle_handle_t hh, haggle_dobj_t *dobj)
{
	if (!hh) {
		libhaggle_errno = LIBHAGGLE_ERR_BAD_HANDLE;
		LIBHAGGLE_DBG("Bad handle\n");
		return HAGGLE_PARAM_ERROR;
	}

	return haggle_ipc_send_dataobject(hh, dobj, NULL, IO_NO_REPLY);
}


/*
	Generate a data object containing the attributes in the given list. The list
	will be automatically freed, so the caller must make a copy if it wants to keep
	the list.
*/
int haggle_ipc_generate_and_send_control_dataobject(haggle_handle_t hh, haggle_attrlist_t *al)
{
	struct dataobject *dobj;
	list_t *pos;
	int ret;

	if (!al || !hh)
		return HAGGLE_PARAM_ERROR;

	dobj = haggle_dataobject_new();
	
	if (!dobj)
		return HAGGLE_ALLOC_ERROR;
	
	haggle_dataobject_unset_flags(dobj, DATAOBJECT_FLAG_PERSISTENT);
	
	list_for_each(pos, &al->attributes) {
		haggle_attr_t *a = (haggle_attr_t *)pos;
		haggle_dataobject_add_attribute_weighted(dobj, a->name, a->value, a->weight);
	}

	ret = haggle_ipc_send_control_dataobject(hh, dobj, NULL, IO_NO_REPLY);
	
	haggle_dataobject_free(dobj);

	haggle_attributelist_free(al);

	return ret;
}

int haggle_ipc_send_dataobject(struct haggle_handle *hh, haggle_dobj_t *dobj, 
			       haggle_dobj_t **dobj_reply, long msecs_timeout)
{
	int ret = 0;
	struct dataobject *dobj_recv;
	char *data;
        size_t datalen;

	if (!dobj || !hh)
		return HAGGLE_PARAM_ERROR;

	/* Generate raw xml from dataobject */
        ret = haggle_dataobject_get_raw_alloc(dobj, &data, &datalen);
	
	if (!ret || datalen == 0) {
		return HAGGLE_ALLOC_ERROR;
	}
        
	ret = sendto(hh->sock, data, strlen(data), 0, (struct sockaddr *)&haggle_addr, sizeof(haggle_addr));

	free(data);

	if (ret < 0) {
#if defined(WIN32) || defined(WINCE)
		LIBHAGGLE_DBG("send failure %d\n", WSAGetLastError());
#else
		LIBHAGGLE_DBG("send failure %s\n", strerror(errno));
#endif
		return HAGGLE_SOCKET_ERROR;
	}

	if (msecs_timeout != IO_NO_REPLY && dobj_reply != NULL) {
		struct timeval timeout;
		struct timeval *to_ptr = NULL;
		struct sockaddr_in peer_addr;
		socklen_t addrlen = sizeof(peer_addr);
                fd_set readfds;

		if (msecs_timeout == IO_REPLY_BLOCK) {
			/* We block indefinately */
			to_ptr = NULL;
		} else if (msecs_timeout == IO_REPLY_NON_BLOCK) {
			/* Non-blocking mode, return immediately */
			timeout.tv_sec = 0;
			timeout.tv_usec = 0;
			to_ptr = &timeout;
		} else if (msecs_timeout > 0) {
			/* We block with timeout */
			timeout.tv_sec = msecs_timeout / 1000;
			timeout.tv_usec = (msecs_timeout * 1000) % 1000000;
			to_ptr = &timeout;
		} else {
			/* Bad value */
			return HAGGLE_INTERNAL_ERROR;
		}

                FD_ZERO(&readfds);
                FD_SET(hh->sock, &readfds);

                ret = select(hh->sock + 1, &readfds, NULL, NULL, to_ptr);

		if (ret < 0) {
			LIBHAGGLE_ERR("select error\n");
			return HAGGLE_SOCKET_ERROR;
		} else if (ret == 0) {
			LIBHAGGLE_DBG("Receive timeout!\n");
			return HAGGLE_NO_ERROR;
		} 
                             
                ret = recvfrom(hh->sock, buffer, BUFLEN, 0, (struct sockaddr *)&peer_addr, &addrlen);
                
                if (ret == SOCKET_ERROR) {
                        LIBHAGGLE_DBG("Receive error = %d\n", ret);
                        haggle_set_socket_error();
                        return HAGGLE_SOCKET_ERROR;
                }
               
                dobj_recv = haggle_dataobject_new_from_raw(buffer, ret);
                
                if (dobj_recv == NULL) {
                        dobj_reply = NULL;
                        haggle_set_socket_error();
                        return HAGGLE_ALLOC_ERROR;
                }
                *dobj_reply = dobj_recv;
        }
        return ret;
}

int haggle_ipc_delete_data_object_by_id(haggle_handle_t hh, const dataobject_id_t id)
{
	struct dataobject *dobj;
	int ret;

	if (!hh)
		return HAGGLE_PARAM_ERROR;

	dobj = haggle_dataobject_new();
	
	if (dobj) {
		char base64str[BASE64_LENGTH(sizeof(dataobject_id_t)) + 1];
		memset(base64str, '\0', sizeof(base64str));

		base64_encode((char *)id, sizeof(dataobject_id_t), base64str, sizeof(base64str));

		haggle_dataobject_add_attribute(dobj, HAGGLE_ATTR_CONTROL_NAME, HAGGLE_ATTR_DELETE_DATAOBJECT_VALUE);
		haggle_dataobject_add_attribute(dobj, HAGGLE_ATTR_DATAOBJECT_ID_NAME, base64str);

		ret = haggle_ipc_send_control_dataobject(hh, dobj, NULL, IO_NO_REPLY);

		haggle_dataobject_free(dobj);
		
		return ret;
	}

	return HAGGLE_ALLOC_ERROR;
}


int haggle_ipc_delete_data_object(haggle_handle_t hh, const struct dataobject *dobj)
{
	dataobject_id_t id;

	// Calculate the id from the data object
	haggle_dataobject_calculate_id(dobj, &id);

	return haggle_ipc_delete_data_object_by_id(hh, id);
}

int haggle_event_loop_is_running(haggle_handle_t hh)
{
        return (hh ? hh->event_loop_running : HAGGLE_HANDLE_ERROR);
}

int haggle_event_loop_stop(haggle_handle_t hh)
{
	LIBHAGGLE_DBG("Stopping event loop\n");

	if (!hh) {
		LIBHAGGLE_DBG("Bad haggle handle\n");
		libhaggle_errno = LIBHAGGLE_ERR_BAD_HANDLE;
		return HAGGLE_PARAM_ERROR;
	}
	if (!hh->event_loop_running) {
		LIBHAGGLE_DBG("Event loop not running\n");
		return HAGGLE_EVENT_LOOP_ERROR;
	}
                
        event_loop_signal_raise(hh);

#if defined(OS_WINDOWS)
        if (hh->th) {
                WaitForSingleObject(hh->th, INFINITE);
                /* Is this really necessary here or will the handle be closed by the event loop? */
                CloseHandle(hh->th);
                hh->th = NULL;
        }
#elif defined(OS_UNIX)
        if (hh->th) {
                LIBHAGGLE_DBG("Joining with event loop thread...\n");
                pthread_join(hh->th, NULL);
                hh->th = 0;
        }
#endif
	LIBHAGGLE_DBG("Event loop successfully stopped\n");

	hh->event_loop_running = 0;

	return HAGGLE_NO_ERROR;
}


start_ret_t haggle_event_loop(void *arg)
{
	struct haggle_handle *hh = (struct haggle_handle *)arg;
	static char eventbuf[BUFLEN];
	int ret;

        if (hh->start)
                hh->start(hh->arg);

	while (hh->event_loop_running) {
		struct dataobject *dobj;
		struct attribute *attr;
		int /*session_id, */event_type;
		
		LIBHAGGLE_DBG("Event loop running, waiting for data object...\n");

                ret = wait_for_event(hh, NULL);

		if (ret == EVENT_LOOP_ERROR) {
			LIBHAGGLE_ERR("Haggle event loop wait error!\n");
                        // Break or try again?
			break;
		} else if (ret == EVENT_LOOP_TIMEOUT) {
			LIBHAGGLE_DBG("Event loop timeout!\n");
			break;
                } else if (ret == EVENT_LOOP_SHOULD_EXIT) {
			LIBHAGGLE_DBG("Event loop should exit!\n");
                        event_loop_signal_lower(hh);
			break;
                } else if (ret == EVENT_LOOP_SOCKET_READABLE)  {
                       
                        ret = recv(hh->sock, eventbuf, BUFLEN, 0);
                        
                        if (ret == SOCKET_ERROR) {
                                LIBHAGGLE_ERR("Haggle event loop recv() error!\n");
                                continue;
                        }
                        
                        dobj = haggle_dataobject_new_from_raw(eventbuf, ret);
                        
                        if (!dobj) {
                                LIBHAGGLE_ERR("Haggle event loop ERROR: could not create data object\n");
                                continue;
                        }
                        
                        /* Check control attributes and do parsing, and callbacks */
                        /*
                          attr = haggle_dataobject_get_attribute_by_name(dobj, HAGGLE_ATTR_SESSION_ID_NAME);
                          
                          if (!attr) {
                          LIBHAGGLE_ERR("Dataobject from Haggle daemon has no application id!\n");
                          haggle_dataobject_free(dobj);
                          continue;			
                          }
                          
                          session_id = atoi(haggle_attribute_get_value(attr));
                        */
			
			attr = haggle_dataobject_get_attribute_by_name(dobj, HAGGLE_ATTR_CONTROL_NAME);
                        
                        if (!attr) {
                                LIBHAGGLE_ERR("Data object contains no control attribute!\n");
                                haggle_dataobject_free(dobj);
                                continue;			
                        }

                        haggle_dataobject_remove_attribute(dobj, attr);

			haggle_attribute_free(attr);

                        attr = haggle_dataobject_get_attribute_by_name(dobj, HAGGLE_ATTR_EVENT_TYPE_NAME);
                        
                        if (!attr) {
                                LIBHAGGLE_ERR("Data object contains no recognized event!\n");
                                haggle_dataobject_free(dobj);
                                continue;			
                        }
                        
                        event_type = atoi(haggle_attribute_get_value(attr));
                        
			// Remove the event type attribute
			haggle_dataobject_remove_attribute(dobj, attr);
                        
			haggle_attribute_free(attr);

                        if (hh->handlers[event_type].handler) {
                                hh->handlers[event_type].handler(dobj, hh->handlers[event_type].arg);
                        }
                }
	}
	
	hh->event_loop_running = 0;
        
        if (hh->stop)
                hh->stop(hh->arg);

#if defined(WIN32) || defined(WINCE)
	return 0;
#else
	return NULL;
#endif
}


/* A blocking event loop. Application does threading if necessary. */
int haggle_event_loop_run(haggle_handle_t hh)
{
#if defined(OS_LINUX) || defined(OS_MACOSX)
	/* pthread_attr_t attr; */
#endif
	if (!hh) {
		libhaggle_errno = LIBHAGGLE_ERR_BAD_HANDLE;
		return HAGGLE_PARAM_ERROR;
	}

	if (hh->num_handlers == 0)
		return HAGGLE_EVENT_HANDLER_ERROR;

	hh->event_loop_running = 1;

	haggle_event_loop(hh);

	hh->event_loop_running = 0;

	return HAGGLE_NO_ERROR;
}

/* An asynchronous event loop. The application does not have to care
 * about threading */
int haggle_event_loop_run_async(haggle_handle_t hh)
{
	int ret = 0;

	if (!hh) {
		libhaggle_errno = LIBHAGGLE_ERR_BAD_HANDLE;
		return HAGGLE_PARAM_ERROR;
	}

	if (hh->num_handlers == 0)
		return HAGGLE_EVENT_HANDLER_ERROR;

	if (hh->event_loop_running)
		return HAGGLE_EVENT_LOOP_ERROR;
	
	hh->event_loop_running = 1;        

#if defined(WIN32) || defined(WINCE)
	hh->th = CreateThread(NULL,  0, haggle_event_loop, (void *)hh, 0, 0);

	if (hh->th == NULL)
		return HAGGLE_INTERNAL_ERROR;
#else
	ret = pthread_create(&hh->th, NULL, haggle_event_loop, (void *)hh);

	if (ret != 0)
		return HAGGLE_INTERNAL_ERROR;
#endif
	return HAGGLE_NO_ERROR;
}

int haggle_event_loop_register_callbacks(haggle_handle_t hh, haggle_event_loop_start_t start, haggle_event_loop_stop_t stop, void *arg)
{
        if (!hh || (!start && !stop))
                return -1;
        
        if (hh->start) {
                LIBHAGGLE_ERR("Start callback already set\n");
                return -1;
        }
        if (hh->stop) {
                LIBHAGGLE_ERR("Stop callback already set\n");
                return -1;
        }

        hh->start = start;
        hh->stop = stop;
        hh->arg = arg;

        return HAGGLE_NO_ERROR;
}
