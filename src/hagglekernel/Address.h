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
#ifndef _ADDRESS_H
#define _ADDRESS_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/
class ProtocolSpec;
class ProtocolSpecs;
class Address;
class Addresses;

#include <libcpphaggle/Platform.h>
#include <libcpphaggle/List.h>

#include "Debug.h"

using namespace haggle;

typedef enum {
	ProtocolSpecType_None,
	ProtocolSpecType_RFCOMM,
	ProtocolSpecType_TCP,
	ProtocolSpecType_UDP
} ProtocolSpecType_t;

typedef enum {
	AddressType_EthMAC,
	AddressType_BTMAC,
	AddressType_FilePath,
#if defined(ENABLE_IPv6)
	AddressType_IPv6,
#endif
	AddressType_IPv4
} AddressType_t;

// Address lengths
#define ETH_ALEN	6
#define BT_ALEN		6
#if defined(ENABLE_IPv6)
#define IPv6_ALEN	16
#endif
#define IPv4_ALEN	4

#if defined(ENABLE_IPv6)
#define MAX_ALEN	IPv6_ALEN
#else
#define MAX_ALEN	ETH_ALEN
#endif

/**
	Address class.
	
	Stores an address to a local or remote node.
	
	This class is supposed to be readonly. Once created it should not be 
	modified, other than to add protocols as part of it's creation.
*/
class Address 
#ifdef DEBUG_LEAKS
	: LeakMonitor
