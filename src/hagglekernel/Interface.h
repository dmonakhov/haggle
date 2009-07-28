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
#ifndef _INTERFACE_H
#define _INTERFACE_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/
class Interface;

#include <time.h>

#include <libcpphaggle/Platform.h>
#include <libcpphaggle/Exception.h>
#include <libcpphaggle/Thread.h>
#include <libcpphaggle/Reference.h>
#include <haggleutils.h>
#include <libcpphaggle/String.h>
#include <libcpphaggle/Pair.h>

#include "Debug.h"
#include "Address.h"

using namespace haggle;

// Some interface flags
#define IFFLAG_UP                            (1<<0)
#define IFFLAG_LOCAL                         (1<<1)
#define IFFLAG_SNOOPED                       (1<<2) /* This interface was found by snooping on 
						       incoming data objects */
#define IFFLAG_STORED                        (1<<3) // This interface is in the Interface store
#define IFFLAG_ALL	                     ((1<<4)-1)

#define MAX_MEDIA_PATH_LEN 100
#define GENERIC_MAC_LEN 6
#define LOCAL_PATH_MAX 108 // This is based on the UNIX_PATH_MAX in linux/un.h from the Linux headers

typedef enum {
	IFTYPE_UNDEF = 0,
	IFTYPE_APPLICATION_PORT,
	IFTYPE_APPLICATION_LOCAL,
	IFTYPE_BLUETOOTH,
	IFTYPE_ETHERNET,
	IFTYPE_WIFI,
	IFTYPE_MEDIA,
	_IFTYPE_MAX,
} InterfaceType_t;

/**
	Interface class.
	
	This is the class that keeps interface information. 
 */
#ifdef DEBUG_LEAKS
class Interface : public LeakMonitor
#else
class Interface
#endif
{
	union {
		char raw[1]; // Used for generic access
		char mac[GENERIC_MAC_LEN];
		short application_port;
		char application_local[LOCAL_PATH_MAX];
		char media[MAX_MEDIA_PATH_LEN];
	} identifier;

	static const char *typestr[];
	
	const InterfaceType_t type;
	
	// The name of this interface
	const string name;

	typedef char flag_t;
	// Flag field to show certain boolean things.
	flag_t flags;

	// Set to true if there is a valid identifier set for this interface
	bool identifierIsValid;

	string identifierString;

	// The addresses associated with this interface.
	Addresses addresses;

	// Creates and sets the identfierString
	void setIdentifierString();
public:
	/**
		Constructors. Differs in taking one or multiple addresses.
	*/
	Interface(InterfaceType_t type, const void *identifier, const Address *addr, 
		  const string name = "Unnamed Interface",  const flag_t flags = 0);

	Interface(InterfaceType_t type, const void *identifier, const Addresses *addrs = NULL, 
		  const string name = "Unnamed Interface",  const flag_t flags = 0);

	Interface(const Interface &iface);
	/**
		Destructor.
	*/
	~Interface();


	Interface *copy() const;

	static InterfaceType_t strToType(const char *str);
	static const char *typeToStr(InterfaceType_t type);

	/**
	Gets the currently set name for this interface.
	*/
	const char *getName() const;

	/**
		Gets the type for this interface.
	*/
	const InterfaceType_t getType() const { return type; }
	const char *getTypeStr() const { return typeToStr(type); }
	/**
		Gets a pointer to the beginning of the raw identifier which is unique for this interface.
	*/
	const char *getRawIdentifier() const { return identifier.raw; }
	/**
		Gets the length of the raw identifier which is unique for this interface.
	*/
	const long getIdentifierLen() const;
	/**
		Gets the identifier for this interface as a pair.
	*/
	const Pair<const char *, const long> getIdentifier() const { return make_pair(identifier.raw, getIdentifierLen()); }
	
	/**
		Gets the identifier as a human readable C-string.
	*/
	const char *getIdentifierStr() const { return identifierString.c_str(); }
	
	/**
		Gets the flags as a human readable C-string.
	 */
	const char *getFlagsStr() const;
	/**
		Inserts another address into this interface.
		
		The address is the property of the caller and the caller shall take 
		responsibility for the memory associated with it.
	*/
	void addAddress(const Address *addr);
	
	/**
		Inserts a set of addresses into this interface.
		
		The addresses are the property of the caller and the caller shall take 
		responsibility for the memory associated with it.
	*/
	void addAddresses(const Addresses *adds);
	
	/**
		Returns a list of the addresses referring to this interface.
		
		The returned list is the property of the interface.
	*/
	const Addresses *getAddresses() const;
	
	/**
		Returns true iff the interface has an address that is equal to the given
		address.
	*/
	bool hasAddress(const Address &add) const;
	
	/**
		Returns the first found address of the given type.
		
		May return NULL, if no such address was found.
		
		The returned address is the property of the interface.
	*/
	Address *getAddressByType(AddressType_t type);
	
	/** 
		Sets a given flag.
	*/
	void setFlag(const flag_t flag);

	/** 
		Resets a given flag.
	*/
	void resetFlag(const flag_t flag);
	/** 
		Generic flag set check.
	*/
	bool isFlagSet(const flag_t flag) const;
	/** 
		Is a flag set or not?
	*/
	bool isStored() const;

	/**
		Returns true iff this is a local interface.
	*/
	bool isLocal() const;
	/**
		Returns true iff this is an application interface.
	*/
	bool isApplication() const;
	
	/**
		Sets this interface as being up. 
		
		Interfaces are assumed to be down when they are created. (Connectivities
		should NOT call this function before inserting the interface using
		report_interface. See ConnectivityManager::report_interface for an 
		explanation.)
		
		Also sends a notification of this to haggle if the value changed.
	*/
	void up();
	/**
		Sets this interface as being down. 
		
		Also sends a notification of this to haggle if the value changed.
	*/
	void down();
	/**
		Returns true iff this interface is up.
	*/
	bool isUp() const;
	/**
		Return true iff this interface was found by snooping, i.e.,
		it was not detected by a connectivity.
	*/
	bool isSnooped() const;
	/**
		Returns true if the type and identifier match.
	*/
	bool equal(const InterfaceType_t type, const char *identifier) const;
	/**
		Equality operator.
	*/
	friend bool operator==(const Interface& i1, const Interface& i2);
	friend bool operator<(const Interface& i1, const Interface& i2);
};


typedef Reference<Interface> InterfaceRef;
typedef ReferenceList<Interface> InterfaceRefList;

#endif /* _INTERFACE_H */
