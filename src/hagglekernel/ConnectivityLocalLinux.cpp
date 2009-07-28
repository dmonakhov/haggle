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
#include <libcpphaggle/Watch.h>
#include <libcpphaggle/List.h>

#if defined(OS_LINUX)

#ifdef ENABLE_BLUETOOTH
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include "ProtocolRFCOMM.h"
#include <sys/ioctl.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#endif

#if defined(ENABLE_ETHERNET)
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <string.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if_ether.h>
#include <errno.h>
#endif

#if defined(HAVE_DBUS)
#include <dbus/dbus.h>
#endif

#include "ConnectivityManager.h"
#include "ConnectivityLocal.h"
#include "ConnectivityBluetooth.h"
#include "ConnectivityEthernet.h"
#include "Interface.h"

#if defined(ENABLE_BLUETOOTH)
Interface *hci_get_interface_from_name(const char *ifname);
#endif

#if defined(ENABLE_ETHERNET)

static const char *blacklist_device_names[] = { "vmnet", "vmaster", "pan", "lo", "wifi", NULL };

static bool isBlacklistDeviceName(const char *devname)
{
	int i = 0;
        string dname = devname;

        // Assume that the last character is a single digit indicating
        // the device number. Remove it to be able to match exactly
        // against the blacklist names
        dname[dname.length()-1] = '\0';
        
	while (blacklist_device_names[i]) {
		if (strcmp(blacklist_device_names[i], dname.c_str()) == 0)
			return true;
		i++;
	}

	return false;
}

struct if_info {
	int msg_type;
	int ifindex;
	bool isUp;
	bool isWireless;
	char ifname[256];
	char mac[ETH_ALEN];
	struct in_addr ip;
	struct in_addr broadcast;
	struct sockaddr_in ipaddr;
};

#define netlink_getlink(nl) netlink_request(nl, RTM_GETLINK)
#define netlink_getneigh(nl) netlink_request(nl, RTM_GETNEIGH)
#define netlink_getaddr(nl) netlink_request(nl, RTM_GETADDR | RTM_GETLINK)

static int netlink_request(struct netlink_handle *nlh, int type);
//static int read_netlink(struct netlink_handle *nlh, struct if_info *ifinfo);

static int nl_init_handle(struct netlink_handle *nlh)
{
	int ret;
	socklen_t addrlen;

	if (!nlh)
		return -1;

	memset(nlh, 0, sizeof(struct netlink_handle));
	nlh->seq = 0;
	nlh->local.nl_family = PF_NETLINK;
	nlh->local.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;
	nlh->local.nl_pid = getpid();
	nlh->peer.nl_family = PF_NETLINK;

	nlh->sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

	if (!nlh->sock) {
		CM_DBG("Could not create netlink socket");
		return -2;
	}
	addrlen = sizeof(nlh->local);

	ret = bind(nlh->sock, (struct sockaddr *) &nlh->local, addrlen);

	if (ret == -1) {
		close(nlh->sock);
		CM_DBG("Bind for RT netlink socket failed");
		return -3;
	}
	ret = getsockname(nlh->sock, (struct sockaddr *) &nlh->local, &addrlen);

	if (ret < 0) {
		close(nlh->sock);
		CM_DBG("Getsockname failed ");
		return -4;
	}

	return 0;
}

static int nl_close_handle(struct netlink_handle *nlh)
{
	if (!nlh)
		return -1;

	return close(nlh->sock);
}

