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
#include "Trace.h"
#include <libcpphaggle/Thread.h>
#include <libcpphaggle/Timeval.h>
#include <libcpphaggle/Exception.h>

#include <stdio.h>
#include <stdarg.h>

Trace Trace::trace;

Trace::Trace(TraceType_t _type, bool _enabled) :
	type(_type), traceFile(NULL), startTime(Timeval::now()), enabled(_enabled)
{
       	set_trace_timestamp_base(startTime.getTimevalStruct());
}

Trace::~Trace()
{
	if (traceFile) {
		fclose(traceFile);
	}
	traceFile = NULL;
}


#define TRACE_BUFLEN 5000

int Trace::write(const TraceType_t _type, const char *func, const char *fmt, ...)
{
        Mutex::AutoLocker l(m);
	char buf[TRACE_BUFLEN];
	va_list args;
	int len;
	Timeval t = Timeval::now() - startTime;
	FILE *stream = NULL;
	unsigned long thrNum = 0;

	if (!enabled)
		return 0;

	memset(buf, 0, TRACE_BUFLEN);
	memset(&args, 0, sizeof(va_list));

	switch (_type) {
		case TRACE_TYPE_ERROR: 
			stream = stderr;
			break;
		case TRACE_TYPE_LOG: 
			stream = NULL;
		case TRACE_TYPE_DEBUG: 
		default:
			stream = stdout;
			break;
	}

	if (_type == TRACE_TYPE_LOG) {
		va_start(args, fmt);
		len = vfprintf(stream, fmt, args);
		va_end(args);
		return len;
	}
	
	va_start(args, fmt);

	Thread::selfGetNum(&thrNum);

#ifdef WINCE
	len = _vsnprintf(buf, TRACE_BUFLEN, fmt, args);
#else
	len = vsnprintf(buf, TRACE_BUFLEN, fmt, args);
#endif
	va_end(args);
	
	if (stream)
		fprintf(stream, "%.3lf:[%lu]{%s}:%s", t.getTimeAsSecondsDouble(), thrNum, func, buf);

	if (traceFile)
		fprintf(traceFile, "%.3lf:[%lu]{%s}:%s", t.getTimeAsSecondsDouble(), thrNum, func, buf);

	return len;
}

int Trace::writeWithoutTimestamp(const char *fmt, ...)
{  
        Mutex::AutoLocker l(m);
	va_list args;
	int len;

	if (!enabled)
		return 0;

	va_start(args, fmt);
	len = vfprintf(stdout, fmt, args);
	va_end(args);

	return len;
}

bool Trace::enableFileTrace(const string path)
{
	if (traceFile)
		return false;
	
	if (!create_path(path.c_str())) {
		HAGGLE_ERR("Could not create directory path \'%s\'\n", path.c_str());
		return false;
	}

	string fpath = path + PLATFORM_PATH_DELIMITER + "haggle.log";
	
	traceFile = fopen(fpath.c_str(), "w");

	if (!traceFile) {
		traceFile = NULL;
		HAGGLE_ERR("Could not open haggle log file \'%s\' : %s\n",
			fpath.c_str(), STRERROR(ERRNO));
		return false;
	}
	return true;
}

bool Trace::disableFileTrace()
{
	if (!traceFile)
		return false;

	fclose(traceFile);

	traceFile = NULL;

	return true;
}

LogTrace LogTrace::ltrace;

/*
  NOTE: there is some weird behavior in the statically allocated
  LogTrace object on Android. Due to a bug in Bionic, the constructor is
  called twice, although there is only one statically allocated
  LogTrace object in the class itself.
  
  On regular Linux, with glibc instead of bionic, the constructor is
  only called once.
 
  See android/README for details.
 */
LogTrace::LogTrace(void)
{
	// Sanity check for Android. See NOTE above.  Without this
        // check, the file will be opened twice and the addToLog()
        // function will block on write once the log is flushed to 
        // disk. This will deadlock Haggle.
        if (traceFile)
                return;

	if (!create_path(HAGGLE_DEFAULT_STORAGE_PATH)) {
		fprintf(stderr, "Unable to ensure that the haggle storage path (%s) exists\n",
			HAGGLE_DEFAULT_STORAGE_PATH);
	}
	
	filename = string(HAGGLE_DEFAULT_STORAGE_PATH) + 
		string(PLATFORM_PATH_DELIMITER) + 
		string("trace.log");

	traceFile = fopen(filename.c_str(), "a");

	if (traceFile) {
		addToLog("\n\n%s: Log started, fd=%ld\n", Timeval::now().getAsString().c_str(), fileno(traceFile));
	} else {
		fprintf(stderr,"Unable to open log file!\n");
	}
}

