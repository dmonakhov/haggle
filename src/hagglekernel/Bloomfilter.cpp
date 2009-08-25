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

#include "Bloomfilter.h"
#include <haggleutils.h>
#include "Debug.h"
#include "Trace.h"

Bloomfilter::Bloomfilter(float _error_rate, unsigned int _capacity, bool _counting) :
#ifdef DEBUG_LEAKS
		LeakMonitor(LEAK_TYPE_BLOOMFILTER),
#endif
                error_rate(_error_rate),
                capacity(_capacity)
{
	if (_counting) {
		non_counting = NULL;
		counting = counting_bloomfilter_new(error_rate, capacity);
	} else {
		non_counting = bloomfilter_new(error_rate, capacity);
		counting = NULL;
	}
}

Bloomfilter::Bloomfilter(const Bloomfilter &bf) :
#ifdef DEBUG_LEAKS
                LeakMonitor(LEAK_TYPE_BLOOMFILTER),
#endif
                error_rate(bf.error_rate),
                capacity(bf.capacity)
{
	if (bf.non_counting != NULL) {
		non_counting = bloomfilter_copy(bf.non_counting);
		counting = NULL;
	} else if (counting != NULL) {
		non_counting = NULL;
		counting = counting_bloomfilter_copy(bf.counting);
	} else {
		HAGGLE_ERR("Tried to add to bloomfilter which is neither counting or "
                           "non-counting!\n");
	}
}

Bloomfilter::~Bloomfilter()
{
	if (non_counting != NULL)
		bloomfilter_free(non_counting);
	if (counting != NULL)
		counting_bloomfilter_free(counting);
}

void Bloomfilter::add(const DataObjectRef &dObj)
{
	if (non_counting != NULL) {
		bloomfilter_add(non_counting, (const char *)dObj->getId(), DATAOBJECT_ID_LEN);
	} else if (counting != NULL) {
		counting_bloomfilter_add(counting, (const char *) dObj->getId(), DATAOBJECT_ID_LEN);
	} else{
		HAGGLE_ERR("Tried to add to bloomfilter which is neither counting or non-counting!\n");
	}
}

void Bloomfilter::remove(const DataObjectRef &dObj)
{
	if (non_counting != NULL){
		// Ignored
	} else if (counting != NULL) {
		counting_bloomfilter_remove(counting, (const char *) dObj->getId(), DATAOBJECT_ID_LEN);
	} else {
		HAGGLE_ERR("Tried to remove from bloomfilter which is neither counting or non-counting!\n");
	}
}

bool Bloomfilter::has(const DataObjectRef &dObj)
{
	if (non_counting != NULL) {
		return bloomfilter_check(non_counting, (const char *)dObj->getId(), DATAOBJECT_ID_LEN) != 0;
	} else if (counting != NULL) {
		return counting_bloomfilter_check(counting, (const char *) dObj->getId(), DATAOBJECT_ID_LEN) != 0;
	}
	
	HAGGLE_ERR("Tried to check bloomfilter which is neither counting or non-counting!\n");
	
	return false;
}

string Bloomfilter::toBase64(void)
{
	string retval;
	
	if (non_counting != NULL) {
		char *tmp = bloomfilter_to_base64(non_counting);
		if (tmp != NULL) {
			retval = tmp;
			free(tmp);
		}
	} else if (counting != NULL) {
		char *tmp = counting_bloomfilter_to_base64(counting);
		
		if (tmp != NULL) {
			retval = tmp;
			free(tmp);
		}
	} else {
		HAGGLE_ERR("Tried to convert bloomfilter which is neither counting or non-counting!\n");
	}
	return retval;
}

void Bloomfilter::fromBase64(const string &b64)
{
	// FIXME: how to determine if the b64 string contains a counting or 
	// non-counting bf? The base64_to_* functions don't.
	if (non_counting != NULL) {
		struct bloomfilter *tmp;
		tmp = base64_to_bloomfilter(b64.c_str(), b64.length());
		if (tmp == NULL) {
			HAGGLE_ERR("Bloomfilter assignment failed!\n");
		} else {
			bloomfilter_free(non_counting);
			non_counting = tmp;
		}
	} else if (counting != NULL) {
		struct counting_bloomfilter *tmp;
		tmp = base64_to_counting_bloomfilter(b64.c_str(), b64.length());
		if (tmp == NULL) {
			HAGGLE_ERR("Bloomfilter assignment failed!\n");
		} else {
			counting_bloomfilter_free(counting);
			counting = tmp;
		}
	} else {
		HAGGLE_ERR("Tried to assign bloomfilter which is neither counting or non-counting!\n");
	}
}

string Bloomfilter::toBase64NonCounting(void)
{
	string retval;
	
	if (non_counting != NULL) {
		return toBase64();
	} else if (counting != NULL) {
		char *tmp = counting_bloomfilter_to_noncounting_base64(counting);
		if (tmp != NULL) {
			retval = tmp;
			free(tmp);
		}
	} else {
		HAGGLE_ERR("Tried to assign bloomfilter which is neither counting or non-counting!\n");
	}
	return retval;
}

unsigned long Bloomfilter::countDataObjects(void)
{
	if (non_counting != NULL) {
		return bloomfilter_get_n(non_counting);
	} else if (counting != NULL) {
		return counting_bloomfilter_get_n(counting);
	}
	
	HAGGLE_ERR("Tried to get the number of data objects in bloomfilter which is neither counting or non-counting!\n");
	
	return 0;
}

const char *Bloomfilter::getRaw(void)
{
	if (non_counting != NULL) {
		return (char *)non_counting;
	} else if(counting != NULL) {
		return (char *)counting;
	} 
		
	HAGGLE_ERR("Tried to get raw bloomfilter which is neither counting or non-counting!\n");

	return NULL;
}

unsigned long Bloomfilter::getRawLen(void)
{
	if (non_counting != NULL) {
		return (unsigned long)BLOOMFILTER_TOT_LEN(non_counting);
	} else if (counting != NULL) {
		return (unsigned long)COUNTING_BLOOMFILTER_TOT_LEN(counting);
	}
	
	HAGGLE_ERR("Tried to get length of bloomfilter which is neither counting or non-counting!\n");
	
	return 0;
}

void Bloomfilter::setRaw(const char *bf)
{
	if (non_counting != NULL) {
		if (BLOOMFILTER_TOT_LEN(non_counting) != BLOOMFILTER_TOT_LEN((struct bloomfilter *)bf)) {
			HAGGLE_ERR("Old and new bloomfilter differ in length!\n");
		} else {
			memcpy(non_counting, bf, BLOOMFILTER_TOT_LEN(non_counting));
		}
	} else if (counting != NULL) {
		if (COUNTING_BLOOMFILTER_TOT_LEN(counting) != COUNTING_BLOOMFILTER_TOT_LEN((struct bloomfilter *)bf)) {
			HAGGLE_ERR("Old and new bloomfilter differ in length!\n");
		} else {
			memcpy(counting, bf, COUNTING_BLOOMFILTER_TOT_LEN(counting));
		}
	} else {
		HAGGLE_ERR("Tried to set bloomfilter which is neither counting or non-counting!\n");
	}
}

void Bloomfilter::reset(void)
{
	if (non_counting != NULL) {
		non_counting = bloomfilter_new(error_rate, capacity);
		counting = NULL;
	} else if (counting != NULL) {
		non_counting = NULL;
		counting = counting_bloomfilter_new(error_rate, capacity);
	} else {
		HAGGLE_ERR("Tried to reset bloomfilter which is neither counting or non-counting!\n");
	}
}
