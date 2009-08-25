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

/** */
#ifdef DEBUG_LEAKS
class Bloomfilter : public LeakMonitor
#else
class Bloomfilter
#endif
{
private:
	float error_rate;
	unsigned int capacity;
	struct bloomfilter *non_counting;
	struct counting_bloomfilter *counting;
public:
	/**
		Creates a bloomfilter with the given error rate and capacity.
	*/
	Bloomfilter(float error_rate, unsigned int capacity, bool counting = false);
	/**
		Creates an identical copy of the given bloomfilter.
	*/
	Bloomfilter(const Bloomfilter &bf);
	/**
		Destroys the bloomfilter.
	*/
	~Bloomfilter();
	
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
	bool has(const DataObjectRef &dObj) const;
	
	/**
		Returns a platform-independent representation of the bloomfilter in a
		Base64 encoded string.
	*/
	string toBase64(void) const;
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
	string toBase64NonCounting(void) const;
	
	/**
		Returns the number of data objects in the bloomfilter.
	*/
	unsigned long countDataObjects(void) const;
	
	/**
	*/
	const char *getRaw(void) const;
	/**
	*/
	unsigned long getRawLen(void) const;
	
	/**
	*/
	void setRaw(const char *bf);
	
	/**
		Clears the bloomfilter.
	*/
	void reset(void);
};

#endif /* _HAGGLE_BLOOMFILTER_H */
