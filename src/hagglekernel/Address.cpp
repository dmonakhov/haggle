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

#include "Address.h"
#include "Debug.h"

long Address::getAddressLength(AddressType_t aType)
{
	switch(aType) {
                case AddressType_EthMAC:
                        return ETH_ALEN;
                        break;

                case AddressType_BTMAC:
                        return BT_ALEN;
                        break;

                case AddressType_FilePath:
                        return -1;
                        break;

#if defined(ENABLE_IPv6)
                case AddressType_IPv6:
                        return IPv6_ALEN;
                        break;
#endif
                case AddressType_IPv4:
                        return IPv4_ALEN;
                        break;

                default:
                        return -1;
                        break;
	}
}

void Address::create_addr_str(void)
{
	switch(aType) {
                case AddressType_EthMAC:
                        addr_str = (char *) malloc(ETH_ALEN*3);
                        sprintf(addr_str,
                                "%02X:%02X:%02X:%02X:%02X:%02X",
                                EthMAC[0],
                                EthMAC[1],
                                EthMAC[2],
                                EthMAC[3],
                                EthMAC[4],
                                EthMAC[5]);

                        if (has_broadcast) {
                                broadcast_str = (char *) malloc(ETH_ALEN*3);
                                sprintf(broadcast_str,
                                        "%02X:%02X:%02X:%02X:%02X:%02X",
                                        EthMAC_broadcast[0],
                                        EthMAC_broadcast[1],
                                        EthMAC_broadcast[2],
                                        EthMAC_broadcast[3],
                                        EthMAC_broadcast[4],
                                        EthMAC_broadcast[5]);
                        }
                        break;

                case AddressType_BTMAC:
                        addr_str = (char *) malloc(BT_ALEN*3);
                        sprintf(addr_str,
                                "%02X:%02X:%02X:%02X:%02X:%02X",
                                BTMAC[0],
                                BTMAC[1],
                                BTMAC[2],
                                BTMAC[3],
                                BTMAC[4],
                                BTMAC[5]);

                        if (has_broadcast) {
                                broadcast_str = (char *) malloc(BT_ALEN*3);
                                sprintf(broadcast_str,
                                        "%02X:%02X:%02X:%02X:%02X:%02X",
                                        BTMAC_broadcast[0],
                                        BTMAC_broadcast[1],
                                        BTMAC_broadcast[2],
                                        BTMAC_broadcast[3],
                                        BTMAC_broadcast[4],
                                        BTMAC_broadcast[5]);
                        }
                        break;

                case AddressType_FilePath:
                        // Do nothing. The associated get function handles this case.
                        break;

#if defined(ENABLE_IPv6)
                case AddressType_IPv6:
                        addr_str = (char *) malloc(IPv6_ALEN*3);
                        sprintf(addr_str,
                                "%02X:%02X:%02X:%02X:"
                                "%02X:%02X:%02X:%02X:"
                                "%02X:%02X:%02X:%02X:"
                                "%02X:%02X:%02X:%02X",
                                IPv6[ 0], IPv6[ 1], IPv6[ 2], IPv6[ 3],
                                IPv6[ 4], IPv6[ 5], IPv6[ 6], IPv6[ 7],
                                IPv6[ 8], IPv6[ 9], IPv6[10], IPv6[11],
                                IPv6[12], IPv6[13], IPv6[14], IPv6[15]);
                        if (has_broadcast) {
                                broadcast_str = (char *) malloc(IPv6_ALEN*3);
                                sprintf(broadcast_str,
                                        "%02X:%02X:%02X:%02X:"
                                        "%02X:%02X:%02X:%02X:"
                                        "%02X:%02X:%02X:%02X:"
                                        "%02X:%02X:%02X:%02X",
                                        IPv6_broadcast[ 0], IPv6_broadcast[ 1], 
                                        IPv6_broadcast[ 2], IPv6_broadcast[ 3],
                                        IPv6_broadcast[ 4], IPv6_broadcast[ 5], 
                                        IPv6_broadcast[ 6], IPv6_broadcast[ 7],
                                        IPv6_broadcast[ 8], IPv6_broadcast[ 9], 
                                        IPv6_broadcast[10], IPv6_broadcast[11],
                                        IPv6_broadcast[12], IPv6_broadcast[13], 
                                        IPv6_broadcast[14], IPv6_broadcast[15]);
                        }
                        break;
#endif

                case AddressType_IPv4:
                        addr_str = (char *) malloc((3 + 1)*4);
                        sprintf(addr_str, "%d.%d.%d.%d", IPv4[0], IPv4[1], IPv4[2], IPv4[3]);

                        if (has_broadcast){
                                broadcast_str = (char *) malloc((3 + 1)*4);
                                sprintf(broadcast_str, "%d.%d.%d.%d",
                                        IPv4_broadcast[0], IPv4_broadcast[1], 
                                        IPv4_broadcast[2], IPv4_broadcast[3]);
                        }
                        break;

                default:
                        // Can't really get here, but still...
                        break;
	}
}

