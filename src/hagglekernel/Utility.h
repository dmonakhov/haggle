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
#ifndef _UTILITY_H
#define _UTILITY_H

#include <libcpphaggle/Platform.h>

#include "Interface.h"

/*
	Path delimeters are different from platform to platform, so use the 
	PLATFORM_PATH_DELIMITER macro to specify whichever is used on the current
	platform.
*/
#if defined(OS_LINUX) || defined(OS_MACOSX)
#define PLATFORM_PATH_DELIMITER "/"
#elif defined(OS_WINDOWS_MOBILE)
#define PLATFORM_PATH_DELIMITER "\\"
#elif defined(OS_WINDOWS_DESKTOP)
#define PLATFORM_PATH_DELIMITER "\\"
#else
#error "Unsupported Platform"
#endif

extern char *hdsp;
/*
	Default path to were to put files.
	
	This is created the first time it is used.
*/
#define HAGGLE_DEFAULT_STORAGE_PATH (hdsp?hdsp:hdsp = fill_in_default_path())
/*
	Default path to were to put the data store.
*/
#if defined(OS_WINDOWS_MOBILE)
/*
	Why this? Because on windows mobile, if you run the data base against an 
	SD card, the entire machine may lock up.
*/
extern char *ddsp;
#define DEFAULT_DATASTORE_PATH (ddsp?ddsp:ddsp = fill_in_default_datastore_path())
char *fill_in_default_datastore_path(void);
#else
#define DEFAULT_DATASTORE_PATH HAGGLE_DEFAULT_STORAGE_PATH
#endif

char *fill_in_default_path(void);

/*
	The path of the folder that haggle resides in.
*/
extern char *HAGGLE_FOLDER_PATH;

void fill_in_haggle_path(char *haggle_path);

/**
	Utility function to create all the parts of a path up to and including the
	last component. All components will be created as directories.
	
	The path is expected to use the platform-specific path delimeter, i.e.
	/ on Mac OS X and Linux, but \ on Windows and Windows Mobile.
	
	Returns: 
		True iff successful. 
		If this function fails, part of the path may already have been created.
	
	Terminates if: 
		path == NULL or 
		path is a pointer to a zero-terminated char array
	
	May change the state of:
		ERRNO
*/
bool create_path(const char *path);

/**
   Get a list of local network interfaces on the machine.  
   @param iflist the list to add found interfaces to
   @param onlyUp get only local interfaces that are marked as 'up' (default).
   @returns the number of interfaces found, or -1 on error.
 */
int getLocalInterfaceList(InterfaceRefList& iflist, const bool onlyUp = true);

#if defined(OS_WINDOWS) || defined(OS_MACOSX) || defined(OS_ANDROID)
struct ether_addr {
        u_int8_t ether_addr_octet[6];
};

struct ether_addr *ether_aton_r(const char *asc, struct ether_addr *addr);
#else
#include <netinet/ether.h>
#endif

#if defined(OS_WINDOWS_MOBILE)
char *strdup(const char *src);
#endif

#endif /* _UTILITY_H */
