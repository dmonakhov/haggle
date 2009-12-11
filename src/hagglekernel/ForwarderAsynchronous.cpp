/* Copyright 2009 Uppsala University
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

#include "ForwarderAsynchronous.h"

ForwarderAsynchronous::ForwarderAsynchronous(ForwardingManager *m, const EventType type, const string name) :
	Forwarder(m, name), eventType(type)
{
}

ForwarderAsynchronous::~ForwarderAsynchronous()
{
	stop();
}

void ForwarderAsynchronous::quit()
{
	if (isRunning()) {
		// Tell the thread to quit:
		taskQ.insert(new ForwardingTask(FWD_TASK_QUIT));
		// Make sure noone else adds stuff to the queue:
		taskQ.close();
		
		// Wait until the thread finishes
		join();
	}
}

void ForwarderAsynchronous::newRoutingInformation(const DataObjectRef &dObj)
{
	if (!dObj)
                return;
        
        taskQ.insert(new ForwardingTask(FWD_TASK_NEW_ROUTING_INFO, dObj));
}

void ForwarderAsynchronous::newNeighbor(const NodeRef &neighbor)
{
	if (!neighbor)
                return;

        taskQ.insert(new ForwardingTask(FWD_TASK_NEW_NEIGHBOR, neighbor));
}

void ForwarderAsynchronous::endNeighbor(const NodeRef &neighbor)
{
	if (!neighbor)
                return;
        
        taskQ.insert(new ForwardingTask(FWD_TASK_END_NEIGHBOR, neighbor));
}

void ForwarderAsynchronous::generateTargetsFor(const NodeRef &neighbor)
{
	if (!neighbor)
		return;
	
	taskQ.insert(new ForwardingTask(FWD_TASK_GENERATE_TARGETS, neighbor));
}

void ForwarderAsynchronous::generateDelegatesFor(const DataObjectRef &dObj, const NodeRef &target)
{
	if (!dObj || !target)
		return;
	
	taskQ.insert(new ForwardingTask(FWD_TASK_GENERATE_DELEGATES, dObj, target));
}

void ForwarderAsynchronous::generateRoutingInformationDataObject(const NodeRef& node)
{
	taskQ.insert(new ForwardingTask(FWD_TASK_GENERATE_ROUTING_INFO_DATA_OBJECT, node));
}

#ifdef DEBUG
void ForwarderAsynchronous::printRoutingTable(void)
{
	taskQ.insert(new ForwardingTask(FWD_TASK_PRINT_RIB, NULL, NULL));
}
#endif

/*
 General overview of thread loop:
 
 The forwarding module waits for tasks that are input into the task queue (taskQ) by the 
 forwarding manager. Once a task is available, the forwarding module will read it and
 execute any tasks. The module may return results of the task to the forwarding manager
 by using a private event. In that case, it passes the original task object back to the
 manager with any results embedded. Sometimes it might just be enough to signal that a
 a task is complete.
 
 */
bool ForwarderAsynchronous::run(void)
{
	while (!shouldExit()) {
		ForwardingTask *task = NULL;
		
		switch (taskQ.retrieve(&task)) {
			default:
				if (task)
					delete task;
				break;
			case QUEUE_TIMEOUT:
				/*
				 This shouldn't happen - but we make sure the module doesn't
				 break if it does. This means that it has either taken an 
				 exceptionally long time to get the state string, or that it
				 is not coming. Either one is problematic.
				 */
				HAGGLE_DBG("WARNING: timeout occurred in forwarder task queue.\n");
				break;
			case QUEUE_ELEMENT:
				switch (task->getType()) {
					case FWD_TASK_NEW_ROUTING_INFO:
						newRoutingInformation(getRoutingInformation(task->getDataObject()));
						break;
					case FWD_TASK_NEW_NEIGHBOR:
						_newNeighbor(task->getNode());
						break;
					case FWD_TASK_END_NEIGHBOR:
						_endNeighbor(task->getNode());
						break;
					case FWD_TASK_GENERATE_TARGETS:
						_generateTargetsFor(task->getNode());
						break;
						
					case FWD_TASK_GENERATE_DELEGATES:
						_generateDelegatesFor(task->getDataObject(), task->getNode());
						break;
					case FWD_TASK_GENERATE_ROUTING_INFO_DATA_OBJECT:
						task->setDataObject(createRoutingInformationDataObject());
						addEvent(new Event(eventType, task));
						task = NULL;
						break;
#ifdef DEBUG
					case FWD_TASK_PRINT_RIB:
						_printRoutingTable();
						break;
#endif
					case FWD_TASK_QUIT:
						/*
							When the forwarding module is asked to quit,
							the forwarding manager should save any of its
							state. Return this state to the manager in a 
							callback.
						 */
						if (eventType) {
							task->setRepositoryEntryList(new RepositoryEntryList());
							
							if (task->getRepositoryEntryList()) {
								getSaveState(*task->getRepositoryEntryList());
							}
						}
						// Always send the callback to ensure the manager is notified
						// that the module is done.
						HAGGLE_DBG("Forwarding module %s QUITs\n", getName());
						addEvent(new Event(eventType, task));
						task = NULL;
						
						cancel();
						break;
				}
				break;
		}
		if (task)
			delete task;
	}
	return false;
}
