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

#include "utils.h"

#if defined(OS_MACOSX) || defined(OS_LINUX)
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>   
#include <netinet/in.h>
#include <fcntl.h>
#ifndef OS_MACOSX_IPHONE
#include <net/if_arp.h>
#endif /* OS_MACOSX_IPHONE */

#define ERRNO errno
#endif

#if defined(OS_MACOSX) 
#include <stdlib.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>


#ifndef OS_MACOSX_IPHONE
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/if_ether.h>
#endif /* OS_MACOSX_IPHONE */


#elif defined(OS_LINUX)
#include <linux/if.h>
#include <errno.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>


#if defined(WIN32) || defined(WINCE)
#include <windows.h>
#include <mmsystem.h>
#include <ws2tcpip.h>
#include <WinError.h>
#include <iphlpapi.h>
#define ERRNO WSAGetLastError()
#define EWOULDBLOCK WSAEWOULDBLOCK

/*
* Number of micro-seconds between the beginning of the Windows epoch
* (Jan. 1, 1601) and the Unix epoch (Jan. 1, 1970).
*
* This assumes all Win32 compilers have 64-bit support.
*/
// gettimeofday and strsep is from tcpdump
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS) || defined(__WATCOMC__)
#define DELTA_EPOCH_IN_USEC  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_USEC  11644473600000000ULL
#endif
typedef DWORD64 u_int64_t;
/***********************************************************************
 *
 * Borrowed from WINE sources!! (http://www.winehq.com)
 * Converts a Win32 FILETIME structure to a UNIX time_t value
 */
#include <time.h>
/***********************************************************************
 *           DOSFS_FileTimeToUnixTime
 *
 * Convert a FILETIME format to Unix time.
 * If not NULL, 'remainder' contains the fractional part of the filetime,
 * in the range of [0..9999999] (even if time_t is negative).
 */
time_t FileTimeToUnixTime(const FILETIME *filetime, DWORD *remainder)
{
    /* Read the comment in the function DOSFS_UnixTimeToFileTime. */
#if USE_LONG_LONG
    long long int t = filetime->dwHighDateTime;
    t <<= 32;
    t += (UINT32)filetime->dwLowDateTime;
    t -= 116444736000000000LL;
    if (t < 0)
    {
	if (remainder) *remainder = 9999999 - (-t - 1) % 10000000;
	return -1 - ((-t - 1) / 10000000);
    }
    else
    {
	if (remainder) *remainder = t % 10000000;
	return t / 10000000;
    }

#else  /* ISO version */

    UINT32 a0;			/* 16 bit, low    bits */
    UINT32 a1;			/* 16 bit, medium bits */
    UINT32 a2;			/* 32 bit, high   bits */
    UINT32 r;			/* remainder of division */
    unsigned int carry;		/* carry bit for subtraction */
    int negative;		/* whether a represents a negative value */

    /* Copy the time values to a2/a1/a0 */
    a2 =  (UINT32)filetime->dwHighDateTime;
    a1 = ((UINT32)filetime->dwLowDateTime ) >> 16;
    a0 = ((UINT32)filetime->dwLowDateTime ) & 0xffff;

    /* Subtract the time difference */
    if (a0 >= 32768           ) a0 -=             32768        , carry = 0;
    else                        a0 += (1 << 16) - 32768        , carry = 1;

    if (a1 >= 54590    + carry) a1 -=             54590 + carry, carry = 0;
    else                        a1 += (1 << 16) - 54590 - carry, carry = 1;

    a2 -= 27111902 + carry;
    
    /* If a is negative, replace a by (-1-a) */
    negative = (a2 >= ((UINT32)1) << 31);
    if (negative)
    {
	/* Set a to -a - 1 (a is a2/a1/a0) */
	a0 = 0xffff - a0;
	a1 = 0xffff - a1;
	a2 = ~a2;
    }

    /* Divide a by 10000000 (a = a2/a1/a0), put the rest into r.
       Split the divisor into 10000 * 1000 which are both less than 0xffff. */
    a1 += (a2 % 10000) << 16;
    a2 /=       10000;
    a0 += (a1 % 10000) << 16;
    a1 /=       10000;
    r   =  a0 % 10000;
    a0 /=       10000;

    a1 += (a2 % 1000) << 16;
    a2 /=       1000;
    a0 += (a1 % 1000) << 16;
    a1 /=       1000;
    r  += (a0 % 1000) * 10000;
    a0 /=       1000;

    /* If a was negative, replace a by (-1-a) and r by (9999999 - r) */
    if (negative)
    {
	/* Set a to -a - 1 (a is a2/a1/a0) */
	a0 = 0xffff - a0;
	a1 = 0xffff - a1;
	a2 = ~a2;

        r  = 9999999 - r;
    }

    if (remainder) *remainder = r;

    /* Do not replace this by << 32, it gives a compiler warning and it does
       not work. */
    return ((((time_t)a2) << 16) << 16) + (a1 << 16) + a0;
#endif
}