static int nl_send(struct netlink_handle *nlh, struct nlmsghdr *n)
{
	int res;
	struct iovec iov = {
		(void *) n, n->nlmsg_len
	};
	struct msghdr msg = {
		(void *) &nlh->peer, 
                sizeof(nlh->peer), 
                &iov, 1, NULL, 0, 0
	};

	n->nlmsg_seq = ++nlh->seq;
	n->nlmsg_pid = nlh->local.nl_pid;

	/* Request an acknowledgement by setting NLM_F_ACK */
	n->nlmsg_flags |= NLM_F_ACK;

	/* Send message to netlink interface. */
	res = sendmsg(nlh->sock, &msg, 0);

	if (res < 0) {
		HAGGLE_ERR("error: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

static int nl_parse_link_info(struct nlmsghdr *nlm, struct if_info *ifinfo)
{
	struct rtattr *rta = NULL;
	struct ifinfomsg *ifimsg = (struct ifinfomsg *) NLMSG_DATA(nlm);
	int attrlen = nlm->nlmsg_len - NLMSG_LENGTH(sizeof(struct ifinfomsg));
	int n = 0;

	if (!ifimsg || !ifinfo)
		return -1;

	ifinfo->isWireless = false;
	ifinfo->ifindex = ifimsg->ifi_index;
	ifinfo->isUp = ifimsg->ifi_flags & IFF_UP ? true : false;

	for (rta = IFLA_RTA(ifimsg); RTA_OK(rta, attrlen); rta = RTA_NEXT(rta, attrlen)) {
		if (rta->rta_type == IFLA_ADDRESS) {
			if (ifimsg->ifi_family == AF_UNSPEC) {
				if (RTA_PAYLOAD(rta) == ETH_ALEN) {
					memcpy(ifinfo->mac, (char *) RTA_DATA(rta), ETH_ALEN);
					n++;
				}
			}
		} else if (rta->rta_type == IFLA_IFNAME) {
			strcpy(ifinfo->ifname, (char *) RTA_DATA(rta));
			n++;
		} else if (rta->rta_type == IFLA_WIRELESS) {
			// wireless stuff
			ifinfo->isWireless = true;
		}
	}
	return n;
}
static int nl_parse_addr_info(struct nlmsghdr *nlm, struct if_info *ifinfo)
{
	struct rtattr *rta = NULL;
	struct ifaddrmsg *ifamsg = (struct ifaddrmsg *) NLMSG_DATA(nlm);
	int attrlen = nlm->nlmsg_len - NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	int n = 0;

	if (!ifamsg || !ifinfo)
		return -1;

	ifinfo->ifindex = ifamsg->ifa_index;
	for (rta = IFA_RTA(ifamsg); RTA_OK(rta, attrlen); rta = RTA_NEXT(rta, attrlen)) {
		if (rta->rta_type == IFA_ADDRESS) {
			memcpy(&ifinfo->ipaddr.sin_addr, RTA_DATA(rta), RTA_PAYLOAD(rta));
			ifinfo->ipaddr.sin_family = ifamsg->ifa_family;
		} else if (rta->rta_type == IFA_LOCAL) {
			if (RTA_PAYLOAD(rta) == ETH_ALEN) {
			}
		} else if (rta->rta_type == IFA_LABEL) {
			strcpy(ifinfo->ifname, (char *) RTA_DATA(rta));
		}
	}

	return n;
}

static int get_ipconf(struct if_info *ifinfo)
{
	struct ifreq ifr;
	struct sockaddr_in *sin = (struct sockaddr_in *) &ifr.ifr_addr;
	int sock;

	if (!ifinfo)
		return -1;
	
	sock = socket(PF_INET, SOCK_STREAM, 0);

	memset(&ifr, 0, sizeof(ifr));

	ifr.ifr_ifindex = ifinfo->ifindex;
	strcpy(ifr.ifr_name, ifinfo->ifname);

	if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
		//HAGGLE_ERR("Could not get IP address for %s\n", ifinfo->ifname);
		close(sock);
		return -1;
	}
	memcpy(&ifinfo->ip, &sin->sin_addr, sizeof(struct in_addr));

	if (ioctl(sock, SIOCGIFBRDADDR, &ifr) < 0) {
		//HAGGLE_ERR("Could not get IP address for %s\n", ifinfo->ifname);
		close(sock);
		return -1;
	}
	memcpy(&ifinfo->broadcast, &sin->sin_addr, sizeof(struct in_addr));

	close(sock);

	return 0;
}

int ConnectivityLocal::read_netlink()
{
	int len, num_msgs = 0;
	socklen_t addrlen;
	struct nlmsghdr *nlm;
	struct if_info ifinfo;
#define BUFLEN 2000
	char buf[BUFLEN];

	addrlen = sizeof(struct sockaddr_nl);

	memset(buf, 0, BUFLEN);

	len = recvfrom(nlh.sock, buf, BUFLEN, 0, (struct sockaddr *) &nlh.peer, &addrlen);

	if (len == EAGAIN) {
		CM_DBG("Netlink recv would block\n");
		return 0;
	}
	if (len < 0) {
		CM_DBG("len negative\n");
		return len;
	}
	for (nlm = (struct nlmsghdr *) buf; NLMSG_OK(nlm, (unsigned int) len); nlm = NLMSG_NEXT(nlm, len)) {
		struct nlmsgerr *nlmerr = NULL;
		int ret = 0;

		memset(&ifinfo, 0, sizeof(struct if_info));

		num_msgs++;

		switch (nlm->nlmsg_type) {
		case NLMSG_ERROR:
			nlmerr = (struct nlmsgerr *) NLMSG_DATA(nlm);
			if (nlmerr->error == 0) {
				CM_DBG("NLMSG_ACK");
			} else {
				CM_DBG("NLMSG_ERROR, error=%d type=%d\n", nlmerr->error, nlmerr->msg.nlmsg_type);
			}
			break;
		case RTM_NEWLINK:
			ret = nl_parse_link_info(nlm, &ifinfo);

			/* TODO: Should find a good way to sort out unwanted interfaces. */
			if (ifinfo.isUp && !isBlacklistDeviceName(ifinfo.ifname)) {
				
				if (get_ipconf(&ifinfo) < 0) {
					break;
				}

				if (ifinfo.mac[0] == 0 &&
                                    ifinfo.mac[1] == 0 &&
                                    ifinfo.mac[2] == 0 &&
                                    ifinfo.mac[3] == 0 &&
                                    ifinfo.mac[4] == 0 &&
                                    ifinfo.mac[5] == 0)
                                        break;
                                   
				CM_DBG("Interface newlink %s %s %s\n", 
				       ifinfo.ifname, eth_to_str(ifinfo.mac), 
				       ifinfo.isUp ? "up" : "down");
				
				Addresses addrs;

				addrs.add(new Address(AddressType_EthMAC, ifinfo.mac));
				addrs.add(new Address(AddressType_IPv4, &ifinfo.ip, &ifinfo.broadcast));
				
			 	Interface iface(ifinfo.isWireless ? IFTYPE_WIFI : IFTYPE_ETHERNET, 
						 ifinfo.mac, &addrs, ifinfo.ifname, IFFLAG_LOCAL | IFFLAG_UP);
				
 				ethernet_interfaces_found++;

 				report_interface(&iface, rootInterface, newConnectivityInterfacePolicyAgeless);
			}
			if (!ifinfo.isUp) {
				delete_interface(ifinfo.isWireless ? IFTYPE_WIFI : IFTYPE_ETHERNET, ifinfo.mac);
			}
			break;
		case RTM_DELLINK:
		{
			ret = nl_parse_link_info(nlm, &ifinfo);
			Address mac(AddressType_EthMAC, (unsigned char *) ifinfo.mac);
			CM_DBG("Interface dellink %s %s\n", ifinfo.ifname, mac.getAddrStr());
			// Delete interface here?
			
			delete_interface(ifinfo.isWireless ? IFTYPE_WIFI : IFTYPE_ETHERNET, ifinfo.mac);
		}
			break;
		case RTM_DELADDR:
			ret = nl_parse_addr_info(nlm, &ifinfo);
			CM_DBG("Interface deladdr %s %s\n", ifinfo.ifname, ip_to_str(ifinfo.ipaddr.sin_addr));
			// Delete interface here?
			delete_interface(ifinfo.isWireless ? IFTYPE_WIFI : IFTYPE_ETHERNET, ifinfo.mac);
			break;
		case RTM_NEWADDR:
			ret = nl_parse_addr_info(nlm, &ifinfo);
			CM_DBG("Interface newaddr %s %s\n", ifinfo.ifname, ip_to_str(ifinfo.ipaddr.sin_addr));
			// Update ip address here?
			break;
		case NLMSG_DONE:
			CM_DBG("NLMSG_DONE\n");
			break;
		default:
			CM_DBG("Unknown netlink message\n");
			break;
		}
	}
	return num_msgs;
}

static int netlink_request(struct netlink_handle *nlh, int type)
{
	struct {
		struct nlmsghdr nh;
		struct rtgenmsg rtg;
	} req;

	if (!nlh)
		return -1;

	memset(&req, 0, sizeof(req));
	req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(req));
	req.nh.nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
	req.nh.nlmsg_type = type;
	req.rtg.rtgen_family = AF_INET;

	// Request interface information
	return nl_send(nlh, &req.nh);
}



#define	max(a,b) ((a) > (b) ? (a) : (b))
void ConnectivityLocal::findLocalEthernetInterfaces()
{
	int sock, ret = 0;
#define REQ_BUF_SIZE (sizeof(struct ifreq) * 20)
	struct {
		struct ifconf ifc;
		char buf[REQ_BUF_SIZE];
	} req = { { REQ_BUF_SIZE, { req.buf}}, { 0 } };

	sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock < 0) {
		CM_DBG("Could not open socket\n");
		return;		//-1;
	}
	// This call finds all active interfaces where you can open a UDP socket
	ret = ioctl(sock, SIOCGIFCONF, &req);

	if (ret < 0) {
		CM_DBG("ioctl() failed\n");
		return;		//-1;
	}

	struct ifreq *ifr = (struct ifreq *) req.buf;

	// Goes through all responses
	while (req.ifc.ifc_len) {
		unsigned char macaddr[6];

		int len = (sizeof(ifr->ifr_name) + sizeof(struct sockaddr));

		// Check that the result is an IP adress and that it is an ethernet
		//interface
		if (ifr->ifr_addr.sa_family != AF_INET || strncmp(ifr->ifr_name, "eth", 3) != 0) {
			
			req.ifc.ifc_len -= len;
			ifr = (struct ifreq *) ((char *) ifr + len);

			continue;
		}
		// Get the Ethernet address:
		struct ifreq ifbuf;
		strcpy(ifbuf.ifr_name, ifr->ifr_name);
		ioctl(sock, SIOCGIFHWADDR, &ifbuf);
		memcpy(macaddr, ifbuf.ifr_hwaddr.sa_data, 6);

		struct in_addr ip, broadcast;

                printf("Found interface %02x\n", macaddr[5]);
		memcpy(&ip, &((struct sockaddr_in *) &(ifr->ifr_addr))->sin_addr, sizeof(struct in_addr));

		ioctl(sock, SIOCGIFBRDADDR, &ifbuf);

		memcpy(&broadcast, &((struct sockaddr_in *) &(ifbuf.ifr_broadaddr))->sin_addr, sizeof(struct in_addr));
		
		Addresses addrs;
		addrs.add(new Address(AddressType_EthMAC, macaddr));
		addrs.add(new Address(AddressType_IPv4, &ip, &broadcast));
		
		// Create the interface
		Interface iface(IFTYPE_ETHERNET, macaddr, &addrs, ifr->ifr_name, IFFLAG_LOCAL | IFFLAG_UP);
 
		report_interface(&iface, rootInterface, newConnectivityInterfacePolicyAgeless);

		req.ifc.ifc_len -= len;
		ifr = (struct ifreq *) ((char *) ifr + len);
	}

	close(sock);

	return;			//0;
}
#endif

