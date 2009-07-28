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
const char *platform_get_path(path_type_t type, const char *append)
{
        wchar_t login1[MAX_PATH];
	long len = 0;
        int wintype = 0;
        
        switch (type) {
                case PLATFORM_PATH_PROGRAM:
                        wintype = CSIDL_PROGRAM_FILES;
                        break;
                case PLATFORM_PATH_DATA:
                        wintype = CSIDL_APPDATA;
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

	path[len] = '\0';

        if (type == PLATFORM_PATH_PROGRAM || 
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
        wchar_t *login1;
	long len = 0;
        GUID wintype;
        
        switch (type) {
                case PLATFORM_PATH_PROGRAM:
                        wintype = FOLDERID_ProgramFiles;
                        break;
                case PLATFORM_PATH_DATA:
                        wintype = FOLDERID_LocalAppData;
                        break;
                case PLATFORM_PATH_TEMP:
                        wintype = FOLDERID_LocalAppData;
                        break;
                default:
                        return NULL;
        }

	if (SHGetKnownFolderPath(&wintype, 0, NULL, &login1) != S_OK) {
		return NULL;
	}
	for (len = 0; login1[len] != 0; len++)
		path[len] = (char) login1[len];

	path[len] = '\0';
	CoTaskMemFree(login1);

        if (type == PLATFORM_PATH_PROGRAM || 
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
