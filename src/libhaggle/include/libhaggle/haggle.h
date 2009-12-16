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
#ifndef _LIBHAGGLE_HAGGLE_H
#define _LIBHAGGLE_HAGGLE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
	These are the error codes libhaggle returns. Please note that they are all 
	negative because some functions either return a positive result, or an error
	code.
*/
enum {
	// A generic haggle error
	HAGGLE_ERROR = -100,
	// Unable to allocate memory
	HAGGLE_ALLOC_ERROR,
	// A socket function failed.
	HAGGLE_SOCKET_ERROR,
	// Unable to register with haggle.
	HAGGLE_REGISTRATION_ERROR,
	// Haggle already had an application registered by that name.
	HAGGLE_BUSY_ERROR,
	// Parameter error.
	HAGGLE_PARAM_ERROR,
	// Internal error.
	HAGGLE_INTERNAL_ERROR,
	// Something wrong with the event loop.
	HAGGLE_EVENT_LOOP_ERROR,	
	// No event handlers have been registered yet.
	HAGGLE_EVENT_HANDLER_ERROR,
	// Something wrong with reading or writing a file
	HAGGLE_FILE_ERROR,
	// A bad data object
	HAGGLE_DATAOBJECT_ERROR,
	// WSAStartup (windows only) failed.
	HAGGLE_WSA_ERROR,
        // The handle was bad
        HAGGLE_HANDLE_ERROR,
        // The metadata was bad
        HAGGLE_METADATA_ERROR,
	// No error
	HAGGLE_NO_ERROR = 0,
};

#include "platform.h"
#include "exports.h"
#include "interface.h"
#include "attribute.h"
#include "dataobject.h"
#include "node.h"
#include "ipc.h"

/**
	Similar to errno, this variable is sometimes set to reflect what error 
	occured in a called function.
*/
extern HAGGLE_API int libhaggle_errno;

/**
	TODO: implement error functionality. Current
	error mechanism is the error codes above?
	
	Returns: 
*/
HAGGLE_API int haggle_get_error();

/**
	This functions makes it possible to disable output from haggle based on the
	kind of output.
	
	The level is initially 2.
	
	Level:		What is displayed:
	2			Debugging output (If compiled with debugging on) and errors
	1			Only errors
	0			Nothing
*/
HAGGLE_API void set_trace_level(int level);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif

#endif /* _LIBHAGGLE_HAGGLE_H */
