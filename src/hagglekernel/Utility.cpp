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
#include "Utility.h"

#include <stdio.h>
#include <stdlib.h>

#include <libcpphaggle/String.h>
#include "Address.h"
#include "Debug.h"
#include "Trace.h"

using namespace haggle;

#if defined(OS_LINUX) || defined(OS_MACOSX)
// Needed for getlogin()
#include <unistd.h>
// Needed for stat()
#include <sys/stat.h>
// Needed for getpwuid()
#include <pwd.h>
#endif

#if defined(OS_MACOSX)
#include <ctype.h>
#endif

#if defined(OS_ANDROID)
#define DEFAULT_STORAGE_PATH "/data/haggle"
#elif defined(OS_MACOSX_IPHONE)
#define DEFAULT_STORAGE_PATH_PREFIX "/var/mobile"
#define DEFAULT_STORAGE_PATH_SUFFIX "/.Haggle"
#elif defined(OS_LINUX)
#define DEFAULT_STORAGE_PATH_PREFIX "/home/"
#define DEFAULT_STORAGE_PATH_SUFFIX "/.Haggle"
#elif defined(OS_MACOSX)
#define DEFAULT_STORAGE_PATH_PREFIX "/Users/"
#define DEFAULT_STORAGE_PATH_SUFFIX "/Library/Application Support/Haggle"
#elif defined(OS_WINDOWS)
#include <shlobj.h>
#define DEFAULT_STORAGE_PATH_PREFIX ""
#define DEFAULT_STORAGE_PATH_SUFFIX "\\Haggle"
#else
#error "Unsupported Platform"
#endif

#if defined(OS_WINDOWS) || defined(OS_MACOSX) || defined(OS_ANDROID)

#ifndef isdigit
#define isdigit(c)  (c >= '0' && c <= '9')
#endif
#ifndef islower
#define islower(c)  (c >=  'a' && c <= 'z')
#endif
#ifndef isspace
#define isspace(c)  (c ==  ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v')
#endif
#ifndef isupper
#define isupper(c)  (c >=  'A' && c <= 'Z')
#endif
#ifndef tolower
#define tolower(c)  (isupper(c) ? ( c - 'A' + 'a') : (c))
#endif
#ifndef toupper
#define toupper(c)  (islower(c) ? (c - 'a' + 'A') : (c))
#endif

/*
  ether_aton code from glibc, GPL'd.
 */
struct ether_addr *ether_aton_r(const char *asc, struct ether_addr *addr)
{
        size_t cnt;
        
        for (cnt = 0; cnt < 6; ++cnt) {
                unsigned int number;
                char ch;
                
                ch = tolower (*asc);
				asc++;
                if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f'))
                        return NULL;
                number = isdigit (ch) ? (ch - '0') : (ch - 'a' + 10);
                
                ch = tolower (*asc);
                if ((cnt < 5 && ch != ':') || (cnt == 5 && ch != '\0' && !isspace (ch))) {
                        ++asc;
                        if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f'))
                                return NULL;
                        number <<= 4;
                        number += isdigit (ch) ? (ch - '0') : (ch - 'a' + 10);
                        
                        ch = *asc;
                        if (cnt < 5 && ch != ':')
                                return NULL;
                }
                
                /* Store result.  */
                addr->ether_addr_octet[cnt] = (unsigned char) number;
                
                /* Skip ':'.  */
                ++asc;
        }
        return addr;
}

#endif

char *HAGGLE_FOLDER_PATH;

void fill_in_haggle_path(char *haggle_path)
{
	long i,l;
	
	HAGGLE_FOLDER_PATH = strdup(haggle_path);
	
	// Take the haggle[.exe] bit out:
	l = strlen(PLATFORM_PATH_DELIMITER);
	for(i = strlen(HAGGLE_FOLDER_PATH); i >= 0; i--)
		if(strncmp(&(HAGGLE_FOLDER_PATH[i]), PLATFORM_PATH_DELIMITER, l) == 0)
		{
			HAGGLE_FOLDER_PATH[i] = '\0';
			break;
		}
	HAGGLE_DBG("Haggle folder path set to: %s\n", HAGGLE_FOLDER_PATH);
}

