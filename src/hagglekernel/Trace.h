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
#ifndef _TRACE_H
#define _TRACE_H

#include <stdio.h>

#include <libcpphaggle/Platform.h>
#include <libcpphaggle/Timeval.h>
#include <libcpphaggle/Mutex.h>
#include <libcpphaggle/String.h>

#include "Utility.h"

using namespace haggle;

typedef enum {
	TRACE_TYPE_DEBUG,
	TRACE_TYPE_ERROR,
	TRACE_TYPE_LOG,
	TRACE_TYPE_BENCHMARK,
	_NUM_TRACE_TYPES
} TraceType_t;

#if defined(OS_UNIX) 
#define __TRACE_FUNCTION__ __PRETTY_FUNCTION__ 
#else
#define __TRACE_FUNCTION__ __FUNCTION__ 
#endif

/*
	Currently, these MACROs only support one trace object at a time, and this object 
	is set to the first one created. In the future we might think about how to 
	be able to support multiple trace objects.
*/
	
#define HAGGLE_TRACE_FILE (Trace::trace.getTraceFile())
#define HAGGLE_TRACE(type, format, ...) (Trace::trace.write(type, __TRACE_FUNCTION__, format, ## __VA_ARGS__))

#define HAGGLE_ERR(format, ...) HAGGLE_TRACE(TRACE_TYPE_ERROR, format, ## __VA_ARGS__)
#define HAGGLE_LOG(format, ...) HAGGLE_TRACE(TRACE_TYPE_LOG, format, ## __VA_ARGS__)
#ifdef DEBUG
#define HAGGLE_DBG(format, ...) HAGGLE_TRACE(TRACE_TYPE_DEBUG, format, ## __VA_ARGS__)
#else
#define HAGGLE_DBG(format, ...) {}
#endif /* DEBUG */


#ifndef TRACE_ENABLE_PRINTF
// This is to remove the use of printf from the code. HAGGLE_DBG should be used instead.
#define printf(format, ...) (Trace::trace.writeWithoutTimestamp(format, ## __VA_ARGS__))
#endif

/** */
class Trace {
	TraceType_t type;
        Mutex m;
protected:
	FILE *traceFile;
	Timeval startTime;
	bool enabled;
	static bool stdout_enabled;

	/*
	 Do not allow anyone to create trace objects of their own. See below.
	 */
	Trace(TraceType_t type = TRACE_TYPE_DEBUG, bool enabled = true);
	~Trace();
public:
	/*
	 We use a static trace object to be able to use the trace macros above. In the future we might try to
	 support multiple trace objects. Therefore the use of a static trace object should be seen as a temporary
	 solution.
	 */
	static Trace trace;
	static void disableStdout() { stdout_enabled = false; }
	int write(const TraceType_t type, const char *func, const char *fmt, ...);
	int writeWithoutTimestamp(const char *fmt, ...);
	bool enableFileTrace(const string path = DEFAULT_LOG_STORAGE_PATH);
	bool disableFileTrace();
	void enable() { enabled = true; }
	void disable() { enabled = false; }
	FILE *getTraceFile() const { return traceFile; }
};

#define LOG_ADD(format, ...) (LogTrace::ltrace.addToLog(format, ## __VA_ARGS__))

class LogTrace {
        Mutex m;
	string filename;
	FILE *traceFile;
	LogTrace(void);
	~LogTrace(void);
public:
	static LogTrace ltrace;
	static bool init(void);
	static void fini(void);
	bool open(const string name);
	void close(void);
	string getFile(void) { return filename; }
	void addToLog(const char *fmt, ...);
};

#ifdef BENCHMARK

#define BENCH_TRACE(type, time, res) BenchmarkTrace::trace.write(type, time, res)
#define BENCH_TRACE_DUMP(do_attr, node_attr, attr_num, do_num) BenchmarkTrace::trace.dump(do_attr, node_attr, attr_num, do_num)

// For benchmarking
typedef enum {
	BENCH_TYPE_INIT,
	BENCH_TYPE_QUERYSTART,
	BENCH_TYPE_QUERYEND,
	BENCH_TYPE_RESULT,
	BENCH_TYPE_END,
	_NUM_BENCH_TYPES
} BenchmarkTraceType_t;

typedef struct {
	BenchmarkTraceType_t type;
	Timeval timestamp;
	unsigned int result;
} BenchmarkLogEntry_t;

/** */
class BenchmarkTrace : public Trace {	
	static BenchmarkLogEntry_t *bench_log;
	size_t bench_log_length;
	size_t bench_log_entries; 
	BenchmarkTrace();
	~BenchmarkTrace();
public:
	static BenchmarkTrace trace;
	// For benchmarking
	int write(const BenchmarkTraceType_t type, const Timeval& t, unsigned int res);
	void dump(unsigned int DataObjects_Attr, unsigned int Nodes_Attr,
		  unsigned int Attr_Num, unsigned int DataObjects_Num);
	
};
		       
#endif /* BENCHMARK */

#endif /* _TRACE_H */
