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

#if defined(OS_LINUX) && defined(ENABLE_BLUETOOTH)

#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include <sys/types.h>
#include <sys/socket.h>

// For netlink
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <linux/if.h>
// #include <linux/if_addr.h>
// #include <linux/if_link.h>
#include <linux/if_ether.h>
#include <errno.h>

//  ICMP
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#include "ProtocolRFCOMM.h"
#include <sys/ioctl.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#define MAX_BT_RESPONSES 255

#include "ConnectivityBluetooth.h"

#include "ProtocolRFCOMM.h"
#include <sys/ioctl.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <haggleutils.h>

bdaddr_t bdaddr_any  = {{0, 0, 0, 0, 0, 0}};
bdaddr_t bdaddr_local = {{0, 0, 0, 0xff, 0xff, 0xff}};

//Start haggle service:
/* Parts of this function are based on code from Bluez http://www.bluez.org/, 
which are licensed under the GPL. */
static int add_service(sdp_session_t *session, uint32_t *handle)
{
	int ret = 0;
	unsigned char service_uuid_int[] = HAGGLE_BLUETOOTH_SDP_UUID;
	uint8_t rfcomm_channel = RFCOMM_DEFAULT_CHANNEL;
	const char *service_name = "Haggle";
	const char *service_dsc = "A community oriented communication framework";
	const char *service_prov = "haggleproject.org";

	uuid_t root_uuid; 
	uuid_t rfcomm_uuid, l2cap_uuid, svc_uuid;
	sdp_list_t *root_list;
	sdp_list_t *rfcomm_list = 0, *l2cap_list = 0, *proto_list = 0, *access_proto_list = 0, *service_list = 0;
       
	sdp_data_t *channel = 0;
	sdp_record_t *rec;
	// connect to the local SDP server, register the service record, and
	// disconnect

	if (!session) {
		HAGGLE_DBG("Bad local SDP session\n");
		return -1;
	}
	rec = sdp_record_alloc();

	// set the general service ID
	sdp_uuid128_create(&svc_uuid, &service_uuid_int);
	service_list = sdp_list_append(0, &svc_uuid);
	sdp_set_service_classes(rec, service_list);
	sdp_set_service_id(rec, svc_uuid);

	// make the service record publicly browsable
	sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP); 
	root_list = sdp_list_append(0, &root_uuid); 
	sdp_set_browse_groups(rec, root_list);
	
	// set l2cap information
	sdp_uuid16_create(&l2cap_uuid, L2CAP_UUID);
	l2cap_list = sdp_list_append(0, &l2cap_uuid);
	proto_list = sdp_list_append(0, l2cap_list);

	// set rfcomm information
	sdp_uuid16_create(&rfcomm_uuid, RFCOMM_UUID);
	rfcomm_list = sdp_list_append(0, &rfcomm_uuid);
	channel = sdp_data_alloc(SDP_UINT8, &rfcomm_channel);
	sdp_list_append(rfcomm_list, channel);
	sdp_list_append(proto_list, rfcomm_list);

	// attach protocol information to service record
	access_proto_list = sdp_list_append(0, proto_list);
	sdp_set_access_protos(rec, access_proto_list);

	// set the name, provider, and description
	sdp_set_info_attr(rec, service_name, service_prov, service_dsc);

	ret = sdp_record_register(session, rec, 0);
	
	if (ret < 0) {
		HAGGLE_DBG("Service registration failed\n");
	} else {
		*handle = rec->handle;
	}

	// cleanup
	sdp_data_free(channel);
	sdp_list_free(l2cap_list, 0);	
	sdp_list_free(rfcomm_list, 0);
	sdp_list_free(root_list, 0);
	sdp_list_free(proto_list, 0);
	sdp_list_free(access_proto_list, 0);
	sdp_list_free(service_list, 0);

	sdp_record_free(rec);

	return ret;
}

static int del_service(sdp_session_t *session, uint32_t handle)
{
	uint32_t range = 0x0000ffff;
	sdp_list_t *attr;
	sdp_record_t *rec;
	
	if (!session) {
		CM_DBG("Bad local SDP session!\n");
		return -1;
	}

	attr = sdp_list_append(0, &range);
	rec = sdp_service_attr_req(session, handle, SDP_ATTR_REQ_RANGE, attr);
	sdp_list_free(attr, 0);

	if (!rec) {
		CM_DBG("Service Record not found.\n");
		return -1;
	}

	if (sdp_device_record_unregister(session, &bdaddr_local, rec)) {
		CM_DBG("Failed to unregister service record: %s\n", strerror(errno));
		return -1;
	}

	CM_DBG("Service Record deleted.\n");

	return 0;
}

static int do_search(sdp_session_t *session, uuid_t *uuid)
{
	sdp_list_t *response_list = NULL, *attrid_list, *search_list, *r;
	uint32_t range = 0x0000ffff;
	int err = 0;
	int result = 0;

	search_list = sdp_list_append(0, uuid );
	attrid_list = sdp_list_append(0, &range );

	// perform the search
	err = sdp_service_search_attr_req(session, search_list, SDP_ATTR_REQ_RANGE, attrid_list, &response_list);

	if (err) {
		result = -1;
		goto cleanup;
	}

	// go through each of the service records
	for (r = response_list; r; r = r->next) {
		sdp_record_t *rec = (sdp_record_t*) r->data;
		sdp_list_t *proto_list = NULL;
		
		// get a list of the protocol sequences
		if (sdp_get_access_protos( rec, &proto_list) == 0) {
			sdp_list_t *p = proto_list;
			int port;

			if ((port = sdp_get_proto_port(p, RFCOMM_UUID)) != 0) {
				result = port;
                                printf("Found port %d\n", result);
			} else if ((port = sdp_get_proto_port(p, L2CAP_UUID)) != 0) {
			} else {
			}

			for(; p; p = p->next) {
				sdp_list_free((sdp_list_t*)p->data, 0);
			}
			sdp_list_free(proto_list, 0);
		} 
		sdp_record_free(rec);
	}

cleanup:
	sdp_list_free(response_list, 0);
	sdp_list_free(search_list, 0);
	sdp_list_free(attrid_list, 0);

	return result;
}

