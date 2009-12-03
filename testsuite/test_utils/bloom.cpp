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

#include "testhlp.h"

#include <utils.h>
#include <bloomfilter.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(OS_MACOSX)
#include <stdlib.h>
#elif defined(OS_LINUX)
#include <time.h>
#include <stdlib.h>
#elif defined(OS_WINDOWS)
#include <time.h>
#endif

#define BLOOMFILTER_SIZE 1000
// Birthday principe: this should be _much_ lower than BLOOMFILTER_SIZE to 
// reasonably avoid accidental collisions:
#define NUMBER_OF_DATA_OBJECTS 100
// This must be less than NUMBER_OF_DATA_OBJECTS for the tests to work:
#define NUMBER_OF_DATA_OBJECTS_INSERTED	75
#ifdef COUNTING_BLOOMFILTER
#define NUMBER_OF_DATA_OBJECTS_REMOVED	25
#endif
#define DATA_OBJECT_BYTES				1024
char data_object[NUMBER_OF_DATA_OBJECTS][DATA_OBJECT_BYTES];
long data_object_len[NUMBER_OF_DATA_OBJECTS];

// Returns boolean true/false actually:
int check_for_data_objects(struct bloomfilter *bf)
{
	long	i;
	
	// Check for presence of inserted objects:
	for(i = 0;
		i < NUMBER_OF_DATA_OBJECTS_INSERTED
#ifdef COUNTING_BLOOMFILTER
			- NUMBER_OF_DATA_OBJECTS_REMOVED
#endif
			;
		i++)
		if(	bloomfilter_check(
				bf, 
				data_object[i], 
				data_object_len[i]) == 0)
			return 0;
	
	// Check for non-presence of non-inserted objects:
	for(i = NUMBER_OF_DATA_OBJECTS_INSERTED
#ifdef COUNTING_BLOOMFILTER
			- NUMBER_OF_DATA_OBJECTS_REMOVED
#endif
			; 
		i < NUMBER_OF_DATA_OBJECTS;
		i++)
		if(	bloomfilter_check(
				bf, 
				data_object[i], 
				data_object_len[i]) != 0)
			return 0;
	
	return 1;
}

#if defined(OS_WINDOWS)
int haggle_test_bloom(void)
#else
int main(int argc, char *argv[])
#endif
{
	struct bloomfilter	*my_filter, *filter_copy;
	char				*b64_filter_copy_1, *b64_filter_copy_2;
	long				i,j;
	int					success = (1==1), tmp_succ;
	
	prng_init();
	
	print_over_test_str_nl(0, "Bloomfilter test: ");
	// Create random data objects:
	for(i = 0; i < NUMBER_OF_DATA_OBJECTS; i++)
	{
		data_object_len[i] = 
			((prng_uint8() << 8) | prng_uint8()) % DATA_OBJECT_BYTES;
		for(j = 0; j < data_object_len[i]; j++)
			data_object[i][j] = prng_uint8();
	}
	
	print_over_test_str(1, "Create bloomfilter: ");
	my_filter = bloomfilter_new((float)0.01, 1000);
	
	// Check that it worked
	if(my_filter == NULL)
		return 1;
	
	print_passed();
	print_over_test_str(1, "Add data objects: ");
	// Insert objects:
	for(i = 0; i < NUMBER_OF_DATA_OBJECTS_INSERTED; i++)
		bloomfilter_add(my_filter, data_object[i], data_object_len[i]);
	
#ifdef COUNTING_BLOOMFILTER
	print_passed();
	print_over_test_str(1, "Remove data objects: ");
	// Remove some objects:
	for(i = NUMBER_OF_DATA_OBJECTS_INSERTED - NUMBER_OF_DATA_OBJECTS_REMOVED;
		i < NUMBER_OF_DATA_OBJECTS_INSERTED;
		i++)
		bloomfilter_remove(my_filter, data_object[i], data_object_len[i]);
#endif
	
	print_passed();
	print_over_test_str(1, "Contains those data objects: ");
	// Check filter contents:
	tmp_succ = check_for_data_objects(my_filter);
	success &= tmp_succ;
	print_pass(tmp_succ);
	
	print_over_test_str(1, "Copy: ");
	filter_copy = bloomfilter_copy(my_filter);
	
	// Check that it worked
	if(filter_copy == NULL)
		return 1;
	
	print_passed();
	print_over_test_str(1, "Copy contains data objects: ");
	// Check filter contents:
	tmp_succ = check_for_data_objects(filter_copy);
	success &= tmp_succ;
	print_pass(tmp_succ);
	
	bloomfilter_free(filter_copy);
	
	
	print_over_test_str(1, "To Base64: ");
	b64_filter_copy_1 = bloomfilter_to_base64(my_filter);
	
	// Check that it worked
	if(b64_filter_copy_1 == NULL)
		return 1;
	
	print_passed();
	print_over_test_str(1, "From Base64: ");
	filter_copy = 
		base64_to_bloomfilter(
			b64_filter_copy_1, 
			strlen(b64_filter_copy_1));
	
	// Check that it worked
	if(filter_copy == NULL)
		return 1;
	
	print_passed();
	print_over_test_str(1, "To Base64 match: ");
	b64_filter_copy_2 = bloomfilter_to_base64(filter_copy);
	
	// Check that it worked
	if(b64_filter_copy_2 == NULL)
		return 1;
	
	// Check that the lengths are the same:
	if(strlen(b64_filter_copy_2) != strlen(b64_filter_copy_1))
		tmp_succ = (1==0);
	else{
		// Check that they are equal:
		if(strcmp(b64_filter_copy_2, b64_filter_copy_1) != 0)
			tmp_succ = (1==0);
	}
	
	success &= tmp_succ;
	print_pass(tmp_succ);
	print_over_test_str(1, "Copy contains data objects: ");
	// Check filter contents:
	tmp_succ = check_for_data_objects(filter_copy);
	success &= tmp_succ;
	print_pass(tmp_succ);

	print_over_test_str(1, "Release: ");
	bloomfilter_free(filter_copy);
	
	bloomfilter_free(my_filter);
	
	print_passed();
	print_over_test_str(1, "Total: ");
	// Success?
	return (success?0:1);
}
