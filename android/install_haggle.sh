#!/bin/sh

# This script pushes built files to the sdcard on the phone, assuming
# that the Haggle build dir is in external/haggle of the Android
# source tree. The 'adb' utility from the Android SDK must
# be in the path.

SCRIPT_DIR=$PWD/`dirname $0`
DEVICE_FILES_DIR=$SCRIPT_DIR/device-files
PUSH_DIR=/sdcard
DATA_DIR=/data/local
ADB=adb
ADB_PARAMS=
ANDROID_DIR=$ANDROID_BUILD_TOP

if [ -z $TARGET_PRODUCT ]; then
        echo "There is no TARGET_PRODUCT environment variable set."
	echo "Please make sure that the Android build environment"
	echo "is configured by running \'source build/env-setup-sh\'."
	echo
	exit
fi

# Restart adb with root permissions
adb root

pushd $ANDROID_DIR

if [ ! -d $ANDROID_PRODUCT_OUT ]; then
    echo "Cannot find product directory $ANDROID_PRODUCT_OUT"
    exit
fi

popd
	
echo "Looking for Android devices..."

DEVICES=$(adb devices | awk '{ if (match($2,"device")) print $1}')
NUM_DEVICES=$(echo $DEVICES | awk '{print split($0,a, " ")}')

if [ $NUM_DEVICES -lt 1 ]; then
    echo "There are no Android devices connected to the computer."
    echo "Please connect at least one device before installation can proceed."
    echo
    exit
fi 

echo "$NUM_DEVICES Android devices found."
echo
echo "Assuming android source is found in $ANDROID_DIR"
echo "Please make sure this is correct before proceeding."
echo
echo "Press any key to install Haggle on these devices, or ctrl-c to abort"

# Wait for some user input
read

pushd $SCRIPT_DIR

for dev in $DEVICES; do
    echo "Installing configuration files onto device $dev"
    $ADB -s $dev push $DEVICE_FILES_DIR/tiwlan.ini $DATA_DIR/
done

FRAMEWORK_PATH_PREFIX="system/framework"
FRAMEWORK_FILES="haggle.jar"

LIB_PATH_PREFIX="system/lib"
LIBS="libhaggle.so libhaggle_jni.so libhaggle-xml2.so"

BIN_HOST_PREXIF=
BIN_PATH_PREFIX="system/bin"
HAGGLE_BIN="haggle"

pushd $ANDROID_DIR
pushd $ANDROID_PRODUCT_OUT

echo $PWD

for dev in $DEVICES; do
    echo
    echo "Installing files onto device $dev"

    # Remount /system partition in rw mode
    $ADB -s $dev shell mount -o remount,rw -t yaffs2 /dev/block/mtdblock3 /system

    # Enter directory holding unstripped binaries
    pushd symbols

    # Install ad hoc settings script
    $ADB -s $dev push $DEVICE_FILES_DIR/adhoc.sh $BIN_PATH_PREFIX/adhoc
    $ADB -s $dev shell chmod 775 $BIN_PATH_PREFIX/adhoc

    # Install Haggle binary
    echo
    echo "Installing binaries"
    echo "    $HAGGLE_BIN"
    $ADB -s $dev push $BIN_PATH_PREFIX/$HAGGLE_BIN /$BIN_PATH_PREFIX/$HAGGLE_BIN
    $ADB -s $dev shell chmod 4775 /$BIN_PATH_PREFIX/$HAGGLE_BIN

    echo "    luckyMe"
    $ADB -s $dev push $BIN_PATH_PREFIX/luckyme /$BIN_PATH_PREFIX/luckyme
    $ADB -s $dev shell chmod 4775 /$BIN_PATH_PREFIX/luckyme

    echo "    clitool"
    $ADB -s $dev push $BIN_PATH_PREFIX/clitool /$BIN_PATH_PREFIX/clitool
    $ADB -s $dev shell chmod 4775 /$BIN_PATH_PREFIX/clitool

    # Install libraries
    echo
    echo "Installing library files"
    for file in $LIBS; do
	echo "    $file"
	$ADB -s $dev push $LIB_PATH_PREFIX/$file /$LIB_PATH_PREFIX/$file
	$ADB -s $dev shell chmod 644 /$LIB_PATH_PREFIX/$file
    done
    
    # Back to product dir
    popd

    # Install framework files
    echo
    echo "Installing framework files"
    for file in $FRAMEWORK_FILES; do
	echo "    $file"
	$ADB -s $dev push $FRAMEWORK_PATH_PREFIX/$file /$FRAMEWORK_PATH_PREFIX/$file
	$ADB -s $dev shell chmod 644 /$FRAMEWORK_PATH_PREFIX/$file
    done

    # Cleanup data folder if any
    $ADB -s $dev shell rm /data/haggle/haggle.db
    $ADB -s $dev shell rm /data/haggle/haggle.log
    $ADB -s $dev shell rm /data/haggle/trace.log
    $ADB -s $dev shell rm /data/haggle/libhaggle.txt
    $ADB -s $dev shell rm /data/haggle/haggle.pid

    # Reset filesystem to read-only
    $ADB -s $dev shell mount -o remount,ro -t yaffs2 /dev/block/mtdblock3 /system
done

popd
popd
popd
