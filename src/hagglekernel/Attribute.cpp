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
#include <string.h>
#include <stdlib.h>

#include <libcpphaggle/Platform.h>
#include <libcpphaggle/Exception.h>
#include <libcpphaggle/String.h>
#include <haggleutils.h>

#include "DataObject.h"
#include "Node.h"
#include "Interface.h"
#include "Attribute.h"

Attribute::Attribute(const char *_nameValue, const unsigned long _weight) : 
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_ATTRIBUTE),
#endif
	name(""), value(""), weight(_weight)
{
	size_t pos;

	name = _nameValue;
	pos = name.find("=");
	value = name.substr(pos + 1);
	name = name.substr(0, pos);}

Attribute::Attribute(const string _name, const string _value, const unsigned long _weight) : 
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_ATTRIBUTE),
#endif
	name(_name), value(_value), weight(_weight)
{
}

// Copy-constructor
Attribute::Attribute(const Attribute & attr) : 
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_ATTRIBUTE),
#endif
	name(attr.getName()), value(attr.getValue()), weight(attr.getWeight())
{
}

Attribute::~Attribute()
{
}

string Attribute::getString() const
{ 
	char weightstr[11];
	
	snprintf(weightstr, 11, "%lu", weight);

	return string(name + "=" + value + ":" + weightstr); 
}

bool operator==(const Attribute & a, const Attribute & b)
{
	if (a.getName() != b.getName())
		return false;

	if (a.getValue() == "*" || b.getValue() == "*") {
		return true;
	} else {
		return (a.getValue() == b.getValue());
	}
}

bool operator<(const Attribute & a, const Attribute & b)
{
	if (a.getName() != b.getName())
		return (a.getName() < b.getName());

	if (a.getValue() == "*") {
		return false;
	} else {
		return (a.getValue() < b.getValue());
	}
}

Attributes *Attributes::copy() const {
        return new Attributes(*this);
}

Attributes::iterator Attributes::find(const Attribute& a) {
        Pair<iterator, iterator> p = equal_range(a.getName());
		
        for (; p.first != p.second; ++p.first) {
                if (a == (*p.first).second)
                        return p.first;
        }
        return end();
}
	
Attributes::const_iterator Attributes::find(const Attribute& a) const {
        Pair<const_iterator, const_iterator> p = equal_range(a.getName());
		
        for (; p.first != p.second; ++p.first) {
                if (a == (*p.first).second)
                        return p.first;
        }
        return end();
}

Attributes::size_type Attributes::erase(const Attribute& a) { 
        Pair<iterator, iterator> p = equal_range(a.getName());
		
        for (; p.first != p.second; ++p.first) {
                if (a == (*p.first).second) {
                        HashMap<string, Attribute>::erase(p.first);
                        return 1;
                }
        }
        return 0;
}
bool Attributes::add(const Attribute& a) { return insert(make_pair(a.getName(), a)) != end(); }
