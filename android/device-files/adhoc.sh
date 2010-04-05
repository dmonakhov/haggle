#!/system/bin/sh

# A script to set adhoc mode on android devices.
# Usage: adhoc.sh [start|stop] nodeNR

NODE=1
NETDEV=eth0
MODULE_PATH=/system/lib/modules
POST_INSERT_CMD=
IP_PREFIX="192.168.1"

if test -e $MODULE_PATH/wlan.ko; then
    NETDEV=tiwlan0
    MODULE=wlan
    HOSTNAME=Magic
    POST_INSERT_CMD="wlan_loader -f /system/etc/wifi/Fw1251r1c.bin -e /proc/calibration -i /data/local/tiwlan.ini"
elif test -e $MODULE_PATH/bcm4329.ko; then
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

if test -n $2; then
    NODE=$2
fi

if grep $MODULE /proc/modules 2>/dev/null; then
    echo "Shutting down interface $NETDEV"
    ifconfig $NETDEV down
    sleep 1
    echo "Removing module $MODULE"
    rmmod $MODULE
fi

echo "Node IP is $IP_PREFIX.$NODE"

if test -e /system/bin/hostname; then
    hostname "$HOSTNAME-$NODE"
else
    echo "127.0.0.1    $HOSTNAME-$NODE" > /system/etc/hosts
fi

insmod $MODULE_PATH/$MODULE.ko

if test -n $POST_INSERT_CMD; then
    eval $POST_INSERT_CMD
fi

ifconfig $NETDEV $IP_PREFIX.$NODE netmask 255.255.255.0
ifconfig $NETDEV up 

;;
'stop')

if grep $MODULE /proc/modules 2>/dev/null; then
    ifconfig $NETDEV down
    sleep 1
    rmmod $MODULE
fi
;;
*)
echo "Usage: $0 [start|stop] [ node # ]"
;;
esac
