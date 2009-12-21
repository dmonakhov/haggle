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

#ifndef _LIBHAGGLE_DEBUG_H
#define _LIBHAGGLE_DEBUG_H

#include <libhaggle/exports.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
int libhaggle_debug_init();
void libhaggle_debug_fini();

HAGGLE_API int libhaggle_trace(int err, const char *func, const char *fmt, ...);

#define LIBHAGGLE_DBG(f, ...) libhaggle_trace((1==0), __FUNCTION__, f, ## __VA_ARGS__)
#define LIBHAGGLE_ERR(f, ...) libhaggle_trace((1==1), __FUNCTION__, f, ## __VA_ARGS__)
#else 
#define LIBHAGGLE_DBG(f, ...)
#define LIBHAGGLE_ERR(f, ...)
#endif /* DEBUG */

#ifdef __cplusplus
}
#endif

#endif /* _LIBHAGGLE_DEBUG_H */