// This pointer will be filled in the first time HAGGLE_DEFAULT_STORAGE_PATH is used
char *hdsp;

#if !defined(OS_ANDROID) && !defined(OS_MACOSX_IPHONE)
static char *fill_prefix_and_suffix(const char *fillpath)
{
	char *path;

        if (!fillpath)
                return NULL;

        path = (char *)malloc(strlen(DEFAULT_STORAGE_PATH_PREFIX) + 
                                strlen(fillpath) + strlen(DEFAULT_STORAGE_PATH_SUFFIX) + 1);
        
	if (path != NULL)
		sprintf(path, "%s%s%s" , DEFAULT_STORAGE_PATH_PREFIX, fillpath, DEFAULT_STORAGE_PATH_SUFFIX);
        
        return path;
}
#endif

#if defined(OS_ANDROID)
char *fill_in_default_path()
{
        char *path;

        path = (char*)malloc(strlen(DEFAULT_STORAGE_PATH) + 1);
        
        if (!path)
                return NULL;

	strcpy(path, DEFAULT_STORAGE_PATH);

        return path;
}
#elif defined(OS_MACOSX_IPHONE)
char *fill_in_default_path()
{
        char *path, *home;
        
        home = getenv("HOME");

        if (!home) {
                home = DEFAULT_STORAGE_PATH_PREFIX;
        }

        path = (char*)malloc(strlen(home) + strlen(DEFAULT_STORAGE_PATH_SUFFIX) + 1);
        
        if (!path)
                return NULL;

        sprintf(path, "%s%s", home, DEFAULT_STORAGE_PATH_SUFFIX);

        return path;
}
#elif defined(OS_UNIX)
char *fill_in_default_path()
{
	char *login = NULL;
        struct passwd *pwd;
	
	// First try getpwuid:
	pwd = getpwuid(getuid());

	// Success?
	if (pwd != NULL) {
		// Yes. Did we get a name?
		if (pwd->pw_name != NULL) {
			// Yes. Check to make sure we're not getting "root":
			if (strcmp(pwd->pw_name, "root") == 0) {
				printf("Haggle should not be run as root!\n");
			} else {
				login = pwd->pw_name;
			}
		}
	}
	// Have we already established a user name?
	if (login == NULL) {
		// No. Fall back to using getlogin:
		login = getlogin();
		// Success?
		if (login == NULL) {
			// No: WHOA!
			printf("Unable to get user name!\n");
			return NULL;
		}
		// Yes. Check to make sure we're not getting "root":
                /*
                  Why should one not be able to run as root if one
                  wishes?  In any case, this check should be done
                  elsewhere (e.g., in main() when first starting
                  haggle).

		if (strcmp(login, "root") == 0) {
			printf("Haggle should not be run as root!\n");
			return NULL;
		}
                */
	}
        return fill_prefix_and_suffix(login);
}
#elif defined(OS_WINDOWS_MOBILE)
static bool has_haggle_folder(LPCWSTR path)
{
	WCHAR my_path[MAX_PATH+1];
	long i, len;
	WIN32_FILE_ATTRIBUTE_DATA data;
	
	len = MAX_PATH;
	for (i = 0; i < MAX_PATH && len == MAX_PATH; i++) {
		my_path[i] = path[i];
		if (my_path[i] == 0)
			len = i;
	}
	if (len == MAX_PATH)
		return false;
	i = -1;
	do {
		i++;
		my_path[len+i] = DEFAULT_STORAGE_PATH_SUFFIX[i];
	} while(DEFAULT_STORAGE_PATH_SUFFIX[i] != 0 && i < 15);

	if (GetFileAttributesEx(my_path, GetFileExInfoStandard, &data)) {
		return (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	} 
	return false;
}

#include <projects.h>
#pragma comment(lib,"note_prj.lib")
char *fill_in_default_path()
{
	HANDLE	find_handle;
	WIN32_FIND_DATA	find_data;
	char path[MAX_PATH+1];
	WCHAR best_path[MAX_PATH+1];
	bool best_path_has_haggle_folder;
	ULARGE_INTEGER best_avail, best_size;
	long i;
	
	// Start with the application data folder, as a fallback:
	if (!SHGetSpecialFolderPath(NULL, best_path, CSIDL_APPDATA, FALSE)) {
		best_path[0] = 0;
		best_avail.QuadPart = 0;
		best_size.QuadPart = 0;
		best_path_has_haggle_folder = false;
	} else {
		GetDiskFreeSpaceEx(best_path, &best_avail, &best_size, NULL);
		best_path_has_haggle_folder = has_haggle_folder(best_path);
	}
	fprintf(stderr,"Found data card path: \"%ls\" (size: %I64d/%I64d, haggle folder: %s)\n", 
		best_path, best_avail, best_size,
		best_path_has_haggle_folder?"Yes":"No");

	find_handle = FindFirstFlashCard(&find_data);
	if (find_handle != INVALID_HANDLE_VALUE) {
		do {
			// Ignore the root directory (this has been checked for above)
			if (find_data.cFileName[0] != 0) {
				ULARGE_INTEGER	avail, size, free;
				bool haggle_folder;
				
				GetDiskFreeSpaceEx(find_data.cFileName, &avail, &size, &free);
				haggle_folder = has_haggle_folder(find_data.cFileName);
				fprintf(stderr,"Found data card path: \"%ls\" (size: %I64d/%I64d, haggle folder: %s)\n", 
					find_data.cFileName, avail, size,
					haggle_folder?"Yes":"No");
				// is this a better choice than the previous one?
				// FIXME: should there be any case when a memory card is not used?
				if (true) {
					// Yes.
					
					// Save this as the path to use:
					for (i = 0; i < MAX_PATH; i++)
						best_path[i] = find_data.cFileName[i];

					best_avail = avail;
					best_size = size;
					best_path_has_haggle_folder = haggle_folder;
				}
			}
		} while(FindNextFlashCard(find_handle, &find_data));

		FindClose(find_handle);
	}
	// Convert the path to normal characters.
	for (i = 0; i < MAX_PATH; i++)
		path[i] = (char) best_path[i];

	return fill_prefix_and_suffix(path);
}

char *ddsp;

char *fill_in_default_datastore_path()
{
	char path[MAX_PATH+1];
	WCHAR best_path[MAX_PATH+1];
	ULARGE_INTEGER best_avail, best_size;
	long i;
	
	// Start with the application data folder, as a fallback:
	if (!SHGetSpecialFolderPath(NULL, best_path, CSIDL_APPDATA, FALSE)) {
		best_path[0] = 0;
	} else {
		GetDiskFreeSpaceEx(best_path, &best_avail, &best_size, NULL);
	}
	
	// Convert the path to normal characters.
	for (i = 0; i < MAX_PATH; i++)
		path[i] = (char) best_path[i];

	return fill_prefix_and_suffix(path);
}

#elif defined(OS_WINDOWS_XP) || defined(OS_WINDOWS_2000)	
char *fill_in_default_path()
{
	char login[MAX_PATH];
	
	if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, login) != S_OK) {
		fprintf(stderr, "Unable to get folder path!\n");
		return NULL;
	}

        return fill_prefix_and_suffix(login);
}
#elif defined(OS_WINDOWS_VISTA)
char *fill_in_default_path()
{
	PWSTR login1;
	char login[MAX_PATH];
	long i;
	
	if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &login1) != S_OK) {
		fprintf(stderr, "Unable to get user application data folder!\n");
		return NULL;
	}
	
	for(i = 0; login1[i] != 0; i++)
		login[i] = (char) login1[i];
	login[i] = 0;
	CoTaskMemFree(login1);

        return fill_prefix_and_suffix(login);
}
#endif

