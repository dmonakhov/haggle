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
#ifndef _MANAGERMODULE_H
#define _MANAGERMODULE_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/
template <class theManager> class ManagerModule;

#include <libcpphaggle/Thread.h>
#include "Manager.h"

#include "HaggleKernel.h"
#include "Queue.h"
#include "Debug.h"

using namespace haggle;

/**
	This class serves as a parent class to managers' modules. It provides
	additional resources to that of a Runnable, such as a Queue and a reference
	to the manager that owns it.
*/
template <class theManager>
class ManagerModule : 
#ifdef DEBUG_LEAKS
	LeakMonitor,
#endif
	public Runnable
{
private:
        /**	Queue for IPC between the manager and the module. */
        Queue *h;
        /** A reference to the actual manager that owns this module. */
        theManager *manager;
        /**
        	This is inherited from the Runnable class. Implemented here as an empty
        	function, to avoid forcing child classes to declare these themselves.
        */
        bool run() { return false; }  // Thread entry
        /**
        	This is inherited from the Runnable class. Implemented here as an empty
        	function, to avoid forcing child classes to declare these themselves.
        */
        void cleanup() {} // Thread exit
public:
        /**
        	Returns the Queue associated with this module. The queue is created
        	when this function is first called, to avoid creating a Queue
		unneccesarily.
	*/
	Queue *getQueue()
	{
		if (!h) 
			h = new Queue(name);
		return h;
	}

        ManagerModule(theManager *m = NULL, const string name = "Unnamed manager module") : 
#ifdef DEBUG_LEAKS
		LeakMonitor(LEAK_TYPE_MANAGERMODULE),
#endif
		Runnable(name), h(NULL), manager(m) {}
        ~ManagerModule() { if(h) delete h; }
        /**
        	Returns the haggle kernel reference the manager has. Or NULL if there
        	was no manager reference.
        */
	HaggleKernel *getKernel()
	{
		if (manager)
			return manager->getKernel();
		return NULL;
	}
		
        /**
        	Returns the manager this module belongs to.
        */
        theManager *getManager() 
	{
                return manager;
        }
	
        /**
	 Returns the manager this module belongs to.
	 */
        const theManager *getManager() const
	{
                return manager;
        }
        /**
        	Adds an event to the main haggle event queue. See events and event
        	queues for information about how to create and handle events.
        */
	void addEvent(Event *e)
	{
		if (getKernel())
			getKernel()->addEvent(e);
	}

	class ModuleException : public Exception
        {
        public:
                ModuleException(const int err = 0,
				const char* data = "ManagerModule Error") :
			Exception(err, data) {}
        };
};

#endif /* _MANAGERMODULE_H */