#endif
{
	AddressType_t aType;
	/*
		These addresses are stored "Network endian".
	*/
	union {
		unsigned char EthMAC[ETH_ALEN];
		unsigned char BTMAC[BT_ALEN];
		unsigned char IPv4[IPv4_ALEN];
#if defined(ENABLE_IPv6)
		unsigned char IPv6[IPv6_ALEN];
#endif
		char *path;
	};
	bool has_broadcast;
	union {
		unsigned char EthMAC_broadcast[ETH_ALEN];
		unsigned char BTMAC_broadcast[BT_ALEN];
		unsigned char IPv4_broadcast[IPv4_ALEN];
#if defined(ENABLE_IPv6)
		unsigned char IPv6_broadcast[IPv6_ALEN];
#endif
	};
	
	ProtocolSpecType_t pType;
	union {
		unsigned short tcp_port;
		unsigned short udp_port;
		unsigned short rfcomm_channel;
	};

	char *addr_str;
	char *broadcast_str;
	char *uri_str;

	/**
		Private function to create address and broadcast address string.
	*/
	void create_addr_str(void);
	/**
		Private function to create address URI string.
	*/
	void create_uri_str(void);
public:
	/**
		Returns the length of a kind of address, in bytes.
		
		Returns -1 if the length is dynamic.
	*/
	static long getAddressLength(AddressType_t type);
	
	/**
		Creates an address. This can be any kind of address.
		
		Address is expected to be in binary format, in network endian.
		
		portOrChannel is expected to be in host endian.
		
		The given addresses are the property of the caller.
	*/
	Address(AddressType_t _type, 
		void *Address, 
		void *Broadcast = NULL,
		ProtocolSpecType_t protocolType = ProtocolSpecType_None, 
		unsigned short portOrChannel = 0);
	
	/**
		Creates an IPv4 or IPv6 address as defined by the socket address type.
		
		This function ignores any port number or other information besides the 
		actual IP address in the sockaddr_in structs.
		
		portOrChannel is expected to be in host endian.
		
		The given addresses are the property of the caller.
	*/
	Address(struct sockaddr *ip, 
		struct sockaddr *ip_broadcast = NULL, 
		ProtocolSpecType_t protocolType = ProtocolSpecType_None, 
		unsigned short portOrChannel = 0);
	
	/**
		If given something formatted the same way as returned by getURI, it
		creates an address from that URI. If not, this creates a file reference
		address.
		
		portOrChannel is expected to be in host endian.
		
		Information in a URI overrides protocolType/portOrChannel.
		
		The given address is the property of the caller.
	*/
	Address(const char *path, ProtocolSpecType_t protocolType = ProtocolSpecType_None, 
		unsigned short portOrChannel = 0);
	
	/**
		Copy constructor.
	*/
	Address(const Address &_original);
	/**
		Destructor.
	*/
	~Address();
	
	/**
		Merges in the information from the given address into this address.
		
		This function is for internal interface module usage. Do not use it 
		outside the interface module.
	*/
	void mergeWith(const Address *addr);
	/**
		Returns a copy to this address.
	*/
	Address *copy(void) const;
	/**
		Adds a protocol to this address.
		
		The protocol is the property of the caller.
	*/
	void addProtocol(ProtocolSpec *proto);
	/**
		Returns the first found protocol of the given type.
		
		May return NULL, if no such address was found.
		
		The returned address is the property of the address.
	*/
	ProtocolSpec *getProtocolByType(ProtocolSpecType_t type);
	/**
		Returns the type of this address.
	*/
	AddressType_t getType(void) const;
	/**
		Returns the type of the protocol associated with this address.
	*/
	ProtocolSpecType_t getProtocolType(void) const;
	/**
		Returns the port or channel for the protocol associated with this 
		address.
		
		If none is set, this function will return 0.
	*/
	unsigned short getProtocolPortOrChannel(void) const;
	/**
		Returns the length of this address in bytes.
	*/
	long getLength(void) const;
	/**
		Returns a pointer to the raw, binary, address. It's only as long as 
		getLength() says it is.
	*/
	const char *getRaw(void) const;
	/**
		Returns a pointer to the raw, binary, broadcast address. It's only as 
		long as getLength() says it is.
		
		Returns NULL if there is no broadcast address set.
	*/
	const char *getRawBroadcast(void) const;
	/**
		Returns a pointer to a readable form of the address.
		
		The returned pointer is the property of the address.
	*/
	const char *getAddrStr(void) const;
	/**
		Returns a pointer to a readable form of the broadcast address, if one 
		exists.
		
		The returned pointer is the property of the address.
		
		May return NULL, if no broadcast address is set.
	*/
	const char *getBroadcastStr(void) const;
	/**
		Returns a pointer to a URI form of the address.
		
		The returned pointer is the property of the address.
	*/
	const char *getURI(void) const;
	/**
		Returns true iff a broadcast address is available.
	*/
	bool hasBroadcast(void) const;
	
	/**
		If this is an IPv4 or IPv6 address, this function fills in a correct
		struct sockaddr_in or struct sockaddr_in6 struct from the information
		in this address. It then returns the size of the struct it filled in.
		
		If the address has a associated port number for UDP or TCP, this 
		function will fill that port number in.
		
		If this is not an IPv4 or IPv6 address, this function does nothing and
		returns 0.
		
		If "port" is anything but 0, the function will fill in that value 
		instead of the value in the address. This value is expected to be in
		host endian.
		
		"sa" has to have space enough to fill in the correct struct.
	*/
	socklen_t fillInSockaddr(struct sockaddr *sa, unsigned short port = 0) const;
	
	/**
		If this is an IPv4 or IPv6 address, and it has a broadcast address set,
		this function fills in a correct struct sockaddr_in or struct 
		sockaddr_in6 struct from the information in the brodcast address. It 
		then returns the size of the struct it filled in.
		
		If the address has a associated port number for UDP or TCP, this 
		function will fill that port number in. If not, 0 will be set.
		
		If this is not an IPv4 or IPv6 address, this function does nothing and
		returns 0.
		
		If "port" is anything but 0, the function will fill in that value 
		instead of the value in the address. This value is expected to be in
		host endian.
		
		"sa" has to have space enough to fill in the correct struct.
	*/
	socklen_t fillInBroadcastSockaddr(struct sockaddr *sa, unsigned short port = 0);
	/**
		Equality operator.
	*/
	friend bool operator==(const Address &i1, const Address &i2);
};

class Addresses : public List<Address *>
{
public:
	Addresses() {}
	Addresses(const Addresses& adds);
	~Addresses();
	void add(Address *a) { push_back(a); }
	Addresses *copy() const;
	Address *pop();
};

#endif

