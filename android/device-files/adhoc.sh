#!/system/bin/sh

# A script to set adhoc mode on android devices.
# Usage: adhoc.sh [start|stop] nodeNR

NODE=10

case "$1" in
'start')
if [ ! -z $2 ]; then
    NODE=$2
fi
echo "Node IP is 192.168.0.$NODE"
hostname "android.uu$NODE"
insmod /system/lib/modules/wlan.ko
wlan_loader -f /system/etc/wifi/Fw1251r1c.bin -e /proc/calibration -i /data/local/tiwlan.ini
ifconfig tiwlan0 192.168.0.$NODE netmask 255.255.255.0
ifconfig tiwlan0 up 

;;
'stop')

if grep wlan /proc/modules 2>/dev/null; then
    ifconfig tiwlan0 down
    rmmod wlan
fi
;;
*)
echo "Usage: $0 [start|stop] [ node # ]"
;;
esac
