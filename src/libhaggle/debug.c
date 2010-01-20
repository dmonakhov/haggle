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

#include <stdio.h>
#include <stdarg.h>

#define LIBHAGGLE_INTERNAL
#include <libhaggle/haggle.h>

#if defined(WINCE)
FILE *tr_out = NULL;
FILE *tr_err = NULL;
#else
#define tr_out stdout
#define tr_err stderr
#endif

static int trace_level = 2;

void set_trace_level(int level)
{
	trace_level = level;
}

int libhaggle_debug_init()
{
#ifdef WINCE2
	const char *path = platform_get_path(PLATFORM_PATH_DATA, "/libhaggle.txt");

	if (!path || tr_out || tr_err)
		return -1;

	tr_out = tr_err = fopen(path, "w");

	if (!tr_out)
		return -1;
#endif
	return 0;
}

void libhaggle_debug_fini()
{
#ifdef WINCE
	if (tr_out)
		fclose(tr_out);
#endif
}
int libhaggle_trace(int err, const char *func, const char *fmt, ...)
{
	static char buf[1024];
	va_list args;
	int len;

	if (trace_level == 0 || (trace_level == 1 && err == 0))
		return 0;

	va_start(args, fmt);
#ifdef WINCE
	len = _vsnprintf(buf, 1024, fmt, args);
#else
	len = vsnprintf(buf, 1024, fmt, args);
#endif
	va_end(args);

	fprintf((err ? tr_err : tr_out), "%s: %s", func, buf);
	fflush(tr_out);

	return 0;
}