/* A wrapper for gettimeofday so that it can be used on Windows platforms. */
/*
	NOTE:
	
	It has been observed that at times the value returned from this function
	is unexpectedly high. (tv->usec >= 2^32)
*/
int gettimeofday(struct timeval *tv, void *tz)
{
	time_t unixtime;
	DWORD remainder; // In microseconds
#ifdef WINCE
	/* In Windows CE, GetSystemTime() returns a time accurate only to the second.
	For higher performance timers we need to use something better. */
	
	// This is true when the base time is not correctly calculated. It will be
	// calculated the next time gettimeofday() is called.
	static int shouldCheckTime = 1;
	// This holds the base time, i.e. the estimated time of boot. This value plus
	// the value the high-performance timer/GetTickCount() provides gives us the
	// right time.
	static struct timeval baseTime;

	if (!tv) 
		return (-1);

	/* The hardware does not implement a high performance timer.
	Note, the time will wrap around after 49.7 days. */
	remainder = GetTickCount();
	unixtime = remainder / 1000;
	remainder = (remainder % 1000) * 1000;
	
	// Should we determine the base time?
	if (shouldCheckTime) {
		FILETIME  ft;
		SYSTEMTIME st;
		time_t nixtime;
		DWORD rem;
		
		// Find the system time:
		GetSystemTime(&st);
		// Convert it into "file time":
		SystemTimeToFileTime(&st, &ft);
		
		// Convert "file time" into readable values:
		nixtime = FileTimeToUnixTime(&ft, &rem);
		
		// Set up the base time:
		baseTime.tv_sec = nixtime - tv->tv_sec;
		baseTime.tv_usec = rem - tv->tv_usec;
		
		// normalize the base time:
		while (baseTime.tv_usec < 0) {
			baseTime.tv_sec--;
			baseTime.tv_usec += 1000000;
		}
		while (baseTime.tv_usec >= 1000000) {
			baseTime.tv_sec++;
			baseTime.tv_usec -= 1000000;
		}
		
		// Set this so we don't do all this over again:
		shouldCheckTime = 0;
	}
	
	tv->tv_sec  = (long) unixtime + baseTime.tv_sec;
	tv->tv_usec = (long) remainder + baseTime.tv_usec;
#else
	FILETIME  ft;
	SYSTEMTIME st;

	if (!tv) 
		return (-1);

	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);

	unixtime = FileTimeToUnixTime(&ft, &remainder);

	tv->tv_sec  = (long) unixtime;
	tv->tv_usec = (long) remainder;
#endif
	
	// normalize the time:
	while (tv->tv_usec < 0) {
		tv->tv_sec--;
		tv->tv_usec += 1000000;
	}
	while (tv->tv_usec >= 1000000) {
		tv->tv_sec++;
		tv->tv_usec -= 1000000;
	}

	return 0;
}
char *strsep(char **stringp, const char *delim)
{
	register char *s;
	register const char *spanp;
	register int c, sc;
	char *tok;

	if ((s = *stringp) == NULL)
		return (NULL);
	for (tok = s;;) {
		c = *s++;
		spanp = delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				*stringp = s;
				return (tok);
			}
		} while (sc != 0);
	}
	/* NOTREACHED */
}

#define ERR_MACRO(A) A, #A ": "

typedef struct
{
    long errVal;
    char *errDescr;
} error_table_element;