static int find_haggle_service(bdaddr_t bdaddr)
{
	const unsigned char svc_uuid_int[] = HAGGLE_BLUETOOTH_SDP_UUID;
	uuid_t svc_uuid;
	char str[20];
	sdp_session_t *sess = NULL; // This session is for the remote sdp server
	int found = 0;

	sess = sdp_connect(&bdaddr_any, &bdaddr, SDP_RETRY_IF_BUSY);

	ba2str(&bdaddr, str);

	if (!sess) {
		HAGGLE_ERR("Failed to connect to SDP server on %s: %s\n", str, strerror(errno));
		return -1;
	}

	sdp_uuid128_create(&svc_uuid, &svc_uuid_int);

	found = do_search(sess, &svc_uuid);
	
	sdp_close(sess);

	return found;
}

void bluetoothDiscovery(ConnectivityBluetooth *conn)
{
        inquiry_info *ii;
	// This isn't being used:
	int num_found = 0;
        int dd, dev_id, ret, i;
	Address *addr;
        InterfaceStatus_t status;
	
	addr = conn->rootInterface->getAddressByType(AddressType_BTMAC);

	if (!addr)
		return;
    
        CM_DBG("Inquiry on interface %s\n", conn->rootInterface->getName());

        dev_id = hci_get_route(NULL);

        dd = hci_open_dev(dev_id);

        if (dev_id < 0 || dd < 0) {
                return;
        }
    
        ii = (inquiry_info *)malloc(sizeof(inquiry_info) * MAX_BT_RESPONSES);

        if (!ii)
                return;
        
        ret = hci_inquiry(dev_id, 8, MAX_BT_RESPONSES, NULL, (inquiry_info **)&ii, IREQ_CACHE_FLUSH);

        if (ret < 0) {
		CM_DBG("Inquiry failed on interface %s\n", conn->rootInterface->getName());
                free(ii);
                return;
        }
        
	for (i = 0; i < ret; i++) {
                bool report_interface = false;
		char macaddr[BT_ALEN];
		int channel = 0;
                char remote_name[256];
                
                strcpy(remote_name, "PeerBluetooth");
                /*
                if (hci_read_remote_name(dd, &ii[i].bdaddr, 256, remote_name, 0) < 0) {
                        fprintf(stderr, "Error looking up name : %s\n", strerror(errno));
                }
                */
		baswap((bdaddr_t *) &macaddr, &ii[i].bdaddr);

		Address addy(AddressType_BTMAC, (unsigned char *) macaddr);
		
                status = conn->is_known_interface(IFTYPE_BLUETOOTH, macaddr);
                
                if (status == INTERFACE_STATUS_HAGGLE) {
                        report_interface = true;
                } else if (status == INTERFACE_STATUS_UNKNOWN) {
                        
                        channel = find_haggle_service(ii[i].bdaddr);
                        
                        if (channel > 0) {
				report_interface = true;
				conn->report_known_interface(IFTYPE_BLUETOOTH, macaddr, true);
			} else if (channel == 0) {
				conn->report_known_interface(IFTYPE_BLUETOOTH, macaddr, false);
			}
                }

		if (report_interface) {
                        Interface iface(IFTYPE_BLUETOOTH, macaddr, &addy, remote_name, IFFLAG_UP);

                        CM_DBG("Found Haggle device [%s - %s]\n", addy.getAddrStr(), remote_name);

			conn->report_interface(&iface, conn->rootInterface, newConnectivityInterfacePolicyTTL2);
			num_found++;
		} else {
			CM_DBG("Device [%s] is not a Haggle device\n", addy.getAddrStr());
		}
	}

	close(dd);
        free(ii);

	CM_DBG("Bluetooth inquiry done! Num discovered=%d\n", num_found);

	return;
}

void ConnectivityBluetooth::hookCleanup()
{
	HAGGLE_DBG("Removing SDP service\n");

	del_service(session, service);
	
	sdp_close(session);
}

void ConnectivityBluetooth::cancelDiscovery(void)
{
	hookStopOrCancel();
	cancel();
}

bool ConnectivityBluetooth::run()
{
	Address *addr = rootInterface->getAddressByType(AddressType_BTMAC);
	
	if (addr) {
		CM_DBG("Bluetooth connectivity detector started for %s\n", addr->getAddrStr());
	} else {
		CM_DBG("Bluetooth connectivity detector started for interface withouth Bluetooth MAC address!\n");
	}

	session = sdp_connect(0, &bdaddr_local, SDP_RETRY_IF_BUSY);

	if (!session) {
		HAGGLE_ERR("Could not connect to local SDP daemon\n");
		return false;
	}

	if (add_service(session, &service) < 0) {
		CM_DBG("Could not add SDP service\n");
		return false;
	} 

	CM_DBG("SDP service handle is %u\n", service);
	
	cancelableSleep(5000);
	
	while (!shouldExit()) {
		
		bluetoothDiscovery(this);
		
		age_interfaces(rootInterface);
		
		cancelableSleep(TIME_TO_WAIT_MSECS);			
	}

	return false;
}
#endif