#if defined(HAVE_DBUS)


/* Main loop integration for D-Bus */
struct watch_data {
	DBusWatch *watch;
	int fd;
	//Watchable *wa;
	int watchIndex;
	void *data;
};
static List<watch_data *> dbusWatches;

void dbus_close_handle(struct dbus_handle *dbh)
{
	dbus_connection_close(dbh->conn);
}

static dbus_bool_t dbus_watch_add(DBusWatch * watch, void *data)
{
	struct watch_data *wd;

	if (!dbus_watch_get_enabled(watch))
		return TRUE;

	wd = new struct watch_data;

	if (!wd)
		return FALSE;

	wd->watch = watch;
	wd->data = data;
#if HAVE_DBUS_WATCH_GET_UNIX_FD
	wd->fd = dbus_watch_get_unix_fd(watch);
#else
	wd->fd = dbus_watch_get_fd(watch);
#endif
	dbusWatches.push_back(wd);

	dbus_watch_set_data(watch, (void *) data, NULL);

	return TRUE;
}

static void dbus_watch_remove(DBusWatch * watch, void *data)
{
	for (List<watch_data *>::iterator it = dbusWatches.begin(); it != dbusWatches.end(); it++) {
		struct watch_data *wd = *it;

		if (wd->watch == watch) {
			dbusWatches.erase(it);
			//delete wd->wa;
			delete wd;
			return;
		}
	}
}

