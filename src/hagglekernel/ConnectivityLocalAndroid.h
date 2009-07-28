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
#ifndef _CONNECTIVITYLOCALANDROID_H_
#define _CONNECTIVITYLOCALANDROID_H_

#ifndef  _IN_CONNECTIVITYLOCAL_H
#error "Do not include this file directly, include ConnectivityLocal.h"
#endif

#include <libcpphaggle/Platform.h>
#include "Connectivity.h"
#include "Interface.h"

#if defined(ENABLE_ETHERNET)

// Include Android property service
#include <cutils/properties.h>

#if defined(ENABLE_TI_WIFI)
/*----- TI API includes -----*/
//#include "osDot11.h"
//#include "802_11Defs.h"
#include "TI_IPC_Api.h"
#include "TI_AdapterApiC.h"
#include "tiioctl.h"
//#include "shlist.h"

#define TI_WIFI_DEV_NAME "tiwlan0"

struct ti_wifi_handle {
        uint8_t own_addr[ETH_ALEN];     
        TI_HANDLE h_adapter;                
        TI_HANDLE h_events[IPC_EVENT_MAX];
        Interface *iface;
};

#endif /* ENABLE_TI_WIFI */

#endif /* ENABLE_ETHERNET */

#if defined(ENABLE_BLUETOOTH)
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

struct hci_handle {
        int sock;
        struct sockaddr_hci addr;
        struct hci_filter flt;
};
#endif

/**
	Local connectivity module

	This module scans the local hardware/software to find
	bluetooth/ethernet/etc. interfaces that are accessible, and tells the
	connectivity manager about them when they are detected.
*/
class ConnectivityLocal : public ConnectivityLocalBase
{
private:
        int uevent_fd;
        int uevent_init();
        void uevent_close();
        void read_uevent();
#if defined(ENABLE_BLUETOOTH)
        struct hci_handle hcih;
        int read_hci();
        void findLocalBluetoothInterfaces();
        bool set_piscan_mode;
        int dev_id;
#endif
        char wifi_iface_name[PROPERTY_VALUE_MAX];
#if defined(ENABLE_WPA_EVENTS)
        struct wpa_ctrl *wpah;
        void read_wpa_event();
        int wpa_handle_init();
        void wpa_handle_close();
#endif
#if defined(ENABLE_ETHERNET)
        long ethernet_interfaces_found;
        
#if defined(ENABLE_TI_WIFI)
        struct ti_wifi_handle tih;
        friend void ti_wifi_event_receive(IPC_EV_DATA *pData);
        int ti_wifi_handle_init();
        void ti_wifi_handle_close();
        int ti_wifi_try_get_local_mac(unsigned char *mac);
        void ti_wifi_event_handle(IPC_EV_DATA *pData);
#endif
        void findLocalEthernetInterfaces();
#endif

        // Called when add_interface _actually_ adds an interface
        //void interface_added(const Interface *node);

        bool run();
        void hookCleanup();
public:

        ConnectivityLocal(ConnectivityManager *m);
        ~ConnectivityLocal();
};

#endif
