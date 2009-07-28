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
#ifndef _RESOURCEMONITOR_H_
#define _RESOURCEMONITOR_H_

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/

class ResourceMonitor;

#include <libcpphaggle/Platform.h>
#include "ResourceManager.h"

#define _IN_RESOURCEMONITOR_H

#if defined(OS_ANDROID)
#include "ResourceMonitorAndroid.h"
#elif defined(OS_LINUX)
#include "ResourceMonitorLinux.h"
#elif defined(OS_MACOSX)
#include "ResourceMonitorMacOSX.h"
#elif defined(OS_WINDOWS_DESKTOP)
#include "ResourceMonitorWindowsXP.h"
#elif defined(OS_WINDOWS_MOBILE)
#include "ResourceMonitorWindowsMobile.h"
#else
#error "Bad OS - Not supported by ResourceMonitor.h"
#endif
#undef _IN_RESOURCEMONITOR_H

#endif
