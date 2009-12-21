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

#include "Manager.h"
#include "Event.h"
#include "HaggleKernel.h"
#include "Attribute.h"

#define MANAGER_CONFIG_ATTR "ManagerConfiguration"
#define FILTER_CONFIG MANAGER_CONFIG_ATTR "=" ATTR_WILDCARD

Manager::Manager(const char *_name, HaggleKernel * _kernel) :
		EventHandler(),
#if defined(ENABLE_METADATAPARSER)
                MetadataParser(string(_name).substr(0, string(_name).find("Manager"))), 
#endif
		name(_name), state(MANAGER_STATE_STOPPED), registered(false), 
		readyForStartup(false), readyForShutdown(false), startupComplete(false), 
		kernel(_kernel)
{
#define __CLASS__ Manager
	setEventHandler(EVENT_TYPE_PREPARE_STARTUP, _onPrepareStartup);
	setEventHandler(EVENT_TYPE_STARTUP, _onStartup);
	setEventHandler(EVENT_TYPE_PREPARE_SHUTDOWN, _onPrepareShutdown);
	setEventHandler(EVENT_TYPE_SHUTDOWN, _onShutdown);
	
	// Register filter for node descriptions
	registerEventTypeForFilter(configEType, "Manager Configuration Filter Event", _onConfig, FILTER_CONFIG);
	
	if (!kernel->registerManager(this)) {
		HAGGLE_ERR("Could not register %s with kernel\n", name.c_str());
#if HAVE_EXCEPTION
		throw ManagerException(-1, "Could not register manager\n");
#endif
	} else {
                registered = true;
        }
}

Manager::~Manager()
{
	// Remove the configuration filter
	unregisterEventTypeForFilter(configEType);
}


void Manager::signalIsReadyForStartup()
{
	readyForStartup = true;
	kernel->signalIsReadyForStartup(this);
}

void Manager::signalIsReadyForShutdown()
{
	readyForShutdown = true;
	kernel->signalIsReadyForShutdown(this);
}

bool Manager::isReadyForStartup()
{
	return readyForStartup;
}

bool Manager::isReadyForShutdown()
{
	return readyForShutdown;
}

bool Manager::registerWithKernel()
{
	if (!registered) {
		kernel->registerManager(this);
		registered = false;
		return true;
	}
	return false;
}

bool Manager::unregisterWithKernel()
{
	state = MANAGER_STATE_STOPPED;
	
	if (registered) {
		kernel->unregisterManager(this);
		registered = false;
		return true;
	}
	return false;
}

void Manager::_onPrepareStartup(Event *e)
{
	state = MANAGER_STATE_PREPARE_STARTUP;
	onPrepareStartup();
}

void Manager::_onStartup(Event *e)
{
	state = MANAGER_STATE_STARTUP;
	onStartup();
	state = MANAGER_STATE_RUNNING;
}

void Manager::_onPrepareShutdown(Event *e)
{
	state = MANAGER_STATE_PREPARE_SHUTDOWN;
	onPrepareShutdown();
}

void Manager::_onShutdown(Event *e)
{
	state = MANAGER_STATE_SHUTDOWN;
	onShutdown();
}

void Manager::_onConfig(Event *e)
{
	DataObjectRefList& dObjs = e->getDataObjectList();

	while (dObjs.size()) {
		onConfig(dObjs.pop());
	}
}