static error_table_element werrs[] = 
{
    {ERR_MACRO(WSABASEERR)"No error"},
    {ERR_MACRO(WSAEACCES)"Permission denied"},
    {ERR_MACRO(WSAEADDRINUSE)"Address already in use"},
    {ERR_MACRO(WSAEADDRNOTAVAIL)"Cannot assign requested address"},
    {ERR_MACRO(WSAEAFNOSUPPORT)"Address family not supported by protocol family"},
    {ERR_MACRO(WSAEALREADY)"Operation already in progress"},
    {ERR_MACRO(WSAEBADF)"Bad file descriptor"},
    {ERR_MACRO(WSAECONNABORTED)"Software caused connection abort"},
    {ERR_MACRO(WSAECONNREFUSED)"Connection refused"},
    {ERR_MACRO(WSAECONNRESET)"Connection reset by peer"},
    {ERR_MACRO(WSAEDESTADDRREQ)"Destination address required"},
    {ERR_MACRO(WSAEDQUOT)"Disc quota exceeded"},
    {ERR_MACRO(WSAEFAULT)"Bad address"},
    {ERR_MACRO(WSAEHOSTDOWN)"Host is down"},
    {ERR_MACRO(WSAEHOSTUNREACH)"No route to host"},
    {ERR_MACRO(WSAEINPROGRESS)"Operation now in progress"},
    {ERR_MACRO(WSAEINTR)"Interrupted function call"},
    {ERR_MACRO(WSAEINVAL)"Invalid argument"},
    {ERR_MACRO(WSAEISCONN)"Socket is already connected"},
    {ERR_MACRO(WSAELOOP)"Too many levels of symbolic links"},
    {ERR_MACRO(WSAEMFILE)"Too many open files"},
    {ERR_MACRO(WSAEMSGSIZE)"Message too long"},
    {ERR_MACRO(WSAENAMETOOLONG)"File name too long"},
    {ERR_MACRO(WSAENETDOWN)"Network is down"},
    {ERR_MACRO(WSAENETRESET)"Network dropped connection on reset"},
    {ERR_MACRO(WSAENETUNREACH)"Network is unreachable"},
    {ERR_MACRO(WSAENOBUFS)"No buffer space available"},
    {ERR_MACRO(WSAENOPROTOOPT)"Bad protocol option"},
    {ERR_MACRO(WSAENOTCONN)"Socket is not connected"},
    {ERR_MACRO(WSAENOTEMPTY)"Directory not empty"},
    {ERR_MACRO(WSAENOTSOCK)"Socket operation on non-socket"},
    {ERR_MACRO(WSAEOPNOTSUPP)"Operation not supported"},
    {ERR_MACRO(WSAEPFNOSUPPORT)"Protocol family not supported"},
    {ERR_MACRO(WSAEPROCLIM)"Too many processes"},
    {ERR_MACRO(WSAEPROTONOSUPPORT)"Protocol not supported"},
    {ERR_MACRO(WSAEPROTOTYPE)"Protocol wrong type for socket"},
    {ERR_MACRO(WSAEREMOTE)"Too many levels of remote in path"},
    {ERR_MACRO(WSAESHUTDOWN)"Cannot send after socket shutdown"},
    {ERR_MACRO(WSAESOCKTNOSUPPORT)"Socket type not supported"},
    {ERR_MACRO(WSAESTALE)"Stale NFS file handle"},
    {ERR_MACRO(WSAETIMEDOUT)"Connection timed out"},
    {ERR_MACRO(WSAETOOMANYREFS)"Too many references; can't splice"},
    {ERR_MACRO(WSAEUSERS)"Too many users"},
    {ERR_MACRO(WSAEWOULDBLOCK)"Resource temporarily unavailable"},
    {ERR_MACRO(WSAHOST_NOT_FOUND)"Host not found"},
    {ERR_MACRO(WSANOTINITIALISED)"Successful WSAStartup() not yet performed"},
    {ERR_MACRO(WSANO_DATA)"Valid name, no data record of requested type"},
    {ERR_MACRO(WSANO_RECOVERY)"This is a non-recoverable error"},
    {ERR_MACRO(WSASYSNOTREADY)"Network subsystem is unavailable"},
    {ERR_MACRO(WSATRY_AGAIN)"Non-authoritative host not found"},
    {ERR_MACRO(WSAVERNOTSUPPORTED)"WINSOCKDLL version out of range"},
    {ERR_MACRO(WSABASEERR)"No Error"},
    {ERR_MACRO(WSAEINTR)"Interrupted system call"},
    {ERR_MACRO(WSAEBADF)"Bad file number"},
    {ERR_MACRO(WSAEACCES)"Permission denied"},
    {ERR_MACRO(WSAEFAULT)"Bad address"},
    {ERR_MACRO(WSAEINVAL)"Invalid argument"},
    {ERR_MACRO(WSAEMFILE)"Too many open files"},
    {ERR_MACRO(WSAEWOULDBLOCK)"Operation would block"},
    {ERR_MACRO(WSAEINPROGRESS)"Operation now in progress"},
    {ERR_MACRO(WSAEALREADY)"Operation already in progress"},
    {ERR_MACRO(WSAENOTSOCK)"Socket operation on non-socket"},
    {ERR_MACRO(WSAEDESTADDRREQ)"Destination address required"},
    {ERR_MACRO(WSAEMSGSIZE)"Message too long"},
    {ERR_MACRO(WSAEPROTOTYPE)"Protocol wrong type for socket"},
    {ERR_MACRO(WSAENOPROTOOPT)"Bad protocol option"},
    {ERR_MACRO(WSAEPROTONOSUPPORT)"Protocol not supported"},
    {ERR_MACRO(WSAESOCKTNOSUPPORT)"Socket type not supported"},
    {ERR_MACRO(WSAEOPNOTSUPP)"Operation not supported on socket"},
    {ERR_MACRO(WSAEPFNOSUPPORT)"Protocol family not supported"},
    {ERR_MACRO(WSAEAFNOSUPPORT)"Address family not supported by protocol family"},
    {ERR_MACRO(WSAEADDRINUSE)"Address already in use"},
    {ERR_MACRO(WSAEADDRNOTAVAIL)"Can't assign requested address"},
    {ERR_MACRO(WSAENETDOWN)"Network is down"},
    {ERR_MACRO(WSAENETUNREACH)"Network is unreachable"},
    {ERR_MACRO(WSAENETRESET)"Net dropped connection or reset"},
    {ERR_MACRO(WSAECONNABORTED)"Software caused connection abort"},
    {ERR_MACRO(WSAECONNRESET)"Connection reset by peer"},
    {ERR_MACRO(WSAENOBUFS)"No buffer space available"},
    {ERR_MACRO(WSAEISCONN)"Socket is already connected"},
    {ERR_MACRO(WSAENOTCONN)"Socket is not connected"},
    {ERR_MACRO(WSAESHUTDOWN)"Can't send after socket shutdown"},
    {ERR_MACRO(WSAETOOMANYREFS)"Too many references, can't splice"},
    {ERR_MACRO(WSAETIMEDOUT)"Connection timed out"},
    {ERR_MACRO(WSAECONNREFUSED)"Connection refused"},
    {ERR_MACRO(WSAELOOP)"Too many levels of symbolic links"},
    {ERR_MACRO(WSAENAMETOOLONG)"File name too long"},
    {ERR_MACRO(WSAEHOSTDOWN)"Host is down"},
    {ERR_MACRO(WSAEHOSTUNREACH)"No Route to Host"},
    {ERR_MACRO(WSAENOTEMPTY)"Directory not empty"},
    {ERR_MACRO(WSAEPROCLIM)"Too many processes"},
    {ERR_MACRO(WSAEUSERS)"Too many users"},
    {ERR_MACRO(WSAEDQUOT)"Disc Quota Exceeded"},
    {ERR_MACRO(WSAESTALE)"Stale NFS file handle"},
    {ERR_MACRO(WSASYSNOTREADY)"Network SubSystem is unavailable"},
    {ERR_MACRO(WSAVERNOTSUPPORTED)"WINSOCK DLL Version out of range"},
    {ERR_MACRO(WSANOTINITIALISED)"Successful WSASTARTUP not yet performed"},
    {ERR_MACRO(WSAEREMOTE)"Too many levels of remote in path"},
    {ERR_MACRO(WSAHOST_NOT_FOUND)"Host not found"},
    {ERR_MACRO(WSATRY_AGAIN)"Non-Authoritative Host not found"},
    {ERR_MACRO(WSANO_RECOVERY)"Non-Recoverable errors: FORMERR, REFUSED, NOTIMP"},
    {ERR_MACRO(WSANO_DATA)"Valid name, no data record of requested type"},
    {ERR_MACRO(WSANO_ADDRESS)"No address, look for MX record"},
    {ERR_MACRO(ERROR_MOD_NOT_FOUND)"The specified module could not be found"},
    {ERR_MACRO(WSASERVICE_NOT_FOUND)"Service not found"}
};
static char StrError_fallback_str[256];