LogTrace::~LogTrace(void)
{
	if (traceFile) {
		addToLog("%s: Log stopped\n", Timeval::now().getAsString().c_str());
		fclose(traceFile);
	}
}

void LogTrace::addToLog(const char *fmt, ...)
{  
        Mutex::AutoLocker l(m);
	va_list args;
	
	if (traceFile) {
		va_start(args, fmt);
		vfprintf(traceFile, fmt, args);
		va_end(args);
	} else {
		HAGGLE_ERR("Attempted to write to log file, but no log file open!\n");
	}
}

#ifdef BENCHMARK

BenchmarkTrace BenchmarkTrace::trace;

BenchmarkLogEntry_t *BenchmarkTrace::bench_log = NULL;

BenchmarkTrace::BenchmarkTrace() : 
	Trace(TRACE_TYPE_LOG, false), bench_log_length(0), bench_log_entries(0)
{
	
}

BenchmarkTrace::~BenchmarkTrace()
{
	
}
					 
int BenchmarkTrace::write(const BenchmarkTraceType_t type, const Timeval &t, unsigned int res)
{
	//printf("logging\n");
	// Are we out of space?
	if (bench_log_length >= bench_log_entries) {
		BenchmarkLogEntry_t *tmp;
		
		// Allocate another 1000 entries:
		tmp = (BenchmarkLogEntry_t *)realloc(bench_log, (bench_log_entries + 1000) * sizeof(BenchmarkLogEntry_t));
		// Check, just in case:
		if(tmp == NULL)
			return -1;

		bench_log = tmp;
		bench_log_entries += 1000; 
	}
	
	// Insert another log entry:
	bench_log[bench_log_length].timestamp = t;
	bench_log[bench_log_length].type = type;
	bench_log[bench_log_length].result = res;
	bench_log_length++;

	return 0;
}

void BenchmarkTrace::dump(unsigned int DataObjects_Attr,
		 unsigned int Nodes_Attr,
		 unsigned int Attr_Num,
		 unsigned int DataObjects_Num)
{
	FILE *fp;
#define MAX_FILEPATH_LEN 256
	char str[MAX_FILEPATH_LEN];
	size_t i;
	size_t strlen = 0;
	char typeChar[5] = { 'I', 'S', 'E', 'R', 'F' };   // init, sqlStart, sqlEnd, result, finish

	// Dummy check: empty logs aren't written to file
	if (bench_log_length == 0)
		return;
	
	strlen += snprintf(str + strlen, MAX_FILEPATH_LEN - strlen, HAGGLE_DEFAULT_STORAGE_PATH);
	strlen += snprintf(str + strlen, MAX_FILEPATH_LEN - strlen, PLATFORM_PATH_DELIMITER);

	// Open file with timestamp of first log entry:
	strlen += snprintf(str + strlen, MAX_FILEPATH_LEN - strlen,
		"benchmark-"
#if defined(OS_MACOSX)
		"macosx-"
#elif  defined(OS_LINUX)
		"linux-"
#elif  defined(WINCE)
		"winmobile-"
#elif  defined(WIN32)
		"windows-"
#else
		"unknown-"
#endif
		"%u-%u-%u-%u-%ld.log", 
		DataObjects_Attr, Nodes_Attr, Attr_Num, DataObjects_Num,
		bench_log[0].timestamp.getSeconds());

	fp = fopen(str, "w");
	
	if (fp) {
                fprintf(fp, 
                        "# DataObjects_Attr=%u Nodes_attr=%u Attr_num=%u DataObjects_num=%u Platform="
#if defined(OS_MACOSX)
                        "Mac OS X"
#elif  defined(OS_LINUX)
                        "Linux"
#elif  defined(WINCE)
                        "Windows mobile"
#elif  defined(WIN32)
                        "Windows"
#else
                        "Unknown"
#endif
                        "\n", 
                        DataObjects_Attr, Nodes_Attr, Attr_Num, DataObjects_Num);
                // Outpout each log entry:
                for (i = 0; i < bench_log_length; i++) {
                        
                        if (bench_log[i].type == BENCH_TYPE_RESULT) {
                                fprintf(fp, "%s %c %u\n", 
					bench_log[i].timestamp.getAsString().c_str(),
					typeChar[bench_log[i].type], 
					bench_log[i].result);
                        } else {
                                fprintf(fp, "%s %c\n", 
					bench_log[i].timestamp.getAsString().c_str(),
					typeChar[bench_log[i].type]);
                        }
                        
                }
                // Close file:
                fclose(fp);
                
	} else {
		HAGGLE_ERR("Attempted to write to log file, but no log file open!\n");
	}
}
#endif /* BENCHMARK */
