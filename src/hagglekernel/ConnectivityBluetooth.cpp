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

#include <libcpphaggle/Platform.h>

#if defined(ENABLE_BLUETOOTH)

#include "ConnectivityBluetooth.h"

ConnectivityBluetooth::ConnectivityBluetooth(ConnectivityManager *m, const InterfaceRef& iface) :
	ConnectivityBluetoothBase(m, iface, "Bluetooth connectivity")
{
	LOG_ADD("%s: Bluetooth connectivity starting. Scan time: %ld +- %ld seconds\n",
		Timeval::now().getAsString().c_str(),
		BASE_TIME_BETWEEN_SCANS,
		RANDOM_TIME_AMOUNT);
}

ConnectivityBluetooth::~ConnectivityBluetooth()
{
	LOG_ADD("%s: Bluetooth connectivity stopped.\n",
		Timeval::now().getAsString().c_str());
}

ConnectivityBluetoothBase::ConnectivityBluetoothBase(ConnectivityManager *m, const InterfaceRef& iface, const string name) :
	Connectivity(m, iface, name)
{
}

ConnectivityBluetoothBase::~ConnectivityBluetoothBase()
{

}

Mutex ConnectivityBluetoothBase::sdpListMutex;
InterfaceRefList ConnectivityBluetoothBase::sdpWhiteList;
InterfaceRefList ConnectivityBluetoothBase::sdpBlackList;
bool ConnectivityBluetoothBase::ignoreNonListedInterfaces = false;

int ConnectivityBluetoothBase::classifyAddress(const Interface &iface)
{
	Mutex::AutoLocker l(sdpListMutex);
	
	for (InterfaceRefList::iterator it = sdpWhiteList.begin();
		it != sdpWhiteList.end(); it++) {
		if (*it == iface)
			return BLUETOOTH_ADDRESS_IS_HAGGLE_NODE;
	}
	if (ignoreNonListedInterfaces)
		return BLUETOOTH_ADDRESS_IS_NOT_HAGGLE_NODE;
	
	for (InterfaceRefList::iterator it = sdpBlackList.begin();
		it != sdpBlackList.end(); it++) {
		if (*it == iface)
			return BLUETOOTH_ADDRESS_IS_NOT_HAGGLE_NODE;
	}
	return BLUETOOTH_ADDRESS_IS_UNKNOWN;
}

int ConnectivityBluetoothBase::classifyAddress(const InterfaceType_t type, const unsigned char *identifier)
{
	Interface iface(type, identifier);
	
	return classifyAddress(iface);
}

void ConnectivityBluetoothBase::clearSDPLists(void)
{
	Mutex::AutoLocker l(sdpListMutex);
	sdpBlackList.clear();
	sdpWhiteList.clear();
}

void ConnectivityBluetoothBase::updateSDPLists(Metadata *md)
{
	Mutex::AutoLocker l(sdpListMutex);
	/*
	 Check bluetooth module blacklisting/whitelisting data. Formatted like so:
	 
	 <Bluetooth>
	 <ClearBlacklist/>
	 <ClearWhitelist/>
	 <Blacklist>
	 <Interface>bt://11:22:33:44:55:66</interface>
	 </Blacklist>
	 <Whitelist>
	 <Interface>bt://77:88:99:AA:BB:CC</interface>
	 </Whitelist>
	 <IgnoreNonListedInterfaces>yes</IgnoreNonListedInterfaces>
	 </Bluetooth>
	 
	 Of course, all components of the bluetooth section are optional, and it 
	 is possible to insert any number of blacklisted/whitelisted interfaces.
	 
	 It is important for the interface addresses to have the bt:// prefix. If 
	 they do not, then the interfaces will not be accepted.
	 */
	
	Metadata *bl = md->getMetadata("Blacklist");
	Metadata *wl = md->getMetadata("Whitelist");
	Metadata *ignore = md->getMetadata("IgnoreNonListedInterfaces");
	
	Metadata *cbl = md->getMetadata("ClearBlacklist");
	Metadata *cwl = md->getMetadata("ClearWhitelist");
	
	if (cbl)
		sdpBlackList.clear();
	
	if (cwl)
		sdpWhiteList.clear();
	
	if (bl)  {
		InterfaceRef i;		
		Metadata *iface = bl->getMetadata("Interface");
		
		while (iface) {
			Address a(iface->getContent().c_str());
			if (a.getType() == AddressType_BTMAC) {
				i = new Interface(IFTYPE_BLUETOOTH, a.getRaw(), &a);
				sdpBlackList.push_back(i);
				iface = bl->getNextMetadata();
			}
		}
	}
	
	if (wl) {
		InterfaceRef i;
		Metadata *iface = wl->getMetadata("Interface");
		
		while (iface) {
			Address a(iface->getContent().c_str());
			if (a.getType() == AddressType_BTMAC) {
				i = new Interface(IFTYPE_BLUETOOTH, a.getRaw(), &a);
				sdpWhiteList.push_back(i);
				iface = wl->getNextMetadata();
			}
		}
	}
	
	if (ignore) {
		if (ignore->getContent() == "yes")
			ignoreNonListedInterfaces = true;
		else if(ignore->getContent() == "no")
			ignoreNonListedInterfaces = false;
		else
			HAGGLE_ERR("IgnoreNonListedInterfaces content wrong. Must be yes/no.\n");
	}
	
	// For debugging purposes:
	/*
	 HAGGLE_DBG("Whitelist:\n");
	 for(InterfaceRefList::iterator it = sdpWhiteList.begin();
	 it != sdpWhiteList.end();
	 it++)
	 {
	 HAGGLE_DBG("    %s\n", (*it)->getIdentifierStr());
	 }
	 HAGGLE_DBG("Blacklist:\n");
	 for(InterfaceRefList::iterator it = sdpBlackList.begin();
	 it != sdpBlackList.end();
	 it++)
	 {
	 HAGGLE_DBG("    %s\n", (*it)->getIdentifierStr());
	 }
	 HAGGLE_DBG("Ignore non-listed: %s\n", ignoreNonListedInterfaces?"yes":"no");
	 */
}

#endif
