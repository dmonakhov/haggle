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
#ifndef _EVENTQUEUE_H
#define _EVENTQUEUE_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/
class EventQueue;

#ifndef OS_WINDOWS
#include <unistd.h>
#include <fcntl.h>
#endif
#include <time.h>

#include <libcpphaggle/Platform.h>
#include <libcpphaggle/Heap.h>
#include <libcpphaggle/Thread.h>
#include <libcpphaggle/Timeval.h>
#include <libcpphaggle/Watch.h>
#include <haggleutils.h>

#include "Event.h"

typedef enum {
	EQ_ERROR = -1,
	EQ_TIMEOUT,
	EQ_EVENT,
	EQ_EMPTY,
	EQ_EVENT_SHUTDOWN
} EQEvent_t;

// Locking is provided by a mutex, so the queue should be thread safe
/** */
class EventQueue : public Heap
{
private:
        Mutex mutex;
        Mutex shutdown_mutex;
        bool shutdownEvent;
protected:
	Signal signal;
public:
        EventQueue() : Heap(), mutex("EventQ mutex"), 
                       shutdown_mutex("EventQ shutdown mutex"), 
                       shutdownEvent(false) {}
        ~EventQueue() {
                Event *e;

                while ((e = static_cast<Event *>(extractFirst())))
                        delete e;
        }
	EQEvent_t hasNextEvent() { 
                Mutex::AutoLocker l(mutex);		
                return shutdownEvent ? EQ_EVENT_SHUTDOWN : (empty() ? EQ_EMPTY : EQ_EVENT); 
	}
        EQEvent_t getNextEventTime(Timeval *tv) {
                Mutex::AutoLocker l(mutex);

		signal.lower();
		
                if (shutdownEvent) {
			tv->zero();
			return EQ_EVENT_SHUTDOWN;
		} else if (!empty()) {
                        tv->set(front()->getKey());
                        return EQ_EVENT;
                }
                return EQ_EMPTY;
        }
        Event *getNextEvent() {
                Event *e = NULL;
	
                synchronized(shutdown_mutex) {
                        if (shutdownEvent) {
                                shutdownEvent = false;
                                signal.lower();
                                return new Event(EVENT_TYPE_SHUTDOWN, 0);
                        }
                }

                mutex.lock();
                e = static_cast<Event *>(extractFirst());
                mutex.unlock();

                return e;
        }
        void enableShutdownEvent() {
                Mutex::AutoLocker l(shutdown_mutex);
                shutdownEvent = true;
		HAGGLE_DBG("Setting shutdown event\n");
		signal.raise();
        }
        void addEvent(Event *e) {
                Mutex::AutoLocker l(mutex);
                if (insert(e))
			signal.raise();
        }
};

#endif /* _EVENTQUEUE_H */