#define NUMBER_OF_URI_PREFIX_STRINGS 7
static char *prefix_str[NUMBER_OF_URI_PREFIX_STRINGS] =
{
	(char *) "file://",
                        (char *) "tcp://",
                        (char *) "udp://",
                        (char *) "rfcomm://",
                        (char *) "eth://",
                        (char *) "bt://",
                        (char *) "ip://"
                        };

void Address::create_uri_str(void)
{
	if (!addr_str)
		create_addr_str();

	switch (aType) {
                case AddressType_EthMAC:
                        uri_str = (char *) malloc(strlen(prefix_str[4]) + 
                                                  strlen(addr_str) +
                                                  1);
                        sprintf(uri_str,
                                "%s%s",
                                prefix_str[4],
                                addr_str);
                        break;

                case AddressType_BTMAC:
                        switch(pType) {
                                case ProtocolSpecType_RFCOMM:
                                        uri_str = (char *) malloc(strlen(prefix_str[3]) + 
                                                                  strlen(addr_str) +
                                                                  1 + 5 + 1);
                                        sprintf(uri_str,
                                                "%s%s:%u",
                                                prefix_str[3],
                                                addr_str,
                                                getProtocolPortOrChannel());
                                        break;
                                default:
                                        uri_str = (char *) malloc(strlen(prefix_str[5]) + 
                                                                  strlen(addr_str) +
                                                                  1);
                                        sprintf(uri_str,
                                                "%s%s",
                                                prefix_str[5],
                                                addr_str);
                                        break;
                        }
                        break;

                case AddressType_IPv4:
#if defined(ENABLE_IPv6)
                case AddressType_IPv6:
#endif
                        switch(pType) {
                                case ProtocolSpecType_TCP:
                                        uri_str = (char *)malloc(strlen(prefix_str[1]) + 
                                                                 strlen(addr_str) +
                                                                 1 + 5 + 1);
                                        sprintf(uri_str,
                                                "%s%s:%u",
                                                prefix_str[1],
                                                addr_str,
                                                getProtocolPortOrChannel());
                                        break;
                                        
                                case ProtocolSpecType_UDP:
                                        uri_str =(char *)malloc(strlen(prefix_str[2]) + 
                                                                strlen(addr_str) +
                                                                1 + 5 + 1);
                                        sprintf(uri_str,
                                                "%s%s:%u",
                                                prefix_str[2],
                                                addr_str,
                                                getProtocolPortOrChannel());
                                        break;

                                default:
                                        uri_str = (char *)malloc(strlen(prefix_str[6]) + 
                                                                 strlen(addr_str) +
                                                                 1);
                                        sprintf(uri_str,
                                                "%s%s",
                                                prefix_str[6],
                                                addr_str);
                                        break;
                        }
                        break;

                case AddressType_FilePath:
                        uri_str = (char *)malloc(strlen(prefix_str[0]) + 
                                                 strlen(path) + 1);
                        sprintf(uri_str,
                                "%s%s",
                                prefix_str[0],
                                addr_str);
                        break;
	}
}

Address::Address(AddressType_t addressType, 
		 void *Address, 
		 void *Address_Broadcast, 
		 ProtocolSpecType_t protocolType, 
		 unsigned short portOrChannel) :
#ifdef DEBUG_LEAKS
                LeakMonitor(LEAK_TYPE_ADDRESS),
