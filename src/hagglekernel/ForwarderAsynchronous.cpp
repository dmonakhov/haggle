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

ForwarderAsynchronous::ForwarderAsynchronous(
		Timeval _aging_time_delta,
		ForwardingManager *m, 
		const string name) :
	Forwarder(m, name),
	next_aging_time(Timeval::now() + _aging_time_delta),
	aging_time_delta(_aging_time_delta),
	should_recalculate_metric_do(false)
{
	// Check this:
	if (aging_time_delta <= 0) {
#if HAVE_EXCEPTION
		throw Exception(0, "Time delta cannot be zero or less!\n");
#else
                return;
#endif
	}
	// Start the thread:
	start();
}

ForwarderAsynchronous::~ForwarderAsynchronous()
{
	// Tell the thread to quit:
	actionQueue.insert(new FP_Action(FP_quit,NULL,NULL,NULL,NULL));
	// Make sure noone else adds stuff to the queue:
	actionQueue.close();
	// Wait for the thread to terminate:
	stop();
}

void ForwarderAsynchronous::addMetricDO(DataObjectRef &metricDO)
{
	if (!metricDO)
                return;
        
        actionQueue.insert(new FP_Action(FP_add_metric,metricDO,NULL,NULL,NULL));
#ifdef DEBUG
        //actionQueue.insert(new FP_Action(FP_print_table,NULL,NULL,NULL, NULL));
#endif

}

void ForwarderAsynchronous::newNeighbor(NodeRef &neighbor)
{
	if (!neighbor)
                return;

        actionQueue.insert(new FP_Action(FP_new_neighbor,NULL,neighbor,NULL,NULL));
#ifdef DEBUG
        //actionQueue.insert(new FP_Action(FP_print_table,NULL,NULL,NULL, NULL));
#endif
}

void ForwarderAsynchronous::endNeighbor(NodeRef &neighbor)
{
	if (!neighbor)
                return;
        
        actionQueue.insert(new FP_Action(FP_end_neighbor,NULL,neighbor,NULL,NULL));
#ifdef DEBUG
        //actionQueue.insert(new FP_Action(FP_print_table,NULL,NULL,NULL,NULL));
#endif
}

void ForwarderAsynchronous::generateTargetsFor(NodeRef &neighbor)
{
	if (!neighbor)
		return;
	
	// Retreive the result:
	actionQueue.insert(
		new FP_Action(FP_generate_targets_for,NULL,neighbor,NULL,NULL));
}

void ForwarderAsynchronous::generateDelegatesFor(DataObjectRef &dObj, NodeRef &target)
{
	if (!target)
		return;
	
	// Retreive the result:
	actionQueue.insert(
		new FP_Action(FP_generate_delegates_for,dObj,target,NULL,NULL));
}

string ForwarderAsynchronous::getEncodedState(void)
{
	Mutex	theMutex;
	string	retval;
	
	// Mutexes are unlocked by default:
	theMutex.lock();
	// Retreive the result:
	actionQueue.insert(
		new FP_Action(FP_get_encoded_state,NULL,NULL,&retval,&theMutex));
	theMutex.lock();
	
	return retval;
}

void ForwarderAsynchronous::setEncodedState(string &state)
{
	Mutex theMutex;
	
	/*
		This has to be done synchronously, even though it doesn't return a 
		value, because we cannot be sure when the state string is deallocated.
	*/
	
	// Mutexes are unlocked by default:
	theMutex.lock();
	// Retreive the result:
	actionQueue.insert(
		new FP_Action(FP_set_encoded_state,NULL,NULL,&state,&theMutex));
	theMutex.lock();
}

#ifdef DEBUG
void ForwarderAsynchronous::printRoutingTable(void)
{
	Mutex theMutex;
	
	// Mutexes are unlocked by default:
	theMutex.lock();
	actionQueue.insert(
		new FP_Action(FP_print_table,NULL,NULL,NULL,&theMutex));
	theMutex.lock();
}
#endif