char *StrError(long err)
{
	long i;

	for(i = 0; i < sizeof(werrs)/sizeof(error_table_element); i++)
		if (werrs[i].errVal == err)
			return werrs[i].errDescr;
	
	sprintf(StrError_fallback_str, "Unknown error (%ld)", err);

	return StrError_fallback_str;
}
#endif /* WIN32 || WINCE */

double absolute_time_double(double offset) {
	struct timeval absTime;
	double ret = 0.0;

	gettimeofday(&absTime, NULL);
	ret = (double)absTime.tv_sec + (double)absTime.tv_usec / 1000000 + offset;

	return ret;
}

/* Swaps byte order of a 6 byte address (Bluetooth, Ethernet) */

void swap_6bytes(void* to, const void *from)
{
	register unsigned char *d = (unsigned char *) to;
	register const unsigned char *s = (const unsigned char *) from;
	register int i;
	for (i = 0; i < 6; i++)
		d[i] = s[5-i];
}

/* Hmm, this function does not check the size of str, which is a
 * potential buffer overflow when sprintf null-terminates... -Erik */
void buf2str(const char* buf, char* str, int len)
{
	int i, n = 0;
	
	for (i=0; i<len; i++)
		n += sprintf(str+n, "%02x", buf[i] & 0xff);
	str[len*2] = '\0';
}

