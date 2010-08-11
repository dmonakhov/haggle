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
#ifndef _HAGGLE_BLOOMFILTER_H
#define _HAGGLE_BLOOMFILTER_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/

class Bloomfilter;

#include "Debug.h"
#include "DataObject.h"

using namespace haggle;

#define DEFAULT_BLOOMFILTER_ERROR_RATE  (0.01)
#define DEFAULT_BLOOMFILTER_CAPACITY    (1000)

/** */
#ifdef DEBUG_LEAKS
class Bloomfilter : public LeakMonitor
#else
class Bloomfilter
#endif
{
public:
	typedef enum {
		BF_TYPE_NORMAL,
		BF_TYPE_COUNTING
	} BloomfilterType_t;
private:	
	static float default_error_rate;
	static unsigned int default_capacity;
	BloomfilterType_t type;
	float error_rate;
	unsigned int capacity;
	const unsigned long init_n;
	union {
		struct bloomfilter *bf;
		struct counting_bloomfilter *cbf;
		unsigned char *raw;
	};
	Bloomfilter(float _error_rate, unsigned int _capacity, struct bloomfilter *bf);
	Bloomfilter(float _error_rate, unsigned int _capacity, struct counting_bloomfilter *cbf);
public:
	/**
		Creates a bloomfilter with the given error rate and capacity.
	*/
	Bloomfilter(BloomfilterType_t _type = BF_TYPE_NORMAL, float error_rate = default_error_rate, 
		    unsigned int capacity = default_capacity);
	
	Bloomfilter(const unsigned char *bf, size_t len);

	/**
		Creates an identical copy of the given bloomfilter.
	*/
	Bloomfilter(const Bloomfilter &bf);
	/**
		Destroys the bloomfilter.
	*/
	~Bloomfilter();
	
	BloomfilterType_t getType() const { return type; }
	
	static void setDefaultErrorRate(float error_rate) { if (error_rate > 0.0) default_error_rate = error_rate; }
	static float getDefaultErrorRate() { return default_error_rate; }
	static void setDefaultCapacity(unsigned int capacity) { if (capacity > 0) default_capacity = capacity; }
	static unsigned int getDefaultCapacity() { return default_capacity; }
	/**
		Adds the given data object to the bloomfilter.
	*/
	void add(const DataObjectRef &dObj);
	/**
		Removes the given data object from the bloomfilter. Only works on 
		counting bloomfilters. For non-counting bloomfilters, this function 
		does nothing.
	*/
	void remove(const DataObjectRef &dObj);
	/**
		Returns true iff the data object is in the bloomfilter.
	*/
	
	bool merge(const Bloomfilter& bf_merge);
	
	bool has(const DataObjectRef &dObj) const;
	/**
		Returns a newly allocated non counting bloomfilter based
		on a counting bloomfilter.
		
		Returns: A non-counting version of the bloomfilter if
		the original bloomfilter was a counting one. If the 
		original bloomfilter was no a counting one, the function 
		simply returns copy of the bloomfilter.
		If an error occurs, the function returns NULL.
	*/
	Bloomfilter *to_noncounting() const;
	/**
		Returns a platform-independent representation of the bloomfilter in a
		Base64 encoded string.
	*/
	string toBase64() const;
	/**
		Sets the bloomfilter to be the bloomfilter represented by the given
		Base64 encoded string.
		
		Can only set a non-counting bloomfilter to a non-counting bloomfilter
		and a counting bloomfilter to a counting bloomfilter.
	*/
	void fromBase64(const string &b64);
	
	/**
		Returns a platform-independent representation of the bloomfilter in a
		Base64 encoded string.
		
		For non-counting bloomfilters, this is exactly the same as toBase64().
		
		For counting bloomfilters, this function returns what the toBase64() 
		function would return for a non-counting bloomfilter with the same 
		inserted data objects as this bloomfilter. This means
	*/
	string toBase64NonCounting() const;
	
	/**
		Returns the number of data objects in the bloomfilter.
	*/
	unsigned long numObjects() const;
	
	/**
	*/
	const unsigned char *getRaw() const;
	/**
	*/
	size_t getRawLen(void) const;
	
	/**
	*/
	bool setRaw(const unsigned char *bf, size_t bf_len);
	
	/**
		Clears the bloomfilter.
	*/
	void reset();
};

#endif /* _HAGGLE_BLOOMFILTER_H */
