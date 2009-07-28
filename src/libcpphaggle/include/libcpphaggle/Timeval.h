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

#ifndef _TIMEVAL_H
#define _TIMEVAL_H

#include <haggleutils.h>
#include "String.h"

namespace haggle {
/**
	Conveniance class to work with struct timeval.
	Timeval's behavior for negative times is not really defined, so beware.
	However, it is perfectly fine to substract a smaller Timeval from a larger one.
*/
class Timeval {
	struct timeval t;
public:
	static Timeval now();
	Timeval(const Timeval &tv);
	Timeval(const struct timeval& t);
	Timeval(const long seconds = 0, const long microseconds = 0);
	Timeval(const string str);
	/*
	  Set Timeval to current time.
	 */
	Timeval& setNow();
	/*
	  Set Timeval to zero.
	 */
	Timeval& zero() { t.tv_sec = 0; t.tv_usec = 0; return *this; }
	/*
	  Set Timeval given a C-struct timeval.
	 */
	Timeval& set(const struct timeval &_t) { t = _t; return *this; }
	/*
	  Set Timeval given a seconds and microseconds.
	 */
	Timeval& set(const long seconds, const long microseconds) { t.tv_sec = seconds; t.tv_usec = microseconds; return *this; }
	/*
	  Set Timeval given a seconds and microseconds double.
	 */
	Timeval& set(const double seconds) { t.tv_sec = (long) seconds; t.tv_usec = (long)((seconds - (double)t.tv_sec) * 1000000); return *this; }
	/*
	  Returns true if t.tv_sec is positive or zero, and tv_usec within its valid limits (0-99999).
	 */
	bool isValid() const { return (t.tv_sec >= 0 && t.tv_usec >= 0 && t.tv_usec < 1000000); }
	/*
	  Returns a pointer to the C-struct representation of the Timeval.
	 */
	const struct timeval *getTimevalStruct() const { return &t; }
	/* 
	   Returns the "seconds" part of the Timeval.
	 */
	const long getSeconds() const { return t.tv_sec; }	
	/* 
	   Returns the "micro seconds" part of the Timeval. (Note that this it not the time in micro seconds).
	 */
	const long getMicroSeconds() const { return t.tv_usec; }	
	/* 
	   Returns the total time in milli seconds as a 64-bit integer.
	 */
	int64_t getTimeAsMilliSeconds() const;
	/*
	   Returns the total time in seconds as a double.
	 */
	double getTimeAsSecondsDouble() const;

	/* 
	   Returns the total time in milli seconds as a double.
	 */
	double getTimeAsMilliSecondsDouble() const;

	/*
	  Get Timeval as a string we can print.
	 */
	const string getAsString() const;

	// Operators
	friend bool operator<(const Timeval& t1, const Timeval& t2);
	friend bool operator<=(const Timeval& t1, const Timeval& t2);
	friend bool operator==(const Timeval& t1, const Timeval& t2);
	friend bool operator!=(const Timeval& t1, const Timeval& t2);
	friend bool operator>=(const Timeval& t1, const Timeval& t2);
	friend bool operator>(const Timeval& t1, const Timeval& t2);

	friend Timeval operator+(const Timeval& t1, const Timeval& t2);
	friend Timeval operator-(const Timeval& t1, const Timeval& t2);

	Timeval& operator+=(const Timeval& tv);
	Timeval& operator-=(const Timeval& tv);
};

}; // namespace haggle

#endif