static void dbus_watch_remove_all()
{
	while (!dbusWatches.empty()) {
		struct watch_data *wd = dbusWatches.front();

		dbusWatches.pop_front();
		delete wd;
	}
}

static void dbus_watch_toggle(DBusWatch * watch, void *data)
{
	if (dbus_watch_get_enabled(watch))
		dbus_watch_add(watch, data);
	else
		dbus_watch_remove(watch, data);
}

/*
  Handler function for the D-Bus events we listen to.
 */
DBusHandlerResult dbus_handler(DBusConnection * conn, DBusMessage * msg, void *data)
{
#if defined(ENABLE_BLUETOOTH)
	ConnectivityLocal *cl = static_cast < ConnectivityLocal * >(data);
        /*
          A newer BlueZ D-bus API seems to have moved to a
          PropertyChanced member type message for managing the
          adapter mode. The older API has a ModeChanged member
          type message.
          
          We maintain both the old and new API here, so that
          we can work with any version.
          
        */
	if (dbus_message_is_signal(msg, "org.bluez.Adapter", "PropertyChanged")) {
                DBusMessageIter iter;
                int i = 0;
                int type;
                int arg_num = 0;
                const char *msg_arg = NULL;
                int variant;
                char **path;
                string ifname;
                DBusError err;

                dbus_error_init(&err);

                /* Get the interface name from the path */
                if (!dbus_message_get_path_decomposed(msg, &path)) {
                        fprintf(stderr, "Error getting path decomposed\n");
                        return DBUS_HANDLER_RESULT_HANDLED;
                }

                /* Find the interface, i.e., the last
                   component of a path /org/bluez/25717/hci0
                   decomposed into an array. */
                        
                while (path[i]) {
                        i++;
                } 
                ifname = path[i-1];
                dbus_free_string_array(path);

                dbus_message_iter_init (msg, &iter);
                        
                // Iterate through arguments and set 'str' and 'variant'
                while ((type = dbus_message_iter_get_arg_type (&iter)) != DBUS_TYPE_INVALID) {
                        if (type == DBUS_TYPE_STRING && arg_num == 0) {
                                dbus_message_iter_get_basic (&iter, &msg_arg);
                        } else if (type == DBUS_TYPE_VARIANT && arg_num == 1) {
                                DBusMessageIter sub_iter;
                                int sub_arg_num = 0;
                                
                                dbus_message_iter_recurse(&iter, &sub_iter);
                                       
                                while ((type = dbus_message_iter_get_arg_type(&sub_iter)) != DBUS_TYPE_INVALID) {
                                        if (type == DBUS_TYPE_BOOLEAN && sub_arg_num == 0) {
                                                dbus_message_iter_get_basic(&sub_iter, &variant);
                                        }
                                        dbus_message_iter_next(&sub_iter);
                                        sub_arg_num++;
                                }
                                        
                        }
                        dbus_message_iter_next(&iter);
                        arg_num++;
                }

                if (msg_arg && strcmp(msg_arg, "Discoverable") == 0) {

                        if (variant) {
                                CM_DBG("Bluetooth interface '%s' up\n", ifname.c_str());
                                Interface *iface = hci_get_interface_from_name(ifname.c_str());
                                        
                                if (iface) {
                                        cl->report_interface(iface, NULL, newConnectivityInterfacePolicyAgeless);
                                        delete iface;
                                }
                                        
                        } else {
                                CM_DBG("Bluetooth interface '%s' down\n", ifname.c_str());
                                cl->delete_interface(ifname);
                        }
                }
        } else if (dbus_message_is_signal(msg, "org.bluez.Adapter", "ModeChanged")) {   
                int i = 0;
                char **path;
                char *str = NULL;
                string ifname;
                DBusError err;

                dbus_error_init(&err);

                /* Get the interface name from the path */
                if (!dbus_message_get_path_decomposed(msg, &path)) {
                        return DBUS_HANDLER_RESULT_HANDLED;
                }

                /* Find the interface, i.e., the last
                   component of a path /org/bluez/25717/hci0
                   decomposed into an array. */
                        
                while (path[i]) {
                        i++;
                } 
                ifname = path[i-1];
                dbus_free_string_array(path);
                        
                /* Check the status string */
                if (dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID) == TRUE) {
                        if (strcmp(str, "discoverable") == 0) {
                                CM_DBG("Bluetooth interface %s up\n", ifname.c_str());
                                Interface *iface = hci_get_interface_from_name(ifname.c_str());

                                if (iface) {
                                        cl->report_interface(iface, NULL, newConnectivityInterfacePolicyAgeless);
                                        delete iface;
                                }

                        } else if (strcmp(str, "off") == 0) {
                                CM_DBG("Bluetooth interface %s down\n", ifname.c_str());

                                cl->delete_interface(ifname);
                        }
                } else {
                        CM_DBG("DBus error in read args\n");
                        dbus_error_free(&err);
                }
        } else {
		CM_DBG("ERROR: Message on the dbus is not a recognized signal\n");
                return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
#endif
	return DBUS_HANDLER_RESULT_HANDLED;
}

