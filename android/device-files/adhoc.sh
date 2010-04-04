#!/system/bin/sh

# A script to set adhoc mode on android devices.
# Usage: adhoc.sh [start|stop] nodeNR

NODE=10
NETDEV=eth0
MODULE_PATH=/system/lib/modules
POST_INSERT_CMD=

if [ -f $MODULE_PATH/wlan.ko ]; then
    NETDEV=tiwlan0
    MODULE=wlan
    HOSTNAME=Magic
    POST_INSERT_CMD="wlan_loader -f /system/etc/wifi/Fw1251r1c.bin -e /proc/calibration -i /data/local/tiwlan.ini"
elif [ -f $MODULE_PATH/bcm4329.ko ]; then
    NETDEV=eth0
    MODULE=bcm4329
    HOSTNAME=NexusOne
    POST_INSERT_CMD="iwconfig eth0 mode ad-hoc essid HaggleHoc"
else
    echo "Unknown network device..."
    exit
fi

case "$1" in
'start')
if [ ! -z $2 ]; then
    NODE=$2
fi

echo "Node IP is 192.168.2.$NODE"

hostname "$HOSTNAME-$NODE"
insmod $MODULE_PATH/$MODULE.ko

if [ ! -z $POST_INSERT_CMD ]; then
    eval $POST_INSERT_CMD
fi

ifconfig $NETDEV 192.168.2.$NODE netmask 255.255.255.0
ifconfig $NETDEV up 

;;
'stop')

if grep $MODULE /proc/modules 2>/dev/null; then
    ifconfig $NETDEV down
    rmmod $MODULE
fi
;;
*)
echo "Usage: $0 [start|stop] [ node # ]"
;;
esac
