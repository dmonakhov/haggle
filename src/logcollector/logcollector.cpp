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

#include <string.h>
#ifndef OS_WINDOWS
#include <signal.h>
#endif

#if defined(OS_WINDOWS_MOBILE)
int wmain()
{
#else
int main(int argc, char *argv[])
{
#endif
	int retval = 1;
	haggle_handle_t haggle_;
	
	// Find Haggle:
	if(haggle_handle_get("Haggle Logfile collector", &haggle_) != HAGGLE_NO_ERROR)
		goto fail_haggle;
	
	// Add the email attributes to our interests:
	haggle_ipc_add_application_interest(
		haggle_, 
		"Log file", 
		"Trace");

	retval = 0;
	
	// Release the haggle handle:
	haggle_handle_free(haggle_);
fail_haggle:
	return retval;
}
