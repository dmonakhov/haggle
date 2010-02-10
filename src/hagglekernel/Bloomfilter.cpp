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
	type(_counting ? BF_TYPE_COUNTING : BF_TYPE_NORMAL),
	error_rate(_error_rate),
	capacity(_capacity),
	init_n(0),
	raw(NULL)
{
	if (_counting) {
		cbf = counting_bloomfilter_new(error_rate, capacity);
	} else {
		bf = bloomfilter_new(error_rate, capacity);
	}
}
Bloomfilter::Bloomfilter(float _error_rate, unsigned int _capacity, struct bloomfilter *_bf) :
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_BLOOMFILTER),
#endif
	type(BF_TYPE_NORMAL),
	error_rate(_error_rate),
	capacity(_capacity),
	init_n(_bf->n),
	bf(_bf)
{
}

Bloomfilter::Bloomfilter(float _error_rate, unsigned int _capacity, struct counting_bloomfilter *_cbf) :
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_BLOOMFILTER),
#endif
	type(BF_TYPE_COUNTING),
	error_rate(_error_rate),
	capacity(_capacity),
	init_n(_cbf->n),
	cbf(_cbf)
{
}
Bloomfilter::Bloomfilter(const unsigned char *_bf, size_t len) :
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_BLOOMFILTER),
#endif
	type(BF_TYPE_NORMAL),
	error_rate(0),
	capacity(0),
	init_n(((struct bloomfilter *)_bf)->n),
	raw(NULL)
{
	if (BLOOMFILTER_TOT_LEN((struct bloomfilter *)_bf) == len) {
		bf = bloomfilter_copy((struct bloomfilter*)_bf);
		type = BF_TYPE_NORMAL;
		HAGGLE_DBG("Bloomfilter is non-counting and contains %lu objects\n", bloomfilter_get_n(bf)); 
	} else if (COUNTING_BLOOMFILTER_TOT_LEN((struct counting_bloomfilter *)_bf) == len){
		cbf = counting_bloomfilter_copy((struct counting_bloomfilter*)_bf);
		type = BF_TYPE_COUNTING;
		HAGGLE_DBG("Bloomfilter is counting and contains %lu objects\n", counting_bloomfilter_get_n(cbf)); 
	} else {
		HAGGLE_ERR("bloomfilter is neither counting nor non-counting\n");
	}
}

Bloomfilter::Bloomfilter(const Bloomfilter &_bf) :
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_BLOOMFILTER),
#endif
	type(_bf.type),
	error_rate(_bf.error_rate),
	capacity(_bf.capacity),
	init_n(_bf.init_n),
	raw(NULL)
{
	if (type == BF_TYPE_NORMAL) {
		bf = bloomfilter_copy(_bf.bf);
	} else {
		cbf = counting_bloomfilter_copy(_bf.cbf);
	} 
}

Bloomfilter::~Bloomfilter()
{
	if (raw) {
		if (type == BF_TYPE_NORMAL)
			bloomfilter_free(bf);
		else
			counting_bloomfilter_free(cbf);
	}
}

void Bloomfilter::add(const DataObjectRef &dObj)
{
	if (type == BF_TYPE_NORMAL) {
		bloomfilter_add(bf, (const char *)dObj->getId(), DATAOBJECT_ID_LEN);
	} else {
		counting_bloomfilter_add(cbf, (const char *)dObj->getId(), DATAOBJECT_ID_LEN);
	}
}

void Bloomfilter::remove(const DataObjectRef &dObj)
{
	if (type == BF_TYPE_COUNTING) {
		counting_bloomfilter_remove(cbf, (const char *)dObj->getId(), DATAOBJECT_ID_LEN);
	} 
}

bool Bloomfilter::merge(const Bloomfilter& bf_merge)
{
	if (type == BF_TYPE_NORMAL && bf_merge.type == BF_TYPE_NORMAL){
		// Cannot merge a counting bloomfilter
		bool res = bloomfilter_merge(bf, bf_merge.bf) == MERGE_RESULT_OK;
		
		bf->n -= init_n;
		return res;
	} 
	return false;
}