void str2buf(const char* str, char* buf, int len)
{
	int i;

	/* Why not scanf? Does this do something special? -Erik */
	for (i=0; i<len; i++) {
		buf[i] = (str[2*i]>='0' || str[2*i]<='9' ? str[2*i]-'0' : str[2*i]-'a'+10);
		buf[i] += (str[2*i+1]>='0' || str[2*i+1]<='9' ? str[2*i+1]-'0' : str[2*i+1]-'a'+10);
		buf[i] = buf[i] & 0xff; 
	}
}

#ifdef OS_LINUX

int get_ifaddr_from_name(const char ifname, struct in_addr *ipaddr)
{
	int fd;
	struct ifreq ifr;
	
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	
	if (!fd)
		return fd;

	/* I want to get an IPv4 IP address */
	ifr.ifr_addr.sa_family = AF_INET;
	
	/* I want IP address attached to "eth0" */
	strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
	
	ioctl(fd, SIOCGIFADDR, &ifr);
	
	close(fd);
	
	/* copy result */
	memcpy(ipaddr, ifr.ifr_addr.sa_data, sizeof(struct in_addr));
	
	return 0;
}
#endif

void milli_sleep(unsigned long milliseconds)
{
#if defined(WINCE)
	/*
		See comment above.

		Also: FIXME: how long is a clock tick on Win Mobile?
	*/
	Sleep(milliseconds + 1);
#elif defined(WIN32)
	/*
		The reason for this "extra" code is that while unix sleep functions
		are guaranteed to sleep for at least the given time, windows sleep
		functions are only guaranteed to sleep at least the given time minus
		one clock tick.
	*/
	TIMECAPS	tc;
	timeGetDevCaps(&tc, sizeof(tc));
	
	Sleep(milliseconds + (tc.wPeriodMin - (milliseconds % tc.wPeriodMin)));
#elif defined(OS_MACOSX) || defined(OS_LINUX)
	/*
		The reason for this extra code is so that the function will sleep for
		the given time, without being interrupted by signals, like what
		happens on windows.
	*/
	struct timespec	tts, ts;
	
	tts.tv_sec = milliseconds / 1000;
	tts.tv_nsec = (milliseconds % 1000) * 1000000;
	nanosleep(&tts, &ts);
#endif
}

static int trace_disabled = 0;

void trace_disable(int disable)
{
	trace_disabled = disable;
}

#define TRACE_BUFLEN 5000

struct timeval trace_base = {0,0};

