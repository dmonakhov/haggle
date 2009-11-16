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

#define LIBHAGGLE_INTERNAL
#include <libhaggle/platform.h>

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef OS_WINDOWS
/*
 * Copyright (c) 1998 Softweyr LLC.  All rights reserved.
 *
 * strtok_r, from Berkeley strtok
 * Oct 13, 1998 by Wes Peters <wes@softweyr.com>
 *
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notices, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notices, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *
 *	This product includes software developed by Softweyr LLC, the
 *      University of California, Berkeley, and its contributors.
 *
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SOFTWEYR LLC, THE REGENTS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL SOFTWEYR LLC, THE
 * REGENTS, OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

char *
strtok_r(char *s, const char *delim, char **last)
{
    char *spanp;
    int c, sc;
    char *tok;

    if (s == NULL && (s = *last) == NULL)
    {
	return NULL;
    }

    /*
     * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
     */
cont:
    c = *s++;
    for (spanp = (char *)delim; (sc = *spanp++) != 0; )
    {
	if (c == sc)
	{
	    goto cont;
	}
    }

    if (c == 0)		/* no non-delimiter characters */
    {
	*last = NULL;
	return NULL;
    }
    tok = s - 1;

    /*
     * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
     * Note that delim must have one NUL; we stop if we see that, too.
     */
    for (;;)
    {
	c = *s++;
	spanp = (char *)delim;
	do
	{
	    if ((sc = *spanp++) == c)
	    {
		if (c == 0)
		{
		    s = NULL;
		}
		else
		{
		    char *w = s - 1;
		    *w = '\0';
		}
		*last = s;
		return tok;
	    }
	}
	while (sc != 0);
    }
    /* NOTREACHED */
}

wchar_t *strtowstr(const char *str)
{
	wchar_t *wstr;

	wstr = (wchar_t *)malloc(sizeof(wchar_t) * (strlen(str) + 1));

	if (!wstr)
		return NULL;

	MultiByteToWideChar(CP_UTF8, 0, str, strlen(str) + 1, wstr, strlen(str) + 1);

	return wstr;
}

#endif /* OS_WINDOWS */


/*
	Define default path delimiters for each platform
*/

static char path[MAX_PATH_LEN + 1];

