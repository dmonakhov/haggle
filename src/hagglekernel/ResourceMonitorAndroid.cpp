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

#include "ResourceMonitor.h"

#include <libcpphaggle/Watch.h>
#include <haggleutils.h>

#include <string.h>
#include <unistd.h>
#include <poll.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <linux/netlink.h>

ResourceMonitor::ResourceMonitor(ResourceManager *resMan) : 
	ManagerModule<ResourceManager>(resMan, "ResourceMonitor")
{
}


ResourceMonitor::~ResourceMonitor()
{
}

unsigned char ResourceMonitor::getBatteryLifePercent() const 
{
	// Unknown: return 100%
    return 100;
}

unsigned int ResourceMonitor::getBatteryLifeTime() const
{
	// Unknown: return 1 hour
    return 1*60*60;
}

unsigned long ResourceMonitor::getAvaliablePhysicalMemory() const 
{
	// Unknown: return 1 GB
    return 1*1024*1024*1024;
}

unsigned long ResourceMonitor::getAvaliableVirtualMemory() const 
{
	// Unknown: return 1 GB
    return 1*1024*1024*1024;
}

/* Returns -1 on failure, 0 on success */
int ResourceMonitor::uevent_init()
{
        struct sockaddr_nl addr;
        int sz = 64*1024;
        int s;

        memset(&addr, 0, sizeof(addr));
        addr.nl_family = AF_NETLINK;
        addr.nl_pid = getpid();
        addr.nl_groups = 0xffffffff;

        uevent_fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    
        if (uevent_fd < 0)
                return -1;

        setsockopt(uevent_fd, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz));

        if (bind(uevent_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
                close(uevent_fd);
                return -1;
        }

        return (uevent_fd > 0) ? 0 : -1;
}

void ResourceMonitor::uevent_close()
{
        if (uevent_fd > 0)
                close(uevent_fd);
}


#define UEVENT_AC_ONLINE_PATH "/sys/class/power_supply/ac/online"
#define UEVENT_USB_ONLINE_PATH "/sys/class/power_supply/usb/online"
#define UEVENT_BATTERY_STATUS_PATH "/sys/class/power_supply/battery/status"
#define UEVENT_BATTERY_HEALTH_PATH "/sys/class/power_supply/battery/health"
#define UEVENT_BATTERY_PRESENT_PATH "/sys/class/power_supply/battery/present"
#define UEVENT_BATTERY_CAPACITY_PATH "/sys/class/power_supply/battery/capacity"
#define UEVENT_BATTERY_VOLTAGE_PATH "/sys/class/power_supply/battery/batt_vol"
#define UEVENT_BATTERY_TEMPERATURE_PATH "/sys/class/power_supply/battery/batt_temp"
#define UEVENT_BATTERY_TECHNOLOGY_PATH "/sys/class/power_supply/battery/technology"

static bool isPath(const char *constant, const char *path)
{
        if (strncmp(constant, path, strlen(constant)) == 0) {
                return true;
        }
        return false;
}

static ssize_t readfile(const char *path, char *buf, size_t len)
{
        FILE *fp;
        size_t nitems;

        fp = fopen(path, "r");

        if (!fp)
                return -1;

        nitems = fread(buf, 1, len, fp);

        fclose(fp);

        if (nitems == 0)
                return -1;

        return nitems;
}

void ResourceMonitor::uevent_read()
{
        char buffer[1024];
        int buffer_length = sizeof(buffer);
        
        ssize_t len = recv(uevent_fd, buffer, buffer_length, 0);
        
        if (len <= 0) {
                HAGGLE_ERR("Could not read uevent\n");
                return;
        }
        
        HAGGLE_DBG("UEvent: %s\n", buffer);

        if (isPath(UEVENT_AC_ONLINE_PATH, buffer)) {
                HAGGLE_DBG("AC on/off\n");
        } else if (isPath(UEVENT_USB_ONLINE_PATH, buffer)) {
                HAGGLE_DBG("USB online\n");
        } else if (isPath(UEVENT_BATTERY_STATUS_PATH, buffer)) {
                len = readfile(buffer, buffer, buffer_length);
                if (len)
                        HAGGLE_DBG("Battery status is '%s'\n", buffer);
        } else if (isPath(UEVENT_BATTERY_HEALTH_PATH, buffer)) {
                len = readfile(buffer, buffer, buffer_length);
                if (len)
                        HAGGLE_DBG("Battery health is '%s'\n", buffer);
        } else if (isPath(UEVENT_BATTERY_PRESENT_PATH, buffer)) {
                HAGGLE_DBG("Battery present\n");
        } else if (isPath(UEVENT_BATTERY_VOLTAGE_PATH, buffer)) {
                HAGGLE_DBG("Battery voltage\n");
        } else if (isPath(UEVENT_BATTERY_TEMPERATURE_PATH, buffer)) {
                HAGGLE_DBG("Battery temperature\n");
        } 
}

bool ResourceMonitor::run()
{
	Watch w;

	HAGGLE_DBG("Running resource monitor\n");

        /*
        if (uevent_init() == -1) {
                HAGGLE_ERR("Could not open uevent socket\n");
                return false;
        }
        */
	while (!shouldExit()) {
		int ret;

		w.reset();
                
                int ueventIndex = -1; // = w.add(uevent_fd);

		ret = w.wait();
                
		if (ret == Watch::ABANDONED) {
			break;
		} else if (ret == Watch::FAILED) {
			HAGGLE_ERR("Wait on objects failed\n");
			break;
		}

                if (w.isSet(ueventIndex)) {
                        uevent_read();
                }
	}
	return false;
}

void ResourceMonitor::cleanup()
{
        uevent_close();
}