#endif
                aType(addressType), has_broadcast(false), pType(protocolType), 
                addr_str(NULL), broadcast_str(NULL), uri_str(NULL)
{
	switch(aType) {
		case AddressType_EthMAC:

#if HAVE_EXCEPTION
			if (Address == NULL)
				throw Exception(0, "Address required");
#endif

			memcpy(EthMAC, Address, ETH_ALEN);
			if (Address_Broadcast != NULL) {
				memcpy(EthMAC_broadcast, Address_Broadcast, ETH_ALEN);
				has_broadcast = true;
			}
			break;

		case AddressType_BTMAC:
#if HAVE_EXCEPTION
			if (Address == NULL)
				throw Exception(0, "Address required");
#endif
			memcpy(BTMAC, Address, BT_ALEN);
			if (Address_Broadcast != NULL) {
				memcpy(BTMAC_broadcast, Address_Broadcast, BT_ALEN);
				has_broadcast = true;
			}
			break;

		case AddressType_FilePath:
#if HAVE_EXCEPTION
			if (Address == NULL)
				throw Exception(0, "Address required");
#endif
			path = (char *) malloc(strlen((const char *)Address) + 1);

#if HAVE_EXCEPTION
			if (path == NULL)
				throw Exception(0, "Unable to allocate path");
#endif
			strcpy(path, (const char *)Address);
			break;

#if defined(ENABLE_IPv6)
		case AddressType_IPv6:
#if HAVE_EXCEPTION
			if (Address == NULL)
				throw Exception(0, "Address required");
#endif
			memcpy(IPv6, Address, IPv6_ALEN);

			if (Address_Broadcast != NULL) {
				memcpy(IPv6_broadcast, Address_Broadcast, IPv6_ALEN);
				has_broadcast = true;
			}
			break;
#endif 
		case AddressType_IPv4:

#if HAVE_EXCEPTION
			if (Address == NULL)
				throw Exception(0, "Address required");
#endif
			memcpy(IPv4, Address, IPv4_ALEN);

			if (Address_Broadcast != NULL) {
				memcpy(IPv4_broadcast, Address_Broadcast, IPv4_ALEN);
				has_broadcast = true;
			}
			break;

		default:
#if HAVE_EXCEPTION
			throw Exception(0, "Unknown address type");
#endif
			break;
	}

	switch (pType) {
		case ProtocolSpecType_None:
			tcp_port = 0;
			break;

		case ProtocolSpecType_RFCOMM:
			rfcomm_channel = portOrChannel;
			break;

		case ProtocolSpecType_TCP:
			tcp_port = portOrChannel;
			break;

		case ProtocolSpecType_UDP:
			udp_port = portOrChannel;
			break;
	}
	// Just to make sure
	create_addr_str();
	create_uri_str();
}

Address::Address(struct sockaddr *ip, struct sockaddr *ip_broadcast, 
		 ProtocolSpecType_t protocolType, 
		 unsigned short portOrChannel) :
#ifdef DEBUG_LEAKS
                LeakMonitor(LEAK_TYPE_ADDRESS),
#endif
		aType(AddressType_IPv4), has_broadcast(false), pType(protocolType), 
                addr_str(NULL), broadcast_str(NULL), uri_str(NULL)
{

#if HAVE_EXCEPTION
	if (ip == NULL)
		throw Exception(0, "Address required");
#endif
	if (ip->sa_family == AF_INET) {
		aType = AddressType_IPv4;
		memcpy(IPv4, &(((struct sockaddr_in *)ip)->sin_addr.s_addr), IPv4_ALEN);
	
		if (ip_broadcast != NULL) {
			memcpy(IPv4_broadcast, &(((struct sockaddr_in *)ip_broadcast)->sin_addr), IPv4_ALEN);
			has_broadcast = true;
		}
	}
#if defined(ENABLE_IPv6)
	else if (ip->sa_family == AF_INET6) {
		aType = AddressType_IPv6;
		memcpy(IPv6, &(((struct sockaddr_in6 *)ip)->sin6_addr), IPv6_ALEN);
		
		if (ip_broadcast != NULL) {
			memcpy(IPv6_broadcast, &(((struct sockaddr_in6 *)ip_broadcast)->sin6_addr), IPv6_ALEN);
			has_broadcast = true;
		}
	}
#endif
	else {
#if HAVE_EXCEPTION
		throw Exception(0, "Address required");
#endif	
	}
	switch(pType)
	{
                case ProtocolSpecType_None:
                        tcp_port = 0;
                        break;

                case ProtocolSpecType_RFCOMM:
                        rfcomm_channel = portOrChannel;
                        break;

                case ProtocolSpecType_TCP:
                        tcp_port = portOrChannel;
                        break;

                case ProtocolSpecType_UDP:
                        udp_port = portOrChannel;
                        break;
	}

	// Just to make sure
	create_addr_str();
	create_uri_str();
}