void set_trace_timestamp_base(const struct timeval *tv)
{
	trace_base.tv_sec = tv->tv_sec;
	trace_base.tv_usec = tv->tv_usec;
}

static void timeval_sub_base(struct timeval *t)
{
	t->tv_sec -= trace_base.tv_sec;
	t->tv_usec -= trace_base.tv_usec;
	
	if (t->tv_usec < 0) {
		t->tv_sec--;
		t->tv_usec += 1000000;
	}
	if (t->tv_usec >= 1000000) {
		t->tv_sec++;
		t->tv_usec -= 1000000;
	}
}

void trace(const trace_type_t _type, const char *func, const char *fmt, ...)
{
	char buf[TRACE_BUFLEN];
	va_list args;
	int len;
	struct timeval now;
	FILE *stream = NULL;
	double now_d;

	if(trace_disabled)
		return;
	
	memset(buf, 0, TRACE_BUFLEN);
	
	switch (_type) {
		case TRACE_ERR: 
			stream = stderr;
			break;
		case TRACE_DEBUG:
		default:
			stream = stdout;
			break;
	}
	
	va_start(args, fmt);
	
#ifdef WINCE
	len = _vsnprintf(buf, TRACE_BUFLEN, fmt, args);
#else
	len = vsnprintf(buf, TRACE_BUFLEN, fmt, args);
#endif
	va_end(args);
	
	gettimeofday(&now, NULL);
	timeval_sub_base(&now);
	now_d = now.tv_usec;
	now_d /= 1000000;
	now_d += now.tv_sec;
	
	fprintf(stream, "%.3lf:%s: %s", now_d, func, buf);
	
	return;
}

/* From iputils ping.c */
unsigned short in_cksum(const unsigned short *addr, register int len, unsigned short csum)
{
	register int nleft = len;
	const unsigned short *w = addr;
	register unsigned short answer;
	register int sum = csum;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1)
		sum += htons(*(unsigned char *)w << 8);

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return (answer);
}

/* 
  Convenince function so that you can make more that one call to "inet_ntoa" on the same line,
  e.g., in a printf() statement.
*/
char *ip_to_str(struct in_addr addr)
{
	static char buf[16 * 4]; /* Use a circular buffer so that we
				 * can print up to four ip addresses
				 * on one line */
	static int index = 0;
	char *str;

	strcpy(&buf[index], inet_ntoa(addr));
	str = &buf[index];
	index += 16;
	index %= 64;
	return str;
}


char *eth_to_str(char *addr)
{
	static char buf[30];

	sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
		(unsigned char)addr[0], (unsigned char)addr[1],
		(unsigned char)addr[2], (unsigned char)addr[3],
		(unsigned char)addr[4], (unsigned char)addr[5]);

	return buf;
}


#if defined(WIN32) || defined(WINCE)
int get_peer_mac_address(const struct sockaddr *saddr, const char *ifname, char *mac, int maclen)
{
	DWORD res, index;
	MIB_IFROW mib;

	if (mac == NULL || saddr == NULL || maclen < 6)
		return -1;

	switch (saddr->sa_family) {
	case PF_INET:
		res = GetBestInterface((IPAddr)&((struct sockaddr_in *)saddr)->sin_addr, &index);
		break;
	case PF_INET6:
		res = GetBestInterface((IPAddr)&((struct sockaddr_in6 *)saddr)->sin6_addr, &index);
		break;
	default:
		return -1;
	}
	if (res != NO_ERROR)
		return 0;

	mib.dwIndex = index;

	res = GetIfEntry(&mib);

	if (res != NO_ERROR)
		return 0;

	if ((unsigned int)maclen < mib.dwPhysAddrLen)
		return -1;

	memcpy(mac, mib.bPhysAddr, mib.dwPhysAddrLen);

	return 1;
}
#elif defined(OS_LINUX)
#include <netinet/ip.h>
#include <netinet/in.h>
#include <linux/icmp.h>