bool create_path(const char *p)
{
	// Make sure we don't try to create the string using a NULL pointer:
	if (p == NULL)
		return false;
	
	// Make a string out of the pointer:
	string path = p;
	
	// Check if the path is ""
	if (path.empty())
		return false;
	
	// prefix - the path to the directory directly above the desired directory:
	string prefix = path.substr(0, path.find_last_of(PLATFORM_PATH_DELIMITER));
	
#if defined(OS_WINDOWS)
	wchar_t *wpath = strtowstr_alloc(path.c_str());
	
	if (!wpath)
		return false;

	// Try to create the desired directory:
	if (CreateDirectory(wpath, NULL) != 0) {
		// No? What went wrong?
		switch (GetLastError()) {
			// The directory already existed:
			case ERROR_ALREADY_EXISTS:
				// Done:
				free(wpath);
				return true;
			break;
			
			// Part of the path above the desired directory didn't exist:
			case ERROR_PATH_NOT_FOUND:
				// Try to create that path:
				if (!create_path(prefix.c_str())) {
					// That failed? Then fail.
					free(wpath);
					return false;
				}
				
				// Now the path above the desired directory exists. Create the 
				// desired directory
				if (CreateDirectory(wpath, NULL) != 0) {
					// Failed? Then fail.
					free(wpath);
					return false;
				}
			break;
		}
	}
	free(wpath);
#else 
	struct stat sbuf;
	
	// Check if the directory already exists:
	if (stat(path.c_str(), &sbuf) == 0) {
		// It does. Is it a directory?
		if ((sbuf.st_mode & S_IFDIR) == S_IFDIR) {
			// Yes. Succeed:
			return true;
		} else {
			// No. Fail:
			return false;
		}
	}
	
	// Check to see if the directory above it exists
	if (stat(prefix.c_str(), &sbuf) == 0) {
		switch (errno) {
			// Access violation - unable to write here:
			case EACCES:
			// Would create a loop in the directory tree:
			case ELOOP:
			// Path name too long:
			case ENAMETOOLONG:
			// One of the prior path member was not a directory:
			case ENOTDIR:
				// unrecoverable error:
				return false;
			break;
			
			default:
				// Recoverable error:
				
				// Try to create the directory directly above the desired 
				// directory
				if (!create_path(prefix.c_str())) {
					// Not possible? Fail.
					return false;
				}
			break;
		}
	}
	// The directory above this one now exists - create the desired directory:
	if (mkdir(path.c_str(), 
		// This means: 755.
		S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
		// mkdir failed. Fail:
		return false;
	}
#endif
	// Success:
	return true;
}

#if defined(OS_ANDROID)

#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>

// Android property manager
#include <cutils/properties.h>

#include "TI_IPC_Api.h"
#include "TI_AdapterApiC.h"
/*
  This is a bit of a hack for the android platform. It seems that the
  TI wifi interface cannot be read by the normal ioctl() functions
  (at least not to discover the name and mac). Therefore we have special
  code that uses the driver API directly.
 */
int getLocalInterfaceList(InterfaceRefList& iflist, const bool onlyUp) 
{
        char wifi_iface_name[PROPERTY_VALUE_MAX];
        OS_802_11_MAC_ADDRESS hwaddr;
        char mac[6];
        tiINT32 res;
        unsigned int flags;
        in_addr_t addr, baddr;
        struct ifreq ifr;   
        int ret, s;
        Addresses addrs;
        
        // Read the WiFi interface name from the Android property manager
        ret = property_get("wifi.interface", wifi_iface_name, "sta");
        
        if (ret < 1)
                return -1;

        // Get a handle to the adapter throught the TI API
        TI_HANDLE h_adapter = TI_AdapterInit((tiCHAR *)wifi_iface_name);

        if (h_adapter == NULL)
                return -1;

        memset(&hwaddr, 0, sizeof(OS_802_11_MAC_ADDRESS));
        
        // Read the mac from the adapter
        res = TI_GetCurrentAddress(h_adapter, &hwaddr);
        
        if (res != TI_RESULT_OK) {
                // Deinit handle
                TI_AdapterDeinit(h_adapter);
                return -1;
        }

        memcpy(mac, &hwaddr, 6);

        addrs.add(new Address(AddressType_EthMAC, (void *)mac));

        // We are done with the adapter handle
        TI_AdapterDeinit(h_adapter);

        s = socket(AF_INET, SOCK_DGRAM, 0);
        
        if (s < 0) {
            HAGGLE_ERR("socket() failed: %s\n", strerror(errno));
            return -1;
        }

        memset(&ifr, 0, sizeof(struct ifreq));
        strncpy(ifr.ifr_name, wifi_iface_name, IFNAMSIZ);
        ifr.ifr_name[IFNAMSIZ - 1] = 0;

        /*
          Getting the mac address via ioctl() does not seem to work
        for the TI wifi interface, but we got the mac from the driver
        API instead

        ret = ioctl(s, SIOCGIFHWADDR, &ifr);

        if (ret < 0) {
                CM_DBG("Could not get mac address of interface %s\n", name);
                close(s);
                return NULL;
        }
        */

        if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0) {
                flags = 0;
        } else {
                flags = ifr.ifr_flags;
        }
        
        if (onlyUp && !(flags & IFF_UP)) {
                close(s);
                return 0;
        }

        if (ioctl(s, SIOCGIFADDR, &ifr) < 0) {
                addr = 0;
        } else {
                addr = ((struct sockaddr_in*) &ifr.ifr_addr)->sin_addr.s_addr;
        }

        if (ioctl(s, SIOCGIFBRDADDR, &ifr) < 0) {
                baddr = 0;
        } else {
                baddr = ((struct sockaddr_in *)&ifr.ifr_broadaddr)->sin_addr.s_addr;
                addrs.add(new Address(AddressType_IPv4, &addr, &baddr));
        }

        close(s);

        iflist.push_back(InterfaceRef(new Interface(IFTYPE_ETHERNET, mac, &addrs, wifi_iface_name, IFFLAG_LOCAL | ((flags & IFF_UP) ? IFFLAG_UP : 0))));

        return 1;
}