Address::Address(const char *_path, ProtocolSpecType_t protocolType, unsigned short portOrChannel) :
#ifdef DEBUG_LEAKS 
                LeakMonitor(LEAK_TYPE_ADDRESS),
#endif
                has_broadcast(false), addr_str(NULL), broadcast_str(NULL), uri_str(NULL)
{
	long i;
        
#if HAVE_EXCEPTION
	if (_path == NULL)
		throw Exception(0, "Address required");
#endif
	for (i = 0; i < NUMBER_OF_URI_PREFIX_STRINGS; i++) {
		if (strncmp(_path, prefix_str[i], strlen(prefix_str[i])) == 0) {
			long prefix_len;
                        
			prefix_len = strlen(prefix_str[i]);
                        
			switch (i) {
                                case 0:
                                        // file:
                                        // The whole rest of the string is the path:
                                        path = (char *) malloc(strlen(&(_path[prefix_len])) + 1);
                                        strcpy(path, &(_path[prefix_len]));
                                        break;
                                        
                                case 1:
                                        {
                                                long j,k;
                                                
                                                // tcp:
                                                pType = ProtocolSpecType_TCP;
                                                // This is a trick to reduce the code size. It's ugly, but
                                                // works just fine:
                                                if (0) {
                                                        case 2:
                                                                // udp:
                                                                pType = ProtocolSpecType_UDP;
                                                }
                                                
                                                // Find the ":" that separates the address from the port 
                                                // number
                                                for (j = strlen(_path); _path[j] != ':' && j > 0; j--)
                                                        ;

#if HAVE_EXCEPTION
                                                if (j <= prefix_len)
                                                        throw Exception(0, "Malformed URI: no port number");
#endif
                                                
                                                tcp_port = atoi(&(_path[j+1]));
                                                
                                                if (0) {
                                                        case 6:
                                                                pType = ProtocolSpecType_None;
                                                                j = strlen(_path);
                                                }
                                                // Try to figure out if this is an IPv4 address or IPv6 
                                                // address:
                                                aType = AddressType_EthMAC;
                                                for (k = prefix_len; k < j && aType == AddressType_EthMAC; k++) {
                                                        if(_path[k] == '.')
                                                                aType = AddressType_IPv4;
#if defined(ENABLE_IPv6) 
                                                        if(_path[k] == ':')
                                                        aType = AddressType_IPv6;
#endif
                                                }

#if HAVE_EXCEPTION
                                                        if (aType == AddressType_EthMAC)
                                                                throw Exception(0, "Malformed URI: not IP address");
#endif
                                                
                                                        if (aType == AddressType_IPv4) {
                                                                long l;
                                                                l = 0;
                                                                for (k = prefix_len; k < j; k++) {
                                                                        if (k == prefix_len || _path[k-1] == '.') {
                                                                                IPv4[l] = atoi(&(_path[k]));
                                                                                l++;
                                                                        }
                                                                }
                                                        }
                                                
#if defined(ENABLE_IPv6)
                                                        if (aType == AddressType_IPv6) {
                                                        long l;
                                                        l = 0;
                                                        for (k = prefix_len; k < j; k++) {
                                                                if (k == prefix_len || _path[k-1] == ':') {
                                                                        IPv6[l] = (unsigned char) strtol(&(_path[k]), NULL, 16);
                                                                        l++;
                                                                }
                                                        }
                                                }
#endif 
                                        }
                                        break;
                                        
                                        // This uses the same trick as above, except it also uses the
                                        // information that the BTMAC and EthMAC addresses are the same
                                        // and that Bluetooth and Ethernet MAC addresses are the same
                                        // length.
                                case 3:
                                        {
                                                long	j,k;
                                                
                                                // rfcomm:
                                                
                                                // Find the ":" that separates the address from the port 
                                                // number
                                                for (j = strlen(_path); _path[j] != ':' && j > 0; j--)
                                                        ;
#if HAVE_EXCEPTION
                                                if (j <= prefix_len)
                                                        throw Exception(0, "Malformed URI: no port number");
#endif
                                                
                                                rfcomm_channel = atoi(&(_path[j+1]));
                                                break;
                                                
                                                if (0) {
                                                        case 4:
                                                                // eth:
                                                                aType = AddressType_EthMAC;
                                                                if (0) {
                                                                        case 5:
                                                                                // bt:
                                                                                aType = AddressType_BTMAC;
                                                                }
                                                                pType = ProtocolSpecType_None;
                                                                j = strlen(_path);
                                                }
                                                {
                                                        long l;
                                                        l = 0;
                                                        for (k = prefix_len; k < j; k++) {
                                                                if (k == prefix_len || _path[k-1] == ':') {
                                                                        BTMAC[l] = (unsigned char) strtol(&(_path[k]), NULL, 16);
                                                                        l++;
                                                                }
                                                        }
                                                }
                                        }
                                        break;
                                        
                                default:
                                        // WHOA! shouldn't be able to get here!
#if HAVE_EXCEPTION
                                        throw Exception(0, "Unknown URI format");
#endif
                                        break;
                        }
                        if (pType == ProtocolSpecType_None && 
                            protocolType != ProtocolSpecType_None) {
                                pType = protocolType;
                                switch(pType) {
                                        case ProtocolSpecType_None:
                                                tcp_port = 0;
                                                break;
                                                
                                        case ProtocolSpecType_RFCOMM:
                                                rfcomm_channel = portOrChannel;
                                                break;
                                                
                                        case ProtocolSpecType_TCP:
                                                tcp_port = portOrChannel;
                                                break;
                                                
                                        case ProtocolSpecType_UDP:
                                                udp_port = portOrChannel;
                                                break;
                                }
                        }
                        // Already done.
                        goto done;
                }
        }
        
        aType = AddressType_FilePath;
        path = (char *) malloc(strlen(_path) + 1);
        strcpy(path, _path);
        
        switch(pType) {
                case ProtocolSpecType_None:
                        tcp_port = 0;
                        break;
                        
                case ProtocolSpecType_RFCOMM:
                        rfcomm_channel = portOrChannel;
                        break;
                        
                case ProtocolSpecType_TCP:
                        tcp_port = portOrChannel;
                        break;
                        
                case ProtocolSpecType_UDP:
                        udp_port = portOrChannel;
                        break;
        }
        
done:
        // Just to make sure
        create_addr_str();
        create_uri_str();
}

