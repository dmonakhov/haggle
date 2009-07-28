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
#ifndef _FORWARDEREMPTY_H
#define _FORWARDEREMPTY_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/
class ForwarderEmpty;

#include "Forwarder.h"

/**
*/
class ForwarderEmpty : public Forwarder {
public:
	ForwarderEmpty(
		ForwardingManager *m = NULL, 
		const string name = "Empty forwarding module") :
		Forwarder(m, name) {}
	~ForwarderEmpty() {}
};

#endif