int get_peer_mac_address(const struct sockaddr *saddr, const char *ifname, char *mac, int maclen)
{
        struct {
                struct cmsghdr cm;
                struct in_pktinfo ipi;
        } cmsg = { {sizeof(struct cmsghdr) + sizeof(struct in_pktinfo), SOL_IP, IP_PKTINFO},
                   {0, {0}, {0}}};
        int sock, icmp_sock;
        unsigned char outpack[0x10000];
        int addrlen = 0, datalen = 64;
        struct icmphdr *icmp;
        struct sockaddr *paddr = NULL;
        int ret = 0;
        struct arpreq areq;

	if (mac == NULL || saddr == NULL || maclen < 6)
		return -1;

	switch (saddr->sa_family) {
	case AF_INET:
                addrlen = sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
                addrlen = sizeof(struct sockaddr_in6);
		break;
	default:
                return -2;
	}

        /* First send a ICMP echo request probe to make sure the arp
         * table is updated */
	icmp_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

        if (icmp_sock == INVALID_SOCKET)
                return -3;
                                
        icmp = (struct icmphdr *)outpack;
	icmp->type = ICMP_ECHO;
	icmp->code = 0;
	icmp->checksum = 0;
	icmp->un.echo.sequence = htons(1);
	icmp->un.echo.id = 1;
               
	icmp->checksum = in_cksum((u_short *)icmp, datalen, 0);
        
        paddr = (struct sockaddr *)malloc(addrlen);

        if (!paddr) {
                close(icmp_sock);
                return -4;
        }

        memcpy(paddr, saddr, addrlen);

        do {
		struct iovec iov = {outpack, 0};
		struct msghdr m = { paddr, addrlen, &iov, 1, &cmsg, 0, 0 };
		m.msg_controllen = sizeof(cmsg);
		iov.iov_len = datalen;

		ret = sendmsg(icmp_sock, &m, 0);
	} while (0);
        
        free(paddr);

        close(icmp_sock);

        /* Wait for a short time so that the ARP cache has been populated */
        usleep(50000);

	sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock == INVALID_SOCKET)
		return -5;

        /* ICMP message sent, now check the arp table. */
        memset(&areq, 0, sizeof(struct arpreq));

        if (ifname)
                strcpy(areq.arp_dev, ifname);
        
        memcpy(&areq.arp_pa, saddr, addrlen);

	if (ioctl(sock, SIOCGARP, &areq) != -1) {
                memcpy(mac, areq.arp_ha.sa_data, 6);
                ret = 1;
	} else {
                fprintf(stderr, "ARP failed - %s\n", strerror(errno));
        }

        close(sock);

	return ret;
}
#elif defined(OS_MACOSX_IPHONE)
int get_peer_mac_address(const struct sockaddr *saddr, const char *ifname, char *mac, int maclen)
{

	return 0;
}
#elif defined(OS_MACOSX)

/*
	This code is mostly taken from arp.c on Darwin. It falls under the APSL license.
 
 */
#define ROUNDUP(a) \
((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
int get_peer_mac_address(const struct sockaddr *saddr, const char *ifname, char *mac, int maclen)
{	
	size_t needed;
	char *lim, *buf, *next;
	struct rt_msghdr *rtm;
	struct sockaddr_inarp *sin2;
	struct sockaddr_dl *sdl;
	int res = 0;
	unsigned long addr;
	
	switch (saddr->sa_family) {
		case AF_INET:
			memcpy(&addr, &((struct sockaddr_in*)saddr)->sin_addr.s_addr, sizeof(struct in_addr));
			break;
		case AF_INET6:
			// IPv6 not supported yet
			return -1;
			break;
		default:
			return -1;
	}
	
	int mib[] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO};
	
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
		return -2;
	}
	
	if ((buf = malloc(needed)) == NULL)
		return -3;
	
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
		free(buf);
		return -4;
	}
	
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		sin2 = (struct sockaddr_inarp *)(rtm + 1);
		sdl = (struct sockaddr_dl*)((char*)sin2 + ROUNDUP(sin2->sin_len));
		if (addr) {
			if (addr != sin2->sin_addr.s_addr)
				continue;
			res = 1;
		}

		if (maclen >= sdl->sdl_alen) {
			memcpy(mac, LLADDR(sdl), sdl->sdl_alen);
			break;
		} else {
			res = -5;
			fprintf(stderr, "utils/retrieve_outgoing_interface_address: allocated memory for mac address (%d bytes) too small (actual address: %d bytes)\n", maclen, sdl->sdl_alen);
		}
	}
	free(buf);
	
	return res;
}
#endif