Address::Address(const Address &_original) :
#ifdef DEBUG_LEAKS 
                LeakMonitor(LEAK_TYPE_ADDRESS),
#endif 
                aType(_original.aType),
                has_broadcast(_original.has_broadcast),
                pType(_original.pType),
                addr_str(NULL),
                broadcast_str(NULL),
                uri_str(NULL)
{
        switch(aType) {
                case AddressType_EthMAC:
                        memcpy(EthMAC, _original.EthMAC, ETH_ALEN);
                        if (has_broadcast) {
                                memcpy(EthMAC_broadcast, _original.EthMAC_broadcast, ETH_ALEN);
                        }
                        break;
                        
                case AddressType_BTMAC:
                        memcpy(BTMAC, _original.BTMAC, BT_ALEN);
                        if (has_broadcast) {
                                memcpy(BTMAC_broadcast, _original.BTMAC_broadcast, BT_ALEN);
                        }
                        break;
                        
                case AddressType_FilePath:
#if HAVE_EXCEPTION
                        if (_original.path == NULL)
                                throw Exception(0, "Original path == NULL");
#endif
                        path = (char *) malloc(strlen(_original.path) + 1);

#if HAVE_EXCEPTION
                        if (path == NULL)
                                throw Exception(0, "Unable to allocate path");
#endif
                        strcpy(path, _original.path);
                        break;
                        
#if defined(ENABLE_IPv6)
                case AddressType_IPv6:
                        memcpy(IPv6, _original.IPv6, IPv6_ALEN);
                        if (has_broadcast) {
                                memcpy(IPv6_broadcast, _original.IPv6_broadcast, IPv6_ALEN);
                        }
                        break;
#endif
                        
                case AddressType_IPv4:
                        memcpy(IPv4, _original.IPv4, IPv4_ALEN);
                        if (has_broadcast) {
                                memcpy(IPv4_broadcast, _original.IPv4_broadcast, IPv4_ALEN);
                        }
                        break;
                        
                default:
#if HAVE_EXCEPTION
                        throw Exception(0, "Unknown address type");
#endif
                        break;
        }
        
        switch(pType) {
                case ProtocolSpecType_None:
                        tcp_port = 0;
                        break;
                        
                case ProtocolSpecType_RFCOMM:
                        rfcomm_channel = _original.rfcomm_channel;
                        break;
                        
                case ProtocolSpecType_TCP:
                        tcp_port = _original.tcp_port;
                        break;
                        
                case ProtocolSpecType_UDP:
                        udp_port = _original.udp_port;
                        break;
        }
        // Just to make sure
        create_addr_str();
        create_uri_str();
}