#if defined(OS_WINDOWS_MOBILE)
static int has_haggle_folder(LPCWSTR path)
{
	WCHAR	my_path[MAX_PATH+1];
	long	i, len;
	WIN32_FILE_ATTRIBUTE_DATA	data;
	
	len = MAX_PATH;
	for(i = 0; i < MAX_PATH && len == MAX_PATH; i++)
	{
		my_path[i] = path[i];
		if(my_path[i] == 0)
			len = i;
	}
	if(len == MAX_PATH)
		return 0;
	i = -1;
	do{
		i++;
		my_path[len+i] = DEFAULT_STORAGE_PATH_POSTFIX[i];
	}while(DEFAULT_STORAGE_PATH_POSTFIX[i] != 0 && i < 15);
	if(GetFileAttributesEx(my_path, GetFileExInfoStandard, &data))
	{
		return (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}else{
		return 0;
	}
}

#include <projects.h>
#pragma comment(lib,"note_prj.lib")
void fill_in_default_path()
{
	HANDLE	find_handle;
	WIN32_FIND_DATA	find_data;
	WCHAR best_path[MAX_PATH+1];
	int best_path_has_haggle_folder;
	ULARGE_INTEGER best_avail, best_size;
	long i;
	
	// Start with the application data folder, as a fallback:
	if (!SHGetSpecialFolderPath(NULL, best_path, CSIDL_APPDATA, FALSE)) {
		best_path[0] = 0;
		best_avail.QuadPart = 0;
		best_size.QuadPart = 0;
		best_path_has_haggle_folder = 0;
	}else{
		GetDiskFreeSpaceEx(best_path, &best_avail, &best_size, NULL);
		best_path_has_haggle_folder = has_haggle_folder(best_path);
	}
	fprintf(stderr,"Found data card path: \"%ls\" (size: %I64d/%I64d, haggle folder: %s)\n", 
		best_path, best_avail, best_size,
		best_path_has_haggle_folder?"Yes":"No");

	find_handle = FindFirstFlashCard(&find_data);
	if(find_handle != INVALID_HANDLE_VALUE)
	{
		do{
			// Ignore the root directory (this has been checked for above)
			if(find_data.cFileName[0] != 0)
			{
				ULARGE_INTEGER	avail, size, free;
				int haggle_folder;
				
				GetDiskFreeSpaceEx(find_data.cFileName, &avail, &size, &free);
				haggle_folder = has_haggle_folder(find_data.cFileName);
				fprintf(stderr,"Found data card path: \"%ls\" (size: %I64d/%I64d, haggle folder: %s)\n", 
					find_data.cFileName, avail, size,
					haggle_folder?"Yes":"No");
				// is this a better choice than the previous one?
				// FIXME: should there be any case when a memory card is not used?
				if(1)
				{
					// Yes.
					
					// Save this as the path to use:
					for(i = 0; i < MAX_PATH; i++)
						best_path[i] = find_data.cFileName[i];
					best_avail = avail;
					best_size = size;
					best_path_has_haggle_folder = haggle_folder;
				}
			}
		}while(FindNextFlashCard(find_handle, &find_data));

		FindClose(find_handle);
	}
	// Convert the path to normal characters.
	for(i = 0; i < MAX_PATH; i++)
		path[i] = (char) best_path[i];
}

const char *platform_get_path(path_type_t type, const char *append)
{
	long len = 0;
	
        wchar_t login1[MAX_PATH];
        int wintype = 0;
        
        switch (type) {
                case PLATFORM_PATH_PROGRAM:
                        wintype = CSIDL_PROGRAM_FILES;
                        break;
                case PLATFORM_PATH_PRIVATE:
					wintype = CSIDL_APPDATA;
					break;
				case PLATFORM_PATH_DATA:
                        wintype = CSIDL_APPDATA;
						fill_in_default_path();
						len = strlen(path);
						goto path_valid;
                        break;
                case PLATFORM_PATH_TEMP:
                        wintype = CSIDL_APPDATA;
                        break;
                default:
                        return NULL;
        }

	if (!SHGetSpecialFolderPath(NULL, login1, wintype, FALSE)) {
		return NULL;
	}
	for (len = 0; login1[len] != 0; len++)
		path[len] = (char) login1[len];

path_valid:
        if (type == PLATFORM_PATH_PROGRAM || 
		type == PLATFORM_PATH_PRIVATE || 
		type == PLATFORM_PATH_DATA || 
		type == PLATFORM_PATH_TEMP) {
                if (len + strlen(DEFAULT_STORAGE_PATH_POSTFIX) > MAX_PATH_LEN)
                        return NULL;
                len += snprintf(path + len, MAX_PATH_LEN - len, "%s", DEFAULT_STORAGE_PATH_POSTFIX);
	} 
        if (append) {
                if (len + strlen(append) > MAX_PATH_LEN)
                        return NULL;
                strcpy(path + len, append);
        }
        return path;
}
#elif defined(OS_WINDOWS_VISTA)

const char *platform_get_path(path_type_t type, const char *append)
{
#if 0 // Only on Windows Vista
	wchar_t *login1;
#else
	wchar_t login1[256];
#endif
	long len = 0;
#if 0 // Only on Windows Vista
        GUID wintype;
#else
	int folder;
#endif
        switch (type) {
                case PLATFORM_PATH_PROGRAM:
#if 0 // Only on Windows Vista
                        wintype = FOLDERID_ProgramFiles;
#else
			folder = CSIDL_PROGRAM_FILES;
#endif
			break;
                case PLATFORM_PATH_PRIVATE:
                case PLATFORM_PATH_DATA:
#if 0 // Only on Windows Vista
                        wintype = FOLDERID_LocalAppData;
#else
			folder = CSIDL_LOCAL_APPDATA;
#endif
                        break;
                case PLATFORM_PATH_TEMP:
#if 0 // Only on Windows Vista
                        wintype = FOLDERID_LocalAppData;
#else
			folder = CSIDL_LOCAL_APPDATA;
#endif
                        break;
                default:
                        return NULL;
        }

#if 0 // Only on Windows Vista
	if (SHGetKnownFolderPath(&wintype, 0, NULL, &login1) != S_OK)
#else
	if(SHGetFolderPath(NULL, folder, NULL, SHGFP_TYPE_CURRENT, login1) != S_OK)
#endif
	{
		return NULL;
	}
	for (len = 0; login1[len] != 0; len++)
		path[len] = (char) login1[len];

	path[len] = '\0';
#if 0 // Only on Windows Vista
	CoTaskMemFree(login1);
#endif

        if (type == PLATFORM_PATH_PROGRAM || 
		type == PLATFORM_PATH_PRIVATE || 
		type == PLATFORM_PATH_DATA || 
		type == PLATFORM_PATH_TEMP) {
                if (len + strlen(DEFAULT_STORAGE_PATH_POSTFIX) > MAX_PATH_LEN)
                        return NULL;
                len += snprintf(path + len, MAX_PATH_LEN - len, "%s", DEFAULT_STORAGE_PATH_POSTFIX);
	} 
        if (append) {
                if (len + strlen(append) > MAX_PATH_LEN)
                        return NULL;
                strcpy(path + len, append);
        }
        return path;
}

#elif defined(OS_ANDROID)

const char *platform_get_path(path_type_t type, const char *append)
{
        switch (type) {
                case PLATFORM_PATH_PROGRAM:
                        sprintf(path, "/system/bin");
                        break;
                case PLATFORM_PATH_PRIVATE:
                case PLATFORM_PATH_DATA:
                        strcpy(path, "/data/haggle");
                        break;
                case PLATFORM_PATH_TEMP:
                        strcpy(path, "/data/haggle");
                        break;
                default:
                        return NULL;
        }
        
        if (append) {
                if (strlen(path) + strlen(append) > MAX_PATH_LEN)
                        return NULL;
                strcpy(path +  strlen(path), append);
        }
        return path;
}

#elif defined(OS_MACOSX_IPHONE)
//#include <CoreFoundation/CoreFoundation.h>
/*
  All iPhone applications are sandboxed, which means they cannot
  access the parts of the filesystem where Haggle lives. Therefore,
  an iPhone application cannot, for example, read the Haggle daemon's
  pid file, or launch the daemon if it is not running.

  The application can neither read any data objects passed as files if
  they are not in the application's sandbox.
 */
const char *platform_get_path(path_type_t type, const char *append)
{
        /*
        CFStringRef homeDir = (CFStringRef)NSHomeDirectory();

        CFStringGetCString(homeDir, path, MAX_PATH_LEN, kCFStringEncodingUTF8);
        */
        switch (type) {
                case PLATFORM_PATH_PROGRAM:
                        // TODO: Set to something that makes sense, considering
                        // the iPhone apps are sandboxed.
                        strcpy(path, "/usr/bin");
                        break;
                case PLATFORM_PATH_PRIVATE:
                case PLATFORM_PATH_DATA:
                        strcpy(path, "/var/cache/haggle");
                        break;
                case PLATFORM_PATH_TEMP:
                        strcpy(path, "/tmp");
                        break;
                default:
                        return NULL;
        }
        
        if (append) {
                if (strlen(path) + strlen(append) > MAX_PATH_LEN) {
                        return NULL;
                }
                strcpy(path +  strlen(path), append);
        }
        return path;
}
#elif defined(OS_UNIX)
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

const char *platform_get_path(path_type_t type, const char *append)
{
        struct passwd *pwd;
        char *login = NULL;
        long len = 0;
        path[0] = '\0';

        switch (type) {
                case PLATFORM_PATH_PROGRAM:
                        strcpy(path, "/usr/bin");
                        break;
                case PLATFORM_PATH_PRIVATE:
                case PLATFORM_PATH_DATA:
                        pwd = getpwuid(getuid());

                        if (pwd && pwd->pw_name) {
                                login = pwd->pw_name;
                        } else {
                                login = getlogin();

                                if (!login)
                                        return NULL;
                        }
                        if (strlen(DEFAULT_STORAGE_PATH_PREFIX) +
                            strlen(login) +
                            strlen(DEFAULT_STORAGE_PATH_POSTFIX) > MAX_PATH_LEN)
                                return NULL;
                        
                        len += snprintf(path, MAX_PATH_LEN, "%s%s%s",
                                        DEFAULT_STORAGE_PATH_PREFIX,
                                        login,
                                        DEFAULT_STORAGE_PATH_POSTFIX);
                        break;
                case PLATFORM_PATH_TEMP:
                        strcpy(path, "/tmp");
                        break;
                default:
                        return NULL;
        }
        
        if (append) {
                if (strlen(path) + strlen(append) > MAX_PATH_LEN)
                        return NULL;
                strcpy(path +  strlen(path), append);
        }
        return path;
}
#else

const char *platform_get_path(path_type_t type, const char *append)
{
        return NULL;
}
#endif