#elif defined(OS_LINUX)
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>

int getLocalInterfaceList(InterfaceRefList& iflist, const bool onlyUp) 
{
        char buf[1024];
	int sock;
        struct ifconf ifc;
	struct ifreq *ifr;
	int n, i, num = 0;
        
        sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock == INVALID_SOCKET) {
		HAGGLE_ERR("Could not calculate node id\n");
		return -1;
	}

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
        
	if (ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
		HAGGLE_ERR("SIOCGIFCONF failed\n");
		return -1;
	}
        
        ifr = ifc.ifc_req;
	n = ifc.ifc_len / sizeof(struct ifreq);
        
	for (i = 0; i < n; i++) {
                Addresses addrs;
		struct ifreq *item = &ifr[i];

                // if (ioctl(sock, SIOCGIFBRDADDR, item) < 0)
                //         continue;         
                
		//printf(", MAC %s\\n", ether_ntoa((struct ether_addr *)item->ifr_hwaddr.sa_data));
                if (onlyUp && !(item->ifr_flags & IFF_UP)) 
                        continue;

                addrs.add(new Address(AddressType_IPv4, &((struct sockaddr_in *)&item->ifr_addr)->sin_addr.s_addr, &((struct sockaddr_in *)&item->ifr_broadaddr)->sin_addr.s_addr));
                
		if (ioctl(sock, SIOCGIFHWADDR, item) < 0)
                        continue;
               
                addrs.add(new Address(AddressType_EthMAC, item->ifr_hwaddr.sa_data));

                iflist.push_back(InterfaceRef(new Interface(IFTYPE_ETHERNET, item->ifr_hwaddr.sa_data, &addrs, item->ifr_name, IFFLAG_LOCAL | ((item->ifr_flags & IFF_UP) ? IFFLAG_UP : 0))));
                num++;
        }
        
        close(sock);
              
        return num;
}
#elif defined(OS_MACOSX)

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <sys/types.h>
#define	max(a,b) ((a) > (b) ? (a) : (b))