Address::~Address()
{
        if (aType == AddressType_FilePath)
                free(path);
        if (addr_str)
                free(addr_str);
        if (broadcast_str)
                free(broadcast_str);
        if (uri_str)
                free(uri_str);
}

void Address::mergeWith(const Address *addr)
{
        // The addresses must already match, or we shouldn't get to here.

        // Does the given address have a broadcast address, but this address does 
        // not?
        if (!has_broadcast && addr->has_broadcast) {
                // Copy broadcast address:
                memcpy(IPv4_broadcast, addr->IPv4_broadcast, addr->getLength());
                create_addr_str();
        }

        // Merge protocol (if none is set):
        if(pType == ProtocolSpecType_None && addr->pType != ProtocolSpecType_None) {
                pType = addr->pType;

                switch(pType) {
                        case ProtocolSpecType_None:
                                tcp_port = 0;
                                break;
                                
                        case ProtocolSpecType_RFCOMM:
                                rfcomm_channel = addr->rfcomm_channel;
                                break;
                                
                        case ProtocolSpecType_TCP:
                                tcp_port = addr->tcp_port;
                                break;
                                
                        case ProtocolSpecType_UDP:
                                udp_port = addr->udp_port;
                                break;
                }
        }
}

Address *Address::copy(void) const
{
        return new Address(*this);
}

AddressType_t Address::getType(void) const
{
        return aType;
}

ProtocolSpecType_t Address::getProtocolType(void) const
{
        return pType;
}

unsigned short Address::getProtocolPortOrChannel(void) const
{
        if(pType == ProtocolSpecType_None)
                return 0;
        return tcp_port;
}

long Address::getLength(void) const
{
        return Address::getAddressLength(aType);
}

const char *Address::getRaw(void) const
{
        switch(aType) {
                default:
                        return (char *) IPv4;
                        break;

                case AddressType_FilePath:
                        return path;
                        break;
        }
}

const char *Address::getRawBroadcast(void) const
{
        if(!has_broadcast)
                return NULL;
        
        switch(aType) {
                default:
                        return (char *) IPv4_broadcast;
                        break;

                case AddressType_FilePath:
                        return NULL;
                        break;
        }
}

const char *Address::getAddrStr(void) const
{
        switch(aType)
        {
                default:
                        return addr_str;
                        break;

                case AddressType_FilePath:
                        return path;
                        break;
        }
}

const char *Address::getBroadcastStr(void) const
{
        if (!has_broadcast)
                return NULL;

        switch(aType) {
                default:
                        return broadcast_str;
                        break;

                case AddressType_FilePath:
                        return NULL;
                        break;
        }
}

const char *Address::getURI(void) const
{
        return uri_str;
}

bool Address::hasBroadcast(void) const
{
        return has_broadcast;
}

socklen_t Address::fillInSockaddr(struct sockaddr *sa, unsigned short port) const
{
        if (aType == AddressType_IPv4) {
                struct sockaddr_in *sa4;

                sa4 = (struct sockaddr_in *) sa;

                memset(sa4, 0, sizeof(*sa4));
                sa4->sin_family = AF_INET;
                memcpy(&(sa4->sin_addr), IPv4, IPv4_ALEN);
                if (port != 0) {
                        sa4->sin_port = htons(port);
                } else {
                        switch(pType) {
                                default:
                                        sa4->sin_port = htons(0);
                                        break;

                                case ProtocolSpecType_TCP:
                                        sa4->sin_port = htons(tcp_port);
                                        break;

                                case ProtocolSpecType_UDP:
                                        sa4->sin_port = htons(udp_port);
                                        break;
                        }
                }
                return sizeof(*sa4);
#if defined(ENABLE_IPv6)
        } else if (aType == AddressType_IPv6) {
                        struct sockaddr_in6 *sa6;

                        sa6 = (struct sockaddr_in6 *) sa;

                        memset(sa6, 0, sizeof(*sa6));
                        sa6->sin6_family = AF_INET6;
#if defined(OS_MACOSX)
        sa6->sin6_len = sizeof((*sa6));
#endif
        memcpy(&(sa6->sin6_addr), IPv6, IPv6_ALEN);
                        if(port != 0)
                        {
                                sa6->sin6_port = htons(port);
                        } else {
                                switch(pType) {
                                        default:
                                                sa6->sin6_port = htons(0);
                                                break;

                                        case ProtocolSpecType_TCP:
                                                sa6->sin6_port = htons(tcp_port);
                                                break;

                                        case ProtocolSpecType_UDP:
                                                sa6->sin6_port = htons(udp_port);
                                                break;
                                }
                        }
                        return sizeof(*sa6);
#endif 
        } else {
                return 0;
        }
}

