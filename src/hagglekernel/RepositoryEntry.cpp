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
#include "RepositoryEntry.h"

int RepositoryEntry::init(const char* _authority, const char* _key, const char* _value, unsigned int _id) 
{
	if (!_authority)
		return -1;
	
	authority = (char *)malloc(strlen(_authority)+1);
	
	if (!authority)
		return -1;
	
	strcpy(authority, _authority);
	
	if (_key) {
		key = (char *)malloc(strlen(_key)+1);
		
		if (!key) {
			free(authority);
			return -1;
		}
		strcpy(key, _key);
	} 
	
	if (_value) {
		value = (char *)malloc(strlen(_value)+1);
		
		if (!value) {
			free(authority);
			
			if (key)
				free(key);
		
			return -1;
		}
		
		strcpy(value, _value);
	} 
	return 0;
}

RepositoryEntry::RepositoryEntry(const char* _authority, const char* _key, const char* _value, unsigned int _id) : 
	authority(NULL), key(NULL), value(NULL), id(_id)
{
	if (init(_authority, _key, _value, _id) == -1) {
		fprintf(stderr, "Could not initialize repository entry\n");
	}
}

RepositoryEntry::~RepositoryEntry()
{
	if (authority) 
		free(authority);

	if (key) 
		free(key);

	if (value) 
		free(value);
}


bool operator==(const RepositoryEntry& e1, const RepositoryEntry& e2)
{
        return (strcmp(e1.authority, e2.authority) == 0 && 
                strcmp(e1.key, e2.key) == 0 && 
                strcmp(e1.value, e2.value) == 0);
}