int getLocalInterfaceList(InterfaceRefList& iflist, const bool onlyUp) 
{
	int sock, num = 0, ret = -1;
#define REQ_BUF_SIZE (sizeof(struct ifreq) * 20)
	struct {
		struct ifconf ifc;
		char buf[REQ_BUF_SIZE];
	} req = { { REQ_BUF_SIZE, { req.buf}}, { 0 } };

	sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock == INVALID_SOCKET) {
		HAGGLE_ERR("Could not open socket\n");
		return -1;
	}

	ret = ioctl(sock, SIOCGIFCONF, &req);

	if (ret < 0) {
		HAGGLE_ERR("ioctl() failed\n");
		return -1;
	}

	struct ifreq *ifr = (struct ifreq *) req.buf;
	int len = 0;
	
	for (; req.ifc.ifc_len != 0; ifr = (struct ifreq *) ((char *) ifr + len), req.ifc.ifc_len -= len) {
		unsigned char macaddr[ETH_ALEN];
		struct in_addr ip, bc;

		len = (sizeof(ifr->ifr_name) + max(sizeof(struct sockaddr),
						       ifr->ifr_addr.sa_len));

		if (ifr->ifr_addr.sa_family != AF_LINK	// || strncmp(ifr->ifr_name, "en", 2) != 0
		    ) {
			continue;
		}
		struct sockaddr_dl *ifaddr = (struct sockaddr_dl *) &ifr->ifr_addr;

		// Type 6 seems to be Ethernet
		if (ifaddr->sdl_type != 6) {
			continue;
		}

		memcpy(macaddr, LLADDR(ifaddr), ETH_ALEN);

		ifr->ifr_addr.sa_family = AF_INET;

		if (ioctl(sock, SIOCGIFADDR, ifr) == -1) {
			// No IP adress!
			continue;
		}
		memcpy(&ip, &((struct sockaddr_in *) &ifr->ifr_addr)->sin_addr, sizeof(struct in_addr));

		if (ioctl(sock, SIOCGIFBRDADDR, ifr) == -1) {
			// No Broadcast adress!
			continue;
		}
		memcpy(&bc, &((struct sockaddr_in *) &ifr->ifr_broadaddr)->sin_addr, sizeof(struct in_addr));

		if (ioctl(sock, SIOCGIFFLAGS, ifr) == -1) {
                        continue;
                }

                if (onlyUp && !(ifr->ifr_flags & IFF_UP)) 
                        continue;

		Addresses addrs;
		addrs.add(new Address(AddressType_EthMAC, macaddr));
		addrs.add(new Address(AddressType_IPv4, (unsigned char *) &ip, (unsigned char *) &bc));
		
		// FIXME: separate 802.3 (wired) from 802.11 (wireless) ethernet
		iflist.push_back(InterfaceRef(new Interface(IFTYPE_ETHERNET, macaddr, &addrs, ifr->ifr_name, IFFLAG_UP | IFFLAG_LOCAL)));
			
                num++;
	}

	close(sock);

	return num;
}
#elif defined(OS_WINDOWS)