int dbus_hci_adapter_removed_watch(struct dbus_handle *dbh, void *data)
{
	int ret = -1;

	if (!dbus_connection_add_filter(dbh->conn, dbus_handler, (void *) data, NULL))
		return -1;

	dbus_bus_add_match(dbh->conn, "type='signal',interface='org.bluez.Manager',member='AdapterRemoved'", &dbh->err);

	if (dbus_error_is_set(&dbh->err))
		dbus_error_free(&dbh->err);
	else
		ret = 0;

	return ret;
}


int dbus_hci_property_changed_watch(struct dbus_handle *dbh, void *data)
{
	int ret = -1;

	if (!dbus_connection_add_filter(dbh->conn, dbus_handler, (void *) data, NULL))
		return -1;

        // PropertyChanged is used in the newer BlueZ API:
	dbus_bus_add_match(dbh->conn, "type='signal',interface='org.bluez.Adapter',member='PropertyChanged'", &dbh->err);
        
        // ModeChanged, is used in the older BlueZ API:
        dbus_bus_add_match(dbh->conn, "type='signal',interface='org.bluez.Adapter',member='ModeChanged'", &dbh->err);

	if (dbus_error_is_set(&dbh->err))
		dbus_error_free(&dbh->err);
	else
		ret = 0;

	return ret;
}

