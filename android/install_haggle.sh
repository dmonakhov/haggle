#!/bin/sh

# This script pushes built files to the sdcard on the phone, assuming
# that the Haggle build dir is in external/haggle of the Android
# source tree. The 'adb' utility from the Android SDK must
# be in the path.

THIS_DIR=`dirname $0`
DEVICE_FILES_DIR=$THIS_DIR/device-files
PUSH_DIR=/sdcard
DATA_DIR=/data/local
SOURCE_DIR=out/target/product/dream
ADB=adb
ADB_PARAMS=

if [ ! -z $1 ]; then
    ADB_PARAMS="-s $1"
    echo "Directing commands towards device $1"
else
    NUM_DEV_LINES=`adb devices | wc -l` 
    
    if [ $NUM_DEV_LINES -gt 3 ]; then
	echo "More than one device connected. You must specify a device ID"
	echo "Type 'adb devices' for a device list"
	exit
    fi
fi

cd $THIS_DIR

# Depending on how the device is rooted, it might not be possible to
# write directly to the root partition due to lack of permissions. We
# therefore write first to the sdcard, and then we run a script on the
# device as 'root', which copies the files to their correct places.

if [ -f $DEVICE_FILES_DIR/sdcard_install.sh ]; then
    echo
    echo "Pushing 'sdcard_install.sh' to $DATA_DIR/"
    $ADB $ADB_PARAMS push $DEVICE_FILES_DIR/sdcard_install.sh $DATA_DIR/
    $ADB $ADB_PARAMS push $DEVICE_FILES_DIR/adhoc.sh $DATA_DIR/
    $ADB $ADB_PARAMS push $DEVICE_FILES_DIR/tiwlan.ini $DATA_DIR/
    $ADB $ADB_PARAMS shell chmod 775 $DATA_DIR/sdcard_install.sh
    $ADB $ADB_PARAMS shell chmod 775 $DATA_DIR/adhoc.sh
fi

cd ../../../

#echo $PWD
FRAMEWORK_FILES="framework/haggle.jar"
FILES="bin/haggle lib/libhaggle.so lib/libhaggle_jni.so framework/haggle.jar lib/libhaggle-xml2.so"
FILES_UNSTRIPPED="sbin/haggle system/lib/libhaggle.so system/lib/libhaggle_jni.so system/lib/libhaggle-xml2.so"

# Install framework files
for f in $FRAMEWORK_FILES; do
    echo
    echo "Pushing '$f' to $PUSH_DIR/"
    $ADB $ADB_PARAMS push $SOURCE_DIR/system/$f $PUSH_DIR/
done

# Install the stripped binaries:

#for f in $FILES; do
#    echo
#    echo "Pushing '$f' to $PUSH_DIR/"
#    $ADB $ADB_PARAMS push $SOURCE_DIR/system/$f $PUSH_DIR/
#done


# Install unstripped binaries for debugging purposes (e.g., running
# gdb on the Android device).
for f in $FILES_UNSTRIPPED; do
    echo
    echo "Pushing '$f' to $PUSH_DIR/"
    $ADB $ADB_PARAMS push $SOURCE_DIR/symbols/$f $PUSH_DIR/
done

echo
echo "Running copy script on device as 'root'..."
echo
# This will call the script to copy the binaries to the root
# filesystem. (Requires root access without 'su'). Otherwise, log in
# on the device using adb, do 'su' and then run script manually.
$ADB $ADB_PARAMS shell su -c $DATA_DIR/sdcard_install.sh
