# Copyright 2008 Uppsala University

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

# Do not build for simulator
ifneq ($(TARGET_SIMULATOR),true)

#
# Haggle
#
LOCAL_SRC_FILES := \
	../utils/base64.c \
	../utils/bloomfilter.c \
	../utils/counting_bloomfilter.c \
	../utils/utils.c \
	../utils/prng.c \
	Address.cpp \
	ApplicationManager.cpp \
	Attribute.cpp \
	BenchmarkManager.cpp \
	ConnectivityBluetooth.cpp \
	ConnectivityBluetoothLinux.cpp \
	Connectivity.cpp \
	ConnectivityEthernet.cpp \
	ConnectivityInterfacePolicy.cpp \
	ConnectivityLocal.cpp \
	ConnectivityManager.cpp \
	Bloomfilter.cpp \
	Certificate.cpp \
	DataManager.cpp \
	DataObject.cpp \
	DataStore.cpp \
	Debug.cpp \
	DebugManager.cpp \
	Event.cpp \
	Filter.cpp \
	Forwarder.cpp \
	ForwardingManager.cpp \
	ForwarderAsynchronous.cpp \
	ForwarderProphet.cpp \
	HaggleKernel.cpp \
	Interface.cpp \
	InterfaceStore.cpp \
	main.cpp \
	Manager.cpp \
	Node.cpp \
	NodeManager.cpp \
	NodeStore.cpp \
	Policy.cpp \
	Protocol.cpp \
	ProtocolLOCAL.cpp \
	ProtocolManager.cpp \
	ProtocolRFCOMM.cpp \
	ProtocolSocket.cpp \
	ProtocolTCP.cpp \
	ProtocolUDP.cpp \
	Queue.cpp \
	RepositoryEntry.cpp \
	ResourceManager.cpp \
	ResourceMonitor.cpp \
	ResourceMonitorAndroid.cpp \
	SecurityManager.cpp \
	SQLDataStore.cpp \
	Trace.cpp \
	Utility.cpp \
	Metadata.cpp \
	XMLMetadata.cpp \
	jni.cpp 

# Includes for the TI wlan driver API
TI_STA_INCLUDES := \
	system/wlan/ti/sta_dk_4_0_4_32/common/src/hal/FirmwareApi \
	system/wlan/ti/sta_dk_4_0_4_32/common/inc \
	system/wlan/ti/sta_dk_4_0_4_32/common/src/inc \
	system/wlan/ti/sta_dk_4_0_4_32/pform/linux/inc \
	system/wlan/ti/sta_dk_4_0_4_32/CUDK/Inc

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../extlibs/libxml2-2.6.31/include \
        $(LOCAL_PATH)/../libcpphaggle/include \
        $(LOCAL_PATH)/../utils \
        $(LOCAL_PATH)/../libhaggle/include \
	$(EXTRA_LIBS_PATH)/core/include \
	$(EXTRA_LIBS_PATH)/sqlite/dist \
	$(EXTRA_LIBS_PATH)/openssl \
	$(EXTRA_LIBS_PATH)/openssl/include \
	$(EXTRA_LIBS_PATH)/dbus \
	$(EXTRA_LIBS_PATH)/bluetooth/bluez/include \
	$(EXTRA_LIBS_PATH)/bluetooth/bluez/lib \
	$(JNI_H_INCLUDE)

# We need to compile our own version of libxml2, because the static
# library provided in Android does not have the configured options we need.
LOCAL_LDLIBS := \
	-lsqlite \
	-lcrypto \
	-ldbus \
	-lbluetooth \
	-llog

LOCAL_SHARED_LIBRARIES += \
	libdl \
	libstdc++ \
	libsqlite \
	libcrypto \
	libdbus \
	libbluetooth \
	liblog

LOCAL_STATIC_LIBRARIES += \
	libcpphaggle \
	libhaggle-xml2

EXTRA_DEFINES:= \
	-DHAVE_CONFIG \
	-DOS_ANDROID \
	-DHAVE_EXCEPTION=0 \
	-DENABLE_ETHERNET \
	-DENABLE_BLUETOOTH \
	-DHAVE_DBUS \
	-DDEBUG

LOCAL_CFLAGS :=-O2 -g $(EXTRA_DEFINES)
LOCAL_CPPFLAGS +=$(EXTRA_DEFINES)
LOCAL_LDFLAGS +=-L$(EXTRA_LIBS_PATH)/../libs/$(TARGET_ARCH_ABI)

ifneq ($(BOARD_WLAN_TI_STA_DK_ROOT),)
# For devices with tiwlan driver
LOCAL_C_INCLUDES += \
	$(TI_STA_INCLUDES) \
	hardware/libhardware_legacy/include 
LOCAL_STATIC_LIBRARIES += libWifiApi
LOCAL_CPPFLAGS +=-DENABLE_TI_WIFI
LOCAL_CFLAGS +=-DENABLE_TI_WIFI
LOCAL_SRC_FILES +=ConnectivityLocalAndroid.cpp
LOCAL_LDLIBS += \
	-lwpa_client \
	-lhardware_legacy \
	-lcutils
LOCAL_SHARED_LIBRARIES += \
	libwpa_client \
	libhardware_legacy \
	libcutils
else
# Generic Linux device with wireless extension compliant wireless
# driver
LOCAL_SRC_FILES += ConnectivityLocalLinux.cpp
endif

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := libhagglekernel_jni

# LOCAL_UNSTRIPPED_PATH := $(TARGET_ROOT_OUT_BIN_UNSTRIPPED)

include $(BUILD_SHARED_LIBRARY)

endif