#include <iphlpapi.h>

int getLocalInterfaceList(InterfaceRefList& iflist, const bool onlyUp) 
{
	IP_ADAPTER_ADDRESSES *ipAA, *ipAA_head;
	DWORD flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_PREFIX;
	ULONG bufSize;
        int num = 0;

	// Figure out required buffer size
	DWORD iReturn = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, NULL, &bufSize);

	if (iReturn != ERROR_BUFFER_OVERFLOW) {
		return -1;
	}

	// Allocate the required buffer
	ipAA_head = (IP_ADAPTER_ADDRESSES *)malloc(bufSize);

	if (!ipAA_head) {
		HAGGLE_ERR("Could not allocate IP_ADAPTER_ADDRESSES buffer\n");
		return -1;
	}

	// Now, get the information for real
	iReturn = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, ipAA_head, &bufSize);

	switch (iReturn) {
		case ERROR_SUCCESS:
			break;
		case ERROR_NO_DATA:
		case ERROR_BUFFER_OVERFLOW:
		case ERROR_INVALID_PARAMETER:
		default:
			HAGGLE_ERR("ERROR: %s\n", STRERROR(ERRNO));
			free(ipAA_head);
			return -1;
	}
	
	// Loop through all the adapters and their addresses
	for (ipAA = ipAA_head; ipAA; ipAA = ipAA->Next) {
		
		// Ignore interfaces that are not Ethernet or 802.11
		if (ipAA->IfType != IF_TYPE_IEEE80211 &&
			ipAA->IfType != IF_TYPE_FASTETHER &&
			ipAA->IfType != IF_TYPE_GIGABITETHERNET &&
			ipAA->IfType != IF_TYPE_ETHERNET_CSMACD) {
		
			//HAGGLE_DBG("Skipping interface of wrong type = %d\n", ipAA->IfType);
			continue;
		}

		// Ignore non operational interfaces
		if (onlyUp && ipAA->OperStatus != IfOperStatusUp) {
			//HAGGLE_DBG("Skipping non operational interface status=%d\n", ipAA->OperStatus);
			continue;
		}

		if (ipAA->Mtu > 1500) {
			// Some weird MTU. Some discovered "Ethernet" interfaces have a really high MTU.
			// These are probably virtual serial line interfaces enabled when synching the phone 
			// over, e.g., a USB cable.
			continue;
		}
		// Ok, this interface seems to be interesting
		Address mac(AddressType_EthMAC, ipAA->PhysicalAddress);

		Interface *ethIface = new Interface(IFTYPE_ETHERNET, (char *)ipAA->PhysicalAddress, &mac, 
				ipAA->AdapterName, IFFLAG_LOCAL | ((ipAA->OperStatus == IfOperStatusUp) ? IFFLAG_UP : 0));
		/*	
		HAGGLE_DBG("LOCAL Interface type=%d index=%d name=%s mtu=%d mac=%s\n", 
			ipAA->IfType, ipAA->IfIndex, ipAA->AdapterName, ipAA->Mtu, mac.getAddrStr());
		*/	
		IP_ADAPTER_UNICAST_ADDRESS *ipAUA;
		IP_ADAPTER_PREFIX *ipAP;

		for (ipAUA = ipAA->FirstUnicastAddress, ipAP = ipAA->FirstPrefix; ipAUA && ipAP; ipAUA = ipAUA->Next, ipAP = ipAP->Next) {
			if (ipAUA->Address.lpSockaddr->sa_family == AF_INET) {
				struct in_addr bc;
				struct sockaddr_in *saddr_v4 = (struct sockaddr_in *)ipAUA->Address.lpSockaddr;
				DWORD mask = 0xFFFFFFFF;

				memcpy(&bc, &saddr_v4->sin_addr, sizeof(bc));

				// Create mask
				mask >>= (32 - ipAP->PrefixLength);
				
				// Create broadcast address
				bc.S_un.S_addr = saddr_v4->sin_addr.S_un.S_addr | ~mask;

				Address ipv4(AddressType_IPv4, (unsigned char *)&saddr_v4->sin_addr, (unsigned char *)&bc);
				ethIface->addAddress(&ipv4);
				/*
				HAGGLE_DBG("IPv4 ADDRESS for interface[%s]: IP=%s PrefixLen=%d Broadcast=%s\n", 
					mac.getAddrStr(), ip_to_str(saddr_v4->sin_addr), 
					ipAP->PrefixLength, ip_to_str(bc));
				*/

#if defined(ENABLE_IPv6)
			} else if (ipAUA->Address.lpSockaddr->sa_family == AF_INET6) {
				// TODO: Handle IPv6 prefix and broadcast address
				struct sockaddr_in6 *saddr_v6 = (struct sockaddr_in6 *)ipAUA->Address.lpSockaddr;
				Address ipv6(AddressType_IPv6, (unsigned char *)&saddr_v6->sin6_addr);
				ethIface->addAddress(&ipv6);
#endif
			}
		}
                iflist.push_back(InterfaceRef(ethIface));
                num++;
	}

	free(ipAA_head);

        return num;
}

#else

// For those platforms with no implementation of this function

int getLocalInterfaceList(InterfaceRefList& iflist, const bool onlyUp) 
{
        return -1;
}

#endif

#if defined(OS_WINDOWS_MOBILE)
char *strdup(const char *src)
{
	char *retval;

	retval = (char *) malloc(strlen(src)+1);
	if(retval)
		strcpy(retval, src);
	return retval;
}
#endif