bool Bloomfilter::has(const DataObjectRef &dObj) const
{
	if (type == BF_TYPE_NORMAL) {
		return bloomfilter_check(bf, (const char *)dObj->getId(), DATAOBJECT_ID_LEN) != 0;
	} else {
		return counting_bloomfilter_check(cbf, (const char *)dObj->getId(), DATAOBJECT_ID_LEN) != 0;
	}
}

Bloomfilter *Bloomfilter::to_noncounting() const
{
	struct bloomfilter *bf_copy;

	if (type == BF_TYPE_NORMAL) {
		return new Bloomfilter(*this);	
	} 

	bf_copy = counting_bloomfilter_to_noncounting(cbf);

	if (!bf_copy)
		return NULL;

	return new Bloomfilter(error_rate, capacity, bf_copy);
}

string Bloomfilter::toBase64(void) const
{
	string retval;
	
	if (type == BF_TYPE_NORMAL) {
		char *tmp = bloomfilter_to_base64(bf);
		
		if (tmp != NULL) {
			retval = tmp;
			free(tmp);
		}
	} else  {
		char *tmp = counting_bloomfilter_to_base64(cbf);
		
		if (tmp != NULL) {
			retval = tmp;
			free(tmp);
		}
	} 
	
	return retval;
}

void Bloomfilter::fromBase64(const string &b64)
{
	// FIXME: how to determine if the b64 string contains a counting or 
	// non-counting bf? The base64_to_* functions don't.
	if (type == BF_TYPE_NORMAL) {
		struct bloomfilter *tmp;
		tmp = base64_to_bloomfilter(b64.c_str(), b64.length());
		if (tmp == NULL) {
			HAGGLE_ERR("Bloomfilter assignment failed!\n");
		} else {
			bloomfilter_free(bf);
			bf = tmp;
		}
	} else {
		struct counting_bloomfilter *tmp;
		tmp = base64_to_counting_bloomfilter(b64.c_str(), b64.length());
		if (tmp == NULL) {
			HAGGLE_ERR("Bloomfilter assignment failed!\n");
		} else {
			counting_bloomfilter_free(cbf);
			cbf = tmp;
		}
	} 
}

string Bloomfilter::toBase64NonCounting(void) const
{
	string retval;
	
	if (type == BF_TYPE_NORMAL) {
		return toBase64();
	} else {
		char *tmp = counting_bloomfilter_to_noncounting_base64(cbf);
		if (tmp != NULL) {
			retval = tmp;
			free(tmp);
		}
	} 
	return retval;
}

unsigned long Bloomfilter::numObjects(void) const
{
	if (type == BF_TYPE_NORMAL) {
		return bloomfilter_get_n(bf);
	} else {
		return counting_bloomfilter_get_n(cbf);
	}
}

const unsigned char *Bloomfilter::getRaw(void) const
{
	return raw;
}

size_t Bloomfilter::getRawLen(void) const
{
	if (type == BF_TYPE_NORMAL) {
		return (unsigned long)BLOOMFILTER_TOT_LEN(bf);
	} else {
		return (unsigned long)COUNTING_BLOOMFILTER_TOT_LEN(cbf);
	}
}

void Bloomfilter::setRaw(const unsigned char *_bf)
{
	if (type == BF_TYPE_NORMAL) {
		if (BLOOMFILTER_TOT_LEN(bf) != BLOOMFILTER_TOT_LEN((struct bloomfilter *)_bf)) {
			HAGGLE_ERR("Old and new bloomfilter differ in length!\n");
		} else {
			memcpy(bf, _bf, BLOOMFILTER_TOT_LEN(bf));
		}
	} else {
		if (COUNTING_BLOOMFILTER_TOT_LEN(cbf) != COUNTING_BLOOMFILTER_TOT_LEN((struct bloomfilter *)_bf)) {
			HAGGLE_ERR("Old and new bloomfilter differ in length!\n");
		} else {
			memcpy(cbf, _bf, COUNTING_BLOOMFILTER_TOT_LEN(cbf));
		}
	}
}

void Bloomfilter::reset()
{
	if (!raw)
		return;
	
	if (type == BF_TYPE_NORMAL) {
		bloomfilter_free(bf);
		bf = bloomfilter_new(error_rate, capacity);
	} else {
		counting_bloomfilter_free(cbf);
		cbf = counting_bloomfilter_new(error_rate, capacity);
	}
}