int send_file(const char* filename, int fd)
{
	#define MAX_SIZE 1400
	char buf[MAX_SIZE];
	int sent, tot_sent = 0;
	char *p;

	FILE *f = fopen(filename, "r");

	if (!f)  {
		printf("fopen error");
	}

	while(!feof(f) && !ferror(f)) {
		size_t len = fread(buf, 1, MAX_SIZE, f);
		
		if (len == 0)
			break;
		
		
		p = buf;
		
		do {
			sent = send(fd, p, len, 0);
			
			if (sent < 0) {
				fprintf(stderr, "Send error\n");
				goto send_done;
			}
			len -= sent;
			p += sent;
			tot_sent += sent;
			//printf("sent %d bytes data\n", sent);
			
		} while (len);
	}
	send_done:
	printf("Total bytes sent %d\n", tot_sent);
	fclose(f);	
	return tot_sent;
}

static int detected_platform = platform_none;

#if defined(OS_MACOSX)
#ifndef OS_MACOSX_IPHONE
#include <CoreServices/CoreServices.h>
#endif /* OS_MACOSX_IPHONE */

#endif

static char hardware_name[256];

int current_platform(void)
{
	if (detected_platform == platform_none) {
		hardware_name[0] = '\0';
		{
#if defined(OS_LINUX)
		// FIXME: this does nothing interesting at the moment:
		detected_platform = platform_linux;
		strcpy(hardware_name, "Linux box");
#elif defined(OS_MACOSX_IPHONE)
		// FIXME: DETECT!
		detected_platform = platform_macosx_iphone_(0,0,0);
		strcpy(hardware_name, "iPhone");
#elif defined(OS_MACOSX)
		long	versMin;
		long	versMaj;
		long	versBugfix;
		
		// Uses Gestalt to find the major and minor version.
		Gestalt(gestaltSystemVersionMajor, &versMaj);
		Gestalt(gestaltSystemVersionMinor, &versMin);
		Gestalt(gestaltSystemVersionBugFix, &versBugfix);
		detected_platform = platform_macosx_(versMaj,versMin,versBugfix);
		strcpy(hardware_name, "Macintosh");
#elif defined(WINCE) || defined(WIN32) || defined(_WIN32) 
		OSVERSIONINFO osvi;
		// Found this in Microsoft documentation. Same for Windows Mobile and 
		// Windows XP/Vista
#if defined(WINCE)
		detected_platform = platform_windows_mobile_standard;
#else
		detected_platform = platform_windows;
#endif
		
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx (&osvi);
#if defined(WINCE)
		// Windows Mobile 5? (FIXME: Guessing here!)
		if (VER_PLATFORM_WIN32_CE==osvi.dwPlatformId && osvi.dwMajorVersion == 4)
			detected_platform = platform_windows_mobile_standard_(5);
		// Windows Mobile 6?
		if (VER_PLATFORM_WIN32_CE==osvi.dwPlatformId && osvi.dwMajorVersion == 5) {
			char	str[256*2];
			char	str2[256];
			long	i;

			SystemParametersInfo(SPI_GETPLATFORMTYPE, 256, str, 0);
			for(i = 0; i < 256; i++)
				str2[i] = str[i*2];
			// Standard or professional?
			// FIXME: this simple test is likely to only work to differentiate
			// the Samsung phone from the HTC touches. More work to follow.
			if(strcmp("SmartPhone", str2) == 0)
				detected_platform = platform_windows_mobile_standard_(6);
			else // if(strcmp("PocketPC", str2) == 0)
				detected_platform = platform_windows_mobile_professional_(6);
		}
		{
			char	str[sizeof(hardware_name)*2];
			long	i;

			SystemParametersInfo(SPI_GETOEMINFO, sizeof(hardware_name), str, 0);
			for(i = 0; i < sizeof(hardware_name); i++)
				hardware_name[i] = str[i*2];
		}
#else
		// Windows XP/Vista?
		if (VER_PLATFORM_WIN32_NT==osvi.dwPlatformId)
			detected_platform |= osvi.dwMajorVersion;
		strcpy(hardware_name, "Windows computer");
#endif
#endif
		}
	}
	return detected_platform;
}

char *get_hardware_name(void)
{
	if (detected_platform == platform_none) {
		(void) current_platform();
	}
	return hardware_name;
}