bool ForwarderAsynchronous::run(void)
{
	/*
		The reason for having two different event processing loops is because
		the first one is waiting for setEncodedState() to be called. This may 
		happen after calls to "synchronous" functions (those that specify a 
		mutex) above, which have to be handled so that the kernel thread doesn't
		stop working, which would prevent setEncodedState() from being called.
	*/
	{
	List<FP_Action *> to_be_processed_later;
	bool has_set_state = false;
	while (!shouldExit() && !has_set_state)
	{
		FP_Action	*action;
		Timeval		time_left;
		
		action = NULL;
		time_left = next_aging_time - Timeval::now();
		
		switch(actionQueue.retrieve(&action, &time_left))
		{
			default:
				if(action)
					delete action;
			break;
			
			case QUEUE_TIMEOUT:
				/*
					This shouldn't happen - but we make sure the module doesn't
					break if it does. This means that it has either taken an 
					exceptionally long time to get the state string, or that it
					is not coming. Either one is problematic.
				*/
				HAGGLE_DBG(
					"WARNING: one aging time expired before "
					"setting the state.\n");
				next_aging_time += aging_time_delta;
				//_ageMetric();
				
				if(action)
					delete action;
			break;
			
			case QUEUE_ELEMENT:
				switch(action->action)
				{
					// These actions should be saved and done later:
					case FP_add_metric:
					case FP_new_neighbor:
					case FP_end_neighbor:
					case FP_generate_targets_for:
					case FP_generate_delegates_for:
						to_be_processed_later.push_back(action);
					break;
					
					// These actions can be safely ignored (there is nothing in
					// the routing tables, so no data to return).
					case FP_get_encoded_state:
						// There isn't anything to get, so don't even call the 
						// function.
						
						// Unlock the mutex:
						if(action->toBeUnlocked)
							action->toBeUnlocked->unlock();
						delete action;
					break;
					
					case FP_set_encoded_state:
						// YAY!
						has_set_state = true;
						_setEncodedState(action->theString);
						if(action->toBeUnlocked)
							action->toBeUnlocked->unlock();
						delete action;
					break;
					
#ifdef DEBUG
					case FP_print_table:
						_printRoutingTable();
						if(action->toBeUnlocked)
							action->toBeUnlocked->unlock();
						delete action;
					break;
#endif
					
					case FP_quit:
						// Already? Fine.
                                                cancel();
						if(action->toBeUnlocked)
							action->toBeUnlocked->unlock();
						delete action;
					break;
				}
			break;
		}
	}
	// Reinsert all the actions that were stored to be dealt with later above
	while(!to_be_processed_later.empty())
	{
		FP_Action	*action;
		
		action = to_be_processed_later.front();
		to_be_processed_later.pop_front();
		actionQueue.insert(action);
	}
	}
	
	while (!shouldExit()) {
		FP_Action	*action;
		Timeval		time_left;
		bool		timeout_recalculates_metric_do;
		
		action = NULL;
		timeout_recalculates_metric_do = false;
		time_left = next_aging_time - Timeval::now();
		// Should we recalculate the metric data object (and the aging time is 
		// not too close)?
		if(should_recalculate_metric_do && time_left > Timeval(2,0))
		{
			time_left = Timeval(2,0);
			timeout_recalculates_metric_do = true;
		}
		
		switch(actionQueue.retrieve(&action, &time_left))
		{
			default:
			break;
			
			case QUEUE_TIMEOUT:
				// Should we recalculate the metric do?
				if(timeout_recalculates_metric_do)
				{
					DataObjectRef oldMetric = myMetricDO;
					// Yes: Recalculate it:
					updateMetricDO();
					
					if(myMetricDO != oldMetric)
						getManager()->sendMetric();
					// It's fresh:
					should_recalculate_metric_do = false;
				}else{
					// No? Then it's time to age the metrics:
					next_aging_time += aging_time_delta;
					_ageMetric();
					// Now we should recalculate the metric do:
					should_recalculate_metric_do = true;
				}
			break;
			
			case QUEUE_ELEMENT:
				switch(action->action)
				{
					case FP_add_metric:
						_addMetricDO(action->theDO);
					break;
					
					case FP_new_neighbor:
						_newNeighbor(action->theNode);
					break;
                                        case FP_end_neighbor:
						_endNeighbor(action->theNode);
					break;
					
					case FP_generate_targets_for:
						_generateTargetsFor(action->theNode);
					break;
					
					case FP_generate_delegates_for:
						_generateDelegatesFor(action->theDO, action->theNode);
					break;
					
					case FP_get_encoded_state:
						_getEncodedState(action->theString);
					break;
					
					case FP_set_encoded_state:
						// Eh.. this shouldn't happen, but ok...
						_setEncodedState(action->theString);
						HAGGLE_DBG("setEncodedState() called more than once\n");
					break;
					
#ifdef DEBUG
					case FP_print_table:
						_printRoutingTable();
					break;
#endif
					
					case FP_quit:
						cancel();
					break;
				}
				if (action->toBeUnlocked)
					action->toBeUnlocked->unlock();
			break;
		}
		if(action)
			delete action;
	}
	return false;
}
