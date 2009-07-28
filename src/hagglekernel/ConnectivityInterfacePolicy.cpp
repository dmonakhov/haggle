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

#include "ConnectivityInterfacePolicy.h"

#include <libcpphaggle/Platform.h>

ConnectivityInterfacePolicy *newConnectivityInterfacePolicyTTL1(void)
{
	return (ConnectivityInterfacePolicy *) 
		new ConnectivityInterfacePolicyTTL(1);
}

ConnectivityInterfacePolicy *newConnectivityInterfacePolicyTTL2(void)
{
	return (ConnectivityInterfacePolicy *) 
		new ConnectivityInterfacePolicyTTL(2);
}

ConnectivityInterfacePolicy *newConnectivityInterfacePolicyTTL3(void)
{
	return (ConnectivityInterfacePolicy *) 
		new ConnectivityInterfacePolicyTTL(3);
}

ConnectivityInterfacePolicyTTL::ConnectivityInterfacePolicyTTL(long TimeToLive)
{
	base_ttl = TimeToLive;
	update();
}

ConnectivityInterfacePolicyTTL::~ConnectivityInterfacePolicyTTL()
{
}

void ConnectivityInterfacePolicyTTL::update()
{
	current_ttl = base_ttl;
}

void ConnectivityInterfacePolicyTTL::age()
{
	if (current_ttl > 0)
		current_ttl--;
}

const char *ConnectivityInterfacePolicyTTL::ageStr()
{
	sprintf(agestr, "ttl=%lu", current_ttl);
	
	return agestr;
}


bool ConnectivityInterfacePolicyTTL::isDead()
{
	return current_ttl == 0;
}

ConnectivityInterfacePolicy *newConnectivityInterfacePolicyAgeless(void)
{
	return (ConnectivityInterfacePolicy *) 
		new ConnectivityInterfacePolicyAgeless();
}

ConnectivityInterfacePolicyAgeless::ConnectivityInterfacePolicyAgeless()
{
}

ConnectivityInterfacePolicyAgeless::~ConnectivityInterfacePolicyAgeless()
{
}

void ConnectivityInterfacePolicyAgeless::update()
{
}

void ConnectivityInterfacePolicyAgeless::age()
{
}

bool ConnectivityInterfacePolicyAgeless::isDead()
{
	return false;
}

const char *ConnectivityInterfacePolicyAgeless::ageStr()
{
	return "infinite";
}


ConnectivityInterfacePolicyTime::ConnectivityInterfacePolicyTime(const Timeval &ttl)
{
	expiry = ttl;
}

ConnectivityInterfacePolicyTime::~ConnectivityInterfacePolicyTime()
{
}

void ConnectivityInterfacePolicyTime::update()
{
}

void ConnectivityInterfacePolicyTime::age()
{
}

bool ConnectivityInterfacePolicyTime::isDead()
{
	return (expiry < Timeval::now());
}


const char *ConnectivityInterfacePolicyTime::ageStr()
{
	return (Timeval::now() - expiry).getAsString().c_str();
}
