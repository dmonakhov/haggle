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

#include <libcpphaggle/Platform.h>
#include "Interface.h"

using namespace haggle;

const char *Interface::typestr[] = {
	"undefined",
	"application[port]",
	"application[local]",
	"bluetooth",
	"ethernet",
	"wifi",
	"media",
	NULL,
};

void Interface::setIdentifierString()
{
	char buf[30];

	if (!identifierIsValid) {
		identifierString = "Invalid identifier";
		return;
	}

	switch (type) {
		case IFTYPE_APPLICATION_PORT:
			sprintf(buf, "%hu", identifier.application_port);
			identifierString = string("Application on port ") + buf;
			return;		 
		case IFTYPE_APPLICATION_LOCAL:
			identifierString = string("Application on local unix socket: ") + identifier.application_local;
			return;
		case IFTYPE_BLUETOOTH:
		case IFTYPE_ETHERNET:
		case IFTYPE_WIFI:
			sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
				(unsigned char)identifier.mac[0], (unsigned char)identifier.mac[1],
				(unsigned char)identifier.mac[2], (unsigned char)identifier.mac[3],
				(unsigned char)identifier.mac[4], (unsigned char)identifier.mac[5]);
			identifierString = buf;
			return;
		case IFTYPE_MEDIA:
			identifierString = identifier.media;
		case IFTYPE_UNDEF:
		default:
			break;
	}
	identifierString = "Undefined";
}

Interface::Interface(InterfaceType_t _type, const void *identifier, 
		     const Address *addr, const string _name, const flag_t _flags) :
#ifdef DEBUG_LEAKS
LeakMonitor(LEAK_TYPE_INTERFACE),
#endif
type(_type), name(_name), flags(_flags & (IFFLAG_ALL ^ IFFLAG_STORED)), identifierIsValid(false), identifierString("Undefined")
{
	if (identifier) {
		identifierIsValid = true;
		memcpy(this->identifier.raw, identifier, getIdentifierLen());
		setIdentifierString();
	}

	if (addr)
		addAddress(addr);
}

Interface::Interface(InterfaceType_t _type, const void *identifier, 
		     const Addresses *addrs, const string _name, const flag_t _flags) :
#ifdef DEBUG_LEAKS
LeakMonitor(LEAK_TYPE_INTERFACE),
#endif
type(_type), name(_name), flags(_flags & (IFFLAG_ALL ^ IFFLAG_STORED)), identifierIsValid(false), identifierString("Undefined")
{
	if (identifier) {
		identifierIsValid = true;
		memcpy(this->identifier.raw, identifier, getIdentifierLen());
		setIdentifierString();
	}

	if (addrs) {
		Addresses::const_iterator it = addrs->begin();

		while (it != addrs->end()) {
			addAddress(*it);
			it++;
		}
	}
}

Interface::Interface(const Interface &iface) :
#ifdef DEBUG_LEAKS
LeakMonitor(LEAK_TYPE_INTERFACE),
#endif
	type(iface.type), name(iface.name), flags(iface.flags & (IFFLAG_ALL ^ IFFLAG_STORED)), 
	identifierIsValid(iface.identifierIsValid), identifierString(iface.identifierString), 
	addresses(iface.addresses)
{		
	memcpy(identifier.raw, iface.identifier.raw, getIdentifierLen());
}

Interface *Interface::copy() const
{
	return new Interface(*this);
}

Interface::~Interface(void)
{
}

const char *Interface::typeToStr(InterfaceType_t type)
{
	return typestr[type];
}

InterfaceType_t Interface::strToType(const char *str)
{
	int i = 0;

	while (typestr[i]) {
		if (strcmp(typestr[i], str) == 0) {
			return (InterfaceType_t)i;
		}
		i++;
	}
	return IFTYPE_UNDEF;
}

const long Interface::getIdentifierLen() const
{
	if (!identifierIsValid)
		return -1;

	switch (type) {
		case IFTYPE_APPLICATION_PORT:
			return sizeof(identifier.application_port);
			break;
			
		case IFTYPE_APPLICATION_LOCAL:
			return sizeof(identifier.application_local);
			break;
		case IFTYPE_BLUETOOTH:
		case IFTYPE_ETHERNET:
		case IFTYPE_WIFI:
			return sizeof(identifier.mac);
			break;
		case IFTYPE_MEDIA:
			return strlen(identifier.media);
			break;
		case IFTYPE_UNDEF:
			return 0;
		default:
			return -1;
			break;
	}
	return -1;
}

const char *Interface::getFlagsStr() const
{
	static string str;
	
	str.clear();
	
	if (flags & IFFLAG_UP)
		str += "UP";
	else
		str += "DOWN";
	
	if (flags & IFFLAG_LOCAL)
		str += "|LOCAL";
	
	if (flags & IFFLAG_SNOOPED)
		str += "|SNOOPED";
	
	return str.c_str();
}

void Interface::addAddress(const Address *addr)
{
	if (addr == NULL)
		return;

	for (Addresses::iterator it = addresses.begin(); it != addresses.end(); it++) {
		// Are these the same?
		if (*addr == *(*it)) {
			// Merge:
			(*it)->mergeWith(addr);
			return;
		}
	}
	// Address not found in list, insert it.
	addresses.push_front(addr->copy());
}