socklen_t Address::fillInBroadcastSockaddr(struct sockaddr *sa, unsigned short port)
{
        if(!has_broadcast)
                return 0;
	
        if (aType == AddressType_IPv4) {
                struct sockaddr_in *sa4;
                
                sa4 = (struct sockaddr_in *) sa;
                
                memset(sa4, 0, sizeof(*sa4));
                sa4->sin_family = AF_INET;
                memcpy(&(sa4->sin_addr), IPv4_broadcast, IPv4_ALEN);
                if (port != 0) {
                        sa4->sin_port = htons(port);
                } else {
                        switch(pType) {
                                default:
                                        sa4->sin_port = htons(0);
                                        break;
                                        
                                case ProtocolSpecType_TCP:
                                        sa4->sin_port = htons(tcp_port);
                                        break;
                                        
                                case ProtocolSpecType_UDP:
                                        sa4->sin_port = htons(udp_port);
                                        break;
                        }
                }
                return sizeof(*sa4);
#if defined(ENABLE_IPv6)
        } else if (aType == AddressType_IPv6) {
                struct sockaddr_in6 *sa6;
                
                sa6 = (struct sockaddr_in6 *) sa;
                
                memset(sa6, 0, sizeof(*sa6));
                sa6->sin6_family = AF_INET6;
#if defined(OS_MACOSX)
                sa6->sin6_len = sizeof((*sa6));
#endif
                memcpy(&(sa6->sin6_addr), IPv6_broadcast, IPv6_ALEN);
                if (port != 0){
                        sa6->sin6_port = htons(port);
                } else{
                        switch(pType) {
                                default:
                                        sa6->sin6_port = htons(0);
                                        break;
                                        
                                case ProtocolSpecType_TCP:
                                        sa6->sin6_port = htons(tcp_port);
                                        break;
                                        
                                case ProtocolSpecType_UDP:
                                        sa6->sin6_port = htons(udp_port);
                                        break;
                        }
                }
                return sizeof(*sa6);
#endif 
        } else {
                return 0;
        }
}

bool operator==(const Address &i1, const Address &i2)
{
        if(&i1 == NULL && &i2 == NULL)
                return true;
        if(&i1 == NULL || &i2 == NULL)
                return false;
        if(i1.aType != i2.aType)
                return false;
        if(memcmp(i1.IPv4, i2.IPv4, ((Address&) i1).getLength()) != 0)
                return false;
        if(i1.pType == ProtocolSpecType_None || i2.pType == ProtocolSpecType_None)
                return true;
        if(i1.pType != i2.pType)
                return false;

        switch(i1.pType) {
                case ProtocolSpecType_None:
                        break;

                case ProtocolSpecType_RFCOMM:
                        if(i1.rfcomm_channel != i2.rfcomm_channel)
                                return false;
                        break;

                case ProtocolSpecType_TCP:
                        if(i1.tcp_port != i2.tcp_port)
                                return false;
                        break;

                case ProtocolSpecType_UDP:
                        if(i1.udp_port != i2.udp_port)
                                return false;
                        break;
        }

        return true;
}

// Container list
Addresses::Addresses(const Addresses& adds) : List<Address *>()
{
        for (Addresses::const_iterator it = adds.begin();
             it != adds.end();
             it++) {
                push_back((*it)->copy());
        }
}

Addresses::~Addresses()
{
        while (!empty()) {
                delete pop();
        }
}

Addresses *Addresses::copy()
{
        return new Addresses(*this);
}

Address *Addresses::pop()
{
        if (empty())
                return NULL;
        else {
                Address *n = front();
                pop_front();
                return n;
        }
}