static int dbus_init_handle(struct dbus_handle *dbh)
{
	if (!dbh)
		return -1;

	dbus_error_init(&dbh->err);

	dbh->conn = dbus_bus_get(DBUS_BUS_SYSTEM, &dbh->err);

	if (dbus_error_is_set(&dbh->err)) {
		HAGGLE_ERR("D-Bus Connection Error (%s)\n", dbh->err.message);
		dbus_error_free(&dbh->err);
	}
	if (NULL == dbh->conn) {
		return -1;
	}

	dbus_connection_flush(dbh->conn);

	if (dbus_error_is_set(&dbh->err)) {
		HAGGLE_ERR("Match Error (%s)\n", dbh->err.message);
		return -1;
	}
	return 0;
}

#endif

#if defined(ENABLE_BLUETOOTH)

Interface *hci_get_interface_from_name(const char *ifname)
{
	struct hci_dev_info di;
	char name[249];
	char macaddr[BT_ALEN];
	int dd, i;

	dd = hci_open_dev(0);

	if (dd < 0) {
		HAGGLE_ERR("Can't open device : %s (%d)\n", strerror(errno), errno);
		return NULL;
	}

	memset(&di, 0, sizeof(struct hci_dev_info));

        strcpy(di.name, ifname);

	if (ioctl(dd, HCIGETDEVINFO, (void *) &di) < 0) {
		CM_DBG("HCIGETDEVINFO failed for %s\n", ifname);
		return NULL;
	}

	/* The interface name, e.g., 'hci0' */
	baswap((bdaddr_t *) & macaddr, &di.bdaddr);

	/* Read local "hostname" */
	if (hci_read_local_name(dd, sizeof(name), name, 1000) < 0) {
		HAGGLE_ERR("Can't read local name on %s: %s (%d)\n", ifname, strerror(errno), errno);
		CM_DBG("Could not read adapter name for device %s\n", ifname);
		hci_close_dev(dd);
		return NULL;
	}
	hci_close_dev(dd);

	/* Process name */
	for (i = 0; i < 248 && name[i]; i++) {
		if ((unsigned char) name[i] < 32 || name[i] == 127)
			name[i] = '.';
	}

	name[248] = '\0';
	Address addr(AddressType_BTMAC, (unsigned char *) macaddr);
	
	return new Interface(IFTYPE_BLUETOOTH, macaddr, &addr, ifname, IFFLAG_LOCAL | IFFLAG_UP);
}

static int hci_init_handle(struct hci_handle *hcih)
{
	if (!hcih)
		return -1;

	memset(hcih, 0, sizeof(struct hci_handle));

	/* Create HCI socket */
	hcih->sock = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);

	if (hcih->sock < 0) {
		HAGGLE_ERR("could not open HCI socket\n");
		return -1;
	}

	/* Setup filter */
	hci_filter_clear(&hcih->flt);
	hci_filter_set_ptype(HCI_EVENT_PKT, &hcih->flt);

	hci_filter_set_event(EVT_STACK_INTERNAL, &hcih->flt);

	if (setsockopt(hcih->sock, SOL_HCI, HCI_FILTER, &hcih->flt, sizeof(hcih->flt)) < 0) {
		return -1;
	}

	/* Bind socket to the HCI device */
	hcih->addr.hci_family = AF_BLUETOOTH;
	hcih->addr.hci_dev = HCI_DEV_NONE;

	if (bind(hcih->sock, (struct sockaddr *) &hcih->addr, sizeof(struct sockaddr_hci)) < 0) {
		return -1;
	}

	return 0;
}

