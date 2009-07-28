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

#include <libhaggle/haggle.h>
#include <time.h>
#if defined(OS_WINDOWS)
#include <libcpphaggle/Timeval.h>
using namespace haggle;
#endif

#if defined(OS_MACOSX)
#include <stdlib.h>
#endif

void prng_init(void)
{
#if defined(OS_MACOSX)
	// No need for initialization
#elif defined(OS_LINUX)
	srandom(time(NULL));
#elif defined(OS_WINDOWS)
	srand(Timeval::now().getMicroSeconds());
#endif
}

unsigned char prng_uint8(void)
{
#if defined(OS_MACOSX)
	return arc4random() & 0xFF;
#elif defined(OS_LINUX)
	return random() & 0xFF;
#elif defined(OS_WINDOWS)
	return rand() & 0xFF;
#endif
}

unsigned long prng_uint32(void)
{
	return
		(((unsigned long) prng_uint8()) << 24) |
		(((unsigned long) prng_uint8()) << 16) |
		(((unsigned long) prng_uint8()) << 8) |
		(((unsigned long) prng_uint8()) << 0);
}