void Interface::addAddresses(const Addresses *adds)
{
	for (Addresses::const_iterator it = adds->begin(); it != adds->end(); it++) {
		addAddress(*it);
	}
}

bool Interface::hasAddress(const Address &add) const
{
	for (Addresses::const_iterator it = addresses.begin(); it != addresses.end(); it++) {
		// Are these the same?
		if(add == *(*it)) {
			return true;
		}
	}
	return false;
}

const char *Interface::getName() const
{
	return name.c_str();
}

const Addresses *Interface::getAddresses() const
{
	return &addresses;
}

Address *Interface::getAddressByType(const AddressType_t _type)
{
	for (Addresses::iterator it = addresses.begin(); it != addresses.end(); it++) {
		if ((*it)->getType() == _type) {
			return *it;
		}
	}
	return NULL;
}

const Address *Interface::getAddressByType(const AddressType_t _type) const
{
	return const_cast<Interface *>(this)->getAddressByType(_type);
}

void Interface::setFlag(const flag_t flag)
{
	flags |= flag;
}

void Interface::resetFlag(const flag_t flag)
{
	flags &= (flag ^ 0xff);
}

bool Interface::isLocal() const
{
	return (flags & IFFLAG_LOCAL) != 0;
}

bool Interface::isFlagSet(const flag_t flag) const
{
	return (flags & flag) != 0;
}

bool Interface::isStored() const
{
	return (flags & IFFLAG_STORED) != 0;
}

bool Interface::isApplication() const
{
	return (type == IFTYPE_APPLICATION_PORT || type == IFTYPE_APPLICATION_LOCAL);
}

void Interface::up()
{
	flags |= IFFLAG_UP;
}

void Interface::down()
{
	flags ^= IFFLAG_UP;
}

bool Interface::isUp() const
{
	return (flags & IFFLAG_UP) != 0;
}

bool Interface::isSnooped() const
{
	return (flags & IFFLAG_SNOOPED) != 0;
}

bool Interface::equal(const InterfaceType_t type, const char *identifier) const
{
	if (!identifierIsValid || type != this->type)
		return false;
	
	switch (type) {
		case IFTYPE_APPLICATION_PORT:			
			if (memcmp(&this->identifier.application_port, 
					identifier, sizeof(this->identifier.application_port)) == 0)
				return true;
			break;
		case IFTYPE_APPLICATION_LOCAL:
			if (strcmp(this->identifier.application_local, identifier) == 0)
				return true;
			break;
		case IFTYPE_BLUETOOTH:
		case IFTYPE_ETHERNET:
		case IFTYPE_WIFI:
			if (memcmp(this->identifier.mac, identifier, ETH_ALEN) == 0)
				return true;
			break;
		case IFTYPE_MEDIA:
			// Probably check identifier here too, whatever type it should be
			break;
		case IFTYPE_UNDEF:
		default:
			return false;
			break;
	}
	return false;
}

bool operator==(const Interface& i1, const Interface& i2)
{
	if (i1.type != i2.type)
		return false;

	// Cannot compare interfaces without valid identifiers
	if (!i1.identifierIsValid || !i2.identifierIsValid)
		return false;

	switch (i1.type) {
		case IFTYPE_APPLICATION_PORT:			
			if (i1.identifier.application_port == i2.identifier.application_port)
				return true;
			break;
		case IFTYPE_APPLICATION_LOCAL:
			if (strcmp(i1.identifier.application_local, i2.identifier.application_local) == 0)
				return true;
			break;
		case IFTYPE_BLUETOOTH:
		case IFTYPE_ETHERNET:
		case IFTYPE_WIFI:
			if (memcmp(i1.identifier.mac, i2.identifier.mac, sizeof(i2.identifier.mac)) == 0)
				return true;
			break;
		case IFTYPE_MEDIA:
			// Probably check identifier here too, whatever type it should be
			break;
		case IFTYPE_UNDEF:
		default:
			return false;
			break;
	}
	return false;
}

bool operator<(const Interface& i1, const Interface& i2)
{
	if (i1.type < i2.type)
		return true;
	else if (i1.type > i2.type)
		return false;

	switch (i1.type) {
		case IFTYPE_APPLICATION_PORT:			
			if (i1.identifier.application_port < i2.identifier.application_port)
				return true;
			break;
		case IFTYPE_APPLICATION_LOCAL:
			if (strcmp(i1.identifier.application_local, i2.identifier.application_local) < 0)
				return true;
			break;
		case IFTYPE_BLUETOOTH:
		case IFTYPE_ETHERNET:
		case IFTYPE_WIFI:
			if (memcmp(i1.identifier.mac, i2.identifier.mac, sizeof(i2.identifier.mac)) < 0)
				return true;
			break;
		case IFTYPE_MEDIA:
			// Probably check identifier here too, whatever type it should be
			break;
		case IFTYPE_UNDEF:
		default:
			return false;
			break;
	}
	return false;
}
