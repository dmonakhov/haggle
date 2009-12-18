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

#include "error.h"
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
