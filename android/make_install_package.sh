#!/bin/bash

THIS_DIR=$PWD
SCRIPT_DIR=`dirname $0`
ANDROID_SRC_DIR=
PHOTOSHARE_SRC_DIR=$HOME/Documents/workspace/hg/android/PhotoShare
HAGGLE_VER=0.2

function usage() {
    echo "Usage: $0 ANDROID_SRC_DIR [ PHOTOSHARE_SRC_DIR ]"
    exit;
}

if [ -z $1 ]; then
    usage;
else
    ANDROID_SRC_DIR=$1
    echo "Using $ANDROID_SRC_DIR as Android source directory"
fi

if [ ! -z $2 ]; then
    PHOTOSHARE_SRC_DIR=$2
    echo "Looking for PhotoShare in $PHOTOSHARE_DIR"
fi


ANDROID_BIN_DIR=$ANDROID_SRC_DIR/out/target/product/dream/system
HAGGLE_BIN=$ANDROID_BIN_DIR/bin/haggle
LIBHAGGLE_SO=$ANDROID_BIN_DIR/lib/libhaggle.so
LIBHAGGLE_XML2_SO=$ANDROID_BIN_DIR/lib/libhaggle-xml2.so
LIBHAGGLE_JNI_SO=$ANDROID_BIN_DIR/lib/libhaggle_jni.so
HAGGLE_JAR=$ANDROID_BIN_DIR/framework/haggle.jar
PHOTOSHARE_APK=$PHOTOSHARE_SRC_DIR/bin/PhotoShare.apk

PACKAGE_DIR_NAME=haggle-$HAGGLE_VER-android
PACKAGE_DIR=/tmp/$PACKAGE_DIR_NAME

if [ ! -f $PHOTOSHARE_APK ]; then
    echo "Could not find PhotoShare android package 'PhotoShare.apk' in '$PHOTOSHARE_SRC_DIR/bin'"
    echo "Make sure the path is correct and that PhotoShare has been compiled with Eclipse"
    echo
    exit
fi

rm -rf $PACKAGE_DIR
mkdir $PACKAGE_DIR
pushd $PACKAGE_DIR

# Copy all the files we need into our package directory
for f in "$HAGGLE_BIN $LIBHAGGLE_SO $LIBHAGGLE_XML2_SO $LIBHAGGLE_JNI_SO $HAGGLE_JAR $PHOTOSHARE_APK"; do
    cp -f $f .
done

# Create install script

cat > install.sh <<EOF
#!/bin/bash

echo "This script will install Haggle and PhotoShare on any Android devices connected to this computer."
echo "Note that the installation will only be successful if you have an Android developer phone,"
echo "or another device that allows root access."
echo

DEVICES=\$(adb devices | awk '{ if (match(\$2,"device")) print \$1}')
NUM_DEVICES=\$(echo \$DEVICES | awk '{print split(\$0,a, " ")}')

if [ \$NUM_DEVICES -lt 1 ]; then
    echo "There are no Android devices connected to the computer."
    echo "Please connect at least one device before installation can proceed."
    echo
    exit
fi 

echo "\$NUM_DEVICES Android devices found."
echo "Press any key to install Haggle and PhotoShare on these devices, or ctrl-c to abort"

# Wait for some user input
read

for dev in \$DEVICES; do
    echo "Installing onto device \$dev"
    # Remount /system partition in read/write mode
    adb -s \$dev shell su -c mount -o remount,rw -t yaffs2 /dev/block/mtdblock3 /system
    adb -s \$dev push haggle /system/bin/haggle
    adb -s \$dev shell su -c chmod 4775 /system/bin/haggle

    adb -s \$dev push libhaggle.so /system/lib/libhaggle.so
    adb -s \$dev shell su -c chmod 644 /system/lib/libhaggle.so

    adb -s \$dev push libhaggle-xml2.so /system/lib/libhaggle-xml2.so
    adb -s \$dev shell su -c chmod 644 /system/lib/libhaggle-xml2.so

    adb -s \$dev push libhaggle_jni.so /system/lib/libhaggle_jni.so
    adb -s \$dev shell su -c chmod 644 /system/lib/libhaggle_jni.so

    adb -s \$dev push haggle.jar /system/framework/haggle.jar
    adb -s \$dev shell su -c chmod 644 /system/framework/haggle.jar

    adb -s \$dev uninstall org.haggle.PhotoShare
    adb -s \$dev install PhotoShare.apk

    # Reset /system partition to read-only mode
    adb -s \$dev shell su -c mount -o remount,ro -t yaffs2 /dev/block/mtdblock3 /system

    # Cleanup data folder if any
    adb -s \$dev shell su -c rm /data/haggle/*
done

EOF

chmod +x install.sh

pushd ..

# Make a tar-ball of everything
tar zcf $THIS_DIR/haggle-$HAGGLE_VER-android-bin.tar.gz $PACKAGE_DIR_NAME

popd
popd
