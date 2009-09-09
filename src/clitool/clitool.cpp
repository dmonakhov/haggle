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
#include <libcpphaggle/Timeval.h>

using namespace haggle;

#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#ifdef OS_UNIX
#include <unistd.h>
#endif

#include "prng.h"

pthread_mutex_t mutex;
pthread_cond_t cond;

void on_interest_list(haggle_dobj_t *dobj, void *arg)
{	
        // Loop through and list interests:
        bool not_done = true;
        int i = 0;

        while(not_done)
        {
                attribute *a;
		
                a = haggle_attributelist_get_attribute_n(dobj->al, i);

                if(a == NULL)
                {
                        not_done = false;
                }else{
                        printf("Interest: %s=%s:%ld\n",
                               haggle_attribute_get_name(a),
                               haggle_attribute_get_value(a),
                               haggle_attribute_get_weight(a));
                        i++;
                }
        }

        pthread_cond_signal(&cond);
}

int main(int argc, char *argv[])
{
	int retval = 1;
	long i;
	haggle_handle_t haggle_;
	char *progname = NULL;
	enum {
		command_add_interest,
		command_delete_interest,
		command_get_interests,
		command_new_dataobject,
		command_blacklist,
		command_shutdown,
		command_fail,
		command_none
	} command = command_none;
	char *command_parameter = NULL;
	char *attr_name = NULL;
	char *attr_value = NULL;
	long attr_weight = 1;
	
        pthread_mutex_init(&mutex, NULL);
        pthread_cond_init(&cond, NULL);

	// Parse command-line arguments:
	for(i = 1; i < argc; i++)
	{
		if(strcmp(argv[i], "-p") == 0)
		{
			i++;
			progname = argv[i];
		}else if(command == command_none && strcmp(argv[i], "add") == 0)
		{
			command = command_add_interest;
			i++;
			if(i < argc)
				command_parameter = argv[i];
		}else if(command == command_none && strcmp(argv[i], "del") == 0)
		{
			command = command_delete_interest;
			i++;
			if(i < argc)
				command_parameter = argv[i];
		}else if(command == command_none && strcmp(argv[i], "new") == 0)
		{
			command = command_new_dataobject;
			i++;
			if(i < argc)
				command_parameter = argv[i];
		}else if(command == command_none && strcmp(argv[i], "get") == 0)
		{
			command = command_get_interests;
		}else if(command == command_none && strcmp(argv[i], "blacklist") == 0)
		{
			command = command_blacklist;
			i++;
			if(i < argc)
				command_parameter = argv[i];
		}else if(command == command_none && strcmp(argv[i], "shutdown") == 0)
		{
			command = command_shutdown;
		}else{
			printf("Unrecognized parameter: %s\n", argv[i]);
			command = command_fail;
		}
	}
	
	if(progname == NULL)
	{
		progname = (char *) "Haggle command line tool";
	}
	
	switch(command)
	{
		case command_add_interest:
		case command_delete_interest:
		case command_new_dataobject:
			// Parse the argument:
			if(command_parameter != NULL)
			{
				attr_name = command_parameter;
				attr_value = command_parameter;
				i = 0;
				while(attr_name[i] != '=' && attr_name[i] != '\0')
					i++;
				if(attr_name[i] != '\0')
				{
					attr_name[i] = '\0';
					i++;
					
					attr_value = &(attr_name[i]);
					i = 0;
					while(attr_value[i] != ':' && attr_value[i] != '\0')
						i++;
					if(attr_value[i] == ':')
					{
						attr_value[i] = '\0';
						i++;
						attr_weight = atol(&(attr_value[i]));
					}
		// Only break out of this switch statement if there was an argument,
		// and it has been correctly parsed:
		break;
				}
			}
			printf("Unable to parse attribute.\n");
			// Intentional fall-through:
		case command_none:
		case command_fail:
			printf(
"\n"
"Usage:\n"
"clitool [-p <name of program>] add <attribute>\n"
"clitool [-p <name of program>] del <attribute>\n"
"clitool [-p <name of program>] new <attribute>\n"
"clitool [-p <name of program>] get\n"
"clitool [-p <name of program>] blacklist <Ethernet MAC address>\n"
"\n"
"-p          Allows this program to masquerade as another.\n"
"add         Tries to add <attribute> to the list of interests for this\n"
"            application.\n"
"del         Tries to remove <attribute> from the list of interests for this\n"
"            application.\n"
"new         Creates and publishes a new data object with the given attribute\n"
"get         Tries to retrieve all interests for this application.\n"
"blacklist   Toggles blacklisting of the given interface.\n"
"shutdown    Terminates haggle\n"
"\n"
"Attributes are specified as such: <name>=<value>[:<weight>]. Name and value\n"
"are text strings, and weight is an optional integer. Name can of course not\n"
"include an '=' character.\n");
			
			return 1;
		break;
		
		default:
		break;
	}
	
	set_trace_level(1);
	
	// Find Haggle:
        retval = haggle_handle_get(progname, &haggle_);

	if(retval != HAGGLE_NO_ERROR)
		goto fail_haggle;
	
	switch(command)
	{
		case command_add_interest:
			// Add interest:
			haggle_ipc_add_application_interest_weighted(
				haggle_, 
				attr_name, 
				attr_value,
				attr_weight);
			sleep(2);
		break;
		
		case command_delete_interest:
			// Remove interest:
			haggle_ipc_remove_application_interest(
				haggle_,
				attr_name, 
				attr_value);
			sleep(2);
		break;
		
		case command_new_dataobject:
		{
			struct dataobject *dObj;
			Timeval	now = Timeval::now();
			
			// New data object:
			dObj = haggle_dataobject_new();
			
			// Set create time:
			haggle_dataobject_set_createtime(dObj, now.getTimevalStruct());
			// Add attribute:
			haggle_dataobject_add_attribute(
				dObj, 
				attr_name, 
				attr_value);
			// Make sure the data object is permanent:
			haggle_dataobject_set_flags(
				dObj, 
				DATAOBJECT_FLAG_PERSISTENT);
			
			// Publish:
			haggle_ipc_publish_dataobject(haggle_, dObj);
		}
		break;
		
		case command_get_interests:
		{
			haggle_ipc_register_event_interest(haggle_, LIBHAGGLE_EVENT_INTEREST_LIST, on_interest_list);
			pthread_mutex_lock(&mutex);
			
			// Get interests:
			haggle_ipc_get_application_interests_async(haggle_);
			haggle_event_loop_run_async(haggle_);
			pthread_cond_wait(&cond, &mutex);
			pthread_mutex_unlock(&mutex);
			haggle_event_loop_stop(haggle_);

		}
		break;
		
		case command_blacklist:
		{
			struct dataobject *dObj;
			char modified_do[256];
			
			sprintf(
				modified_do, 
				"<Haggle persistent=\"no\">"
				"<Attr name=\"Connectivity\">Blacklist</Attr>"
				"<Connectivity>"
					"<Blacklist type=\"ethernet\">%s</Blacklist>"
				"</Connectivity>"
				"</Haggle>",
				command_parameter);
			
			// New data object:
			dObj = 
				haggle_dataobject_new_from_raw(
					modified_do, 
					strlen(modified_do));
			
			// Publish:
			haggle_ipc_publish_dataobject(haggle_, dObj);
		}
		break;

		case command_shutdown:
		{
			haggle_ipc_shutdown(haggle_);
		}
		break;	
			
		// Shouldn't be able to get here:
		default:
		break;
	}

	retval = 0;
	
	// Release the haggle handle:
	haggle_handle_free(haggle_);
fail_haggle:
        pthread_cond_destroy(&cond);
        pthread_mutex_destroy(&mutex);
	return retval;
}