void hci_close_handle(struct hci_handle *hcih)
{
	close(hcih->sock);
}

int ConnectivityLocal::read_hci()
{
	char buf[HCI_MAX_FRAME_SIZE];
	hci_event_hdr *hdr = (hci_event_hdr *) & buf[1];
	char *body = buf + HCI_EVENT_HDR_SIZE + 1;
	int len;
	u_int8_t type;

	len = read(hcih.sock, buf, HCI_MAX_FRAME_SIZE);

	type = buf[0];

	if (type != HCI_EVENT_PKT) {
		CM_DBG("Not HCI_EVENT_PKT type\n");
		return -1;
	}
	// STACK_INTERNAL events require root permissions
	if (hdr->evt == EVT_STACK_INTERNAL) {
		evt_stack_internal *si = (evt_stack_internal *) body;
		evt_si_device *sd = (evt_si_device *) & si->data;
                struct hci_dev_info di;
                Interface *iface;
                int ret;
                
                ret = hci_devinfo(sd->dev_id, &di);

                // TODO: Should check return value
                
		switch (sd->event) {
		case HCI_DEV_REG:
			HAGGLE_DBG("HCI dev %d registered\n", sd->dev_id);
			break;

		case HCI_DEV_UNREG:
			HAGGLE_DBG("HCI dev %d unregistered\n", sd->dev_id);
			break;

		case HCI_DEV_UP:
			HAGGLE_DBG("HCI dev %d up\n", sd->dev_id);
                        iface = hci_get_interface_from_name(di.name);
                                        
                        if (iface) {
                                report_interface(iface, NULL, newConnectivityInterfacePolicyAgeless);
                                delete iface;
                        }
			break;

		case HCI_DEV_DOWN:
			HAGGLE_DBG("HCI dev %d down\n", sd->dev_id);
                        CM_DBG("Bluetooth interface %s down\n", di.name);
                        delete_interface(string(di.name));
			break;
		default:
			HAGGLE_DBG("HCI unrecognized event\n");
			break;
		}
	} else {
		CM_DBG("Unknown HCI event\n");
	}
	return 0;
}

// Finds local bluetooth interfaces:
void ConnectivityLocal::findLocalBluetoothInterfaces()
{
	int i, ret = 0;
	struct {
		struct hci_dev_list_req dl;
		struct hci_dev_req dr[HCI_MAX_DEV];
	} req;

	memset(&req, 0, sizeof(req));

	req.dl.dev_num = HCI_MAX_DEV;

	ret = ioctl(hcih.sock, HCIGETDEVLIST, (void *) &req);

	if (ret < 0) {
		CM_DBG("HCIGETDEVLIST failed\n");
		return;		// ret;
	}

	for (i = 0; i < req.dl.dev_num; i++) {

		struct hci_dev_info di;
		char devname[9];
		char name[249];
		char macaddr[BT_ALEN];
		int dd, hdev;

		memset(&di, 0, sizeof(struct hci_dev_info));

		di.dev_id = req.dr[i].dev_id;

		hdev = di.dev_id;

		ret = ioctl(hcih.sock, HCIGETDEVINFO, (void *) &di);

		if (ret < 0) {
			CM_DBG("HCIGETDEVINFO failed for dev_id=%d\n", req.dr[i].dev_id);
			return;	// ret;
		}
		/* The interface name, e.g., 'hci0' */
		strncpy(devname, di.name, 9);
		baswap((bdaddr_t *) & macaddr, &di.bdaddr);

		dd = hci_open_dev(hdev);

		if (dd < 0) {
			HAGGLE_ERR("Can't open device hci%d: %s (%d)\n", hdev, strerror(errno), errno);
			continue;
		}

		/* Read local "hostname" */
		if (hci_read_local_name(dd, sizeof(name), name, 1000) < 0) {
			HAGGLE_ERR("Can't read local name on %s: %s (%d)\n", devname, strerror(errno), errno);
			CM_DBG("Could not read adapter name for device %s\n", devname);
			hci_close_dev(dd);
			continue;
		}
		hci_close_dev(dd);

		/* Process name */
		for (i = 0; i < 248 && name[i]; i++) {
			if ((unsigned char) name[i] < 32 || name[i] == 127)
				name[i] = '.';
		}

		name[248] = '\0';
		
		Address addy(AddressType_BTMAC, (unsigned char *) macaddr);
		
		Interface iface(IFTYPE_BLUETOOTH, macaddr, &addy, devname, IFFLAG_LOCAL | IFFLAG_UP);

		report_interface(&iface, rootInterface, newConnectivityInterfacePolicyAgeless);

	}
	return;
}
#endif

void ConnectivityLocal::hookCleanup()
{
#if defined(ENABLE_BLUETOOTH)
	hci_close_handle(&hcih);
#endif

#if defined(ENABLE_ETHERNET)
	nl_close_handle(&nlh);
#endif
#if defined(HAVE_DBUS)
	dbus_watch_remove_all();
	//dbus_close_handle(&dbh);
#endif
}

bool ConnectivityLocal::run()
{
	int ret;
#if defined(ENABLE_ETHERNET)

	ret = nl_init_handle(&nlh);

	if (ret < 0) {
		CM_DBG("Could not open netlink socket\n");
	}

	netlink_getlink(&nlh);

#endif
#if defined(ENABLE_BLUETOOTH)
	// Some events on this socket require root permissions. These
	// include adapter up/down events in read_hci()
	ret = hci_init_handle(&hcih);

	if (ret < 0) {
		CM_DBG("Could not open HCI socket\n");
	}
	findLocalBluetoothInterfaces();
#endif


#if defined(HAVE_DBUS)
        // D-bus allows us to listen to bluetooth up/down events as
        // an unprivileged user.
	ret = dbus_init_handle(&dbh);

	if (ret < 0) {
		CM_DBG("Could not open D-Bus connection\n");
	} else {
		
                if (dbus_hci_property_changed_watch(&dbh, this) < 0) {
			HAGGLE_ERR("Failed add dbus watch\n");
		}
                
		if (dbus_hci_adapter_removed_watch(&dbh, this) < 0) {
			HAGGLE_ERR("Failed add dbus watch\n");
		}

		dbus_connection_set_watch_functions(dbh.conn, dbus_watch_add, dbus_watch_remove, dbus_watch_toggle, (void *) this, NULL);

		dbus_connection_add_filter(dbh.conn, dbus_handler, this, NULL);
	}
#endif

	while (!shouldExit()) {
		Watch w;

		w.reset();
		
#if defined(HAVE_DBUS)
		for (List<watch_data *>::iterator it = dbusWatches.begin(); it != dbusWatches.end(); it++) {
			(*it)->watchIndex = w.add((*it)->fd);
                }
#endif

#if defined(ENABLE_BLUETOOTH) && !defined(HAVE_DBUS)
		int hciIndex = w.add(hcih.sock);
#endif

#if defined(ENABLE_ETHERNET)
		int nlhIndex = w.add(nlh.sock);
#endif

		ret = w.wait();
                
		if (ret == Watch::FAILED) {
			// Some error
			break;
		}
		if (ret == Watch::ABANDONED) {
			// We should exit
			break;
		}
		// This should not happen since we do not have a timeout set
		if (ret == Watch::TIMEOUT)
			continue;

#if defined(ENABLE_ETHERNET)
		if (w.isSet(nlhIndex)) {
			read_netlink();
		}
#endif
#if defined(ENABLE_BLUETOOTH) && !defined(HAVE_DBUS)
		if (w.isSet(hciIndex)) {
			read_hci();
		}
#endif

#if defined(HAVE_DBUS)
		bool doDispatch = false;
		for (List<watch_data *>::iterator it = dbusWatches.begin(); it != dbusWatches.end(); it++) {
			if (w.isSet((*it)->watchIndex)) {
				if (dbus_watch_get_flags((*it)->watch) & DBUS_WATCH_READABLE) {
					dbus_watch_handle((*it)->watch, DBUS_WATCH_READABLE);
					doDispatch = true;
				}
			}
		}
	
		if (doDispatch) {
                        int n = 0;
			while (dbus_connection_dispatch(dbh.conn) == DBUS_DISPATCH_DATA_REMAINS) { printf("Dispatching %d\n", n); }
		}
#endif
	}
	return false;
}

#endif
