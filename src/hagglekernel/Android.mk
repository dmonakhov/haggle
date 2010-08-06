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
	ResourceMonitorAndroid.cpp \
	SecurityManager.cpp \
	SQLDataStore.cpp \
	Trace.cpp \
	Utility.cpp \
	Metadata.cpp \
	XMLMetadata.cpp 

ifneq ($(BOARD_WLAN_TI_STA_DK_ROOT),)
# This Connectivity is specific for tiwlan driver
LOCAL_SRC_FILES += ConnectivityLocalAndroid.cpp
else
# This Connectivity is a generic Linux one which uses wireless extensions to
# discover local interfaces.
LOCAL_SRC_FILES += ConnectivityLocalLinux.cpp
endif

# Includes for the TI wlan driver API
TI_STA_INCLUDES := \
	system/wlan/ti/sta_dk_4_0_4_32/common/src/hal/FirmwareApi \
	system/wlan/ti/sta_dk_4_0_4_32/common/inc \
	system/wlan/ti/sta_dk_4_0_4_32/common/src/inc \
	system/wlan/ti/sta_dk_4_0_4_32/pform/linux/inc \
	system/wlan/ti/sta_dk_4_0_4_32/CUDK/Inc

LOCAL_C_INCLUDES += \
	$(TI_STA_INCLUDES) \
	$(LOCAL_PATH)/../../extlibs/libxml2-2.6.31/include \
        $(LOCAL_PATH)/../libcpphaggle/include \
        $(LOCAL_PATH)/../utils \
        $(LOCAL_PATH)/../libhaggle/include \
	system/core/include \
	hardware/libhardware_legacy/include \
	external/sqlite/dist \
	external/openssl/include \
	external/dbus 

#ifeq ($LOCAL_SDK_VERSION),
#LOCAL_C_INCLUDES += external/bluez/libs/include
#else
LOCAL_C_INCLUDES += external/bluetooth/bluez/include
#endif

LOCAL_SHARED_LIBRARIES := \
	libsqlite \
	libhardware_legacy \
	libdbus

# We need to compile our own version of libxml2, because the static
# library provided in Android does not have the configured options we need.
LOCAL_LDLIBS := -lpthread -lsqlite -lcrypto -ldbus -lhaggle-xml2 -lbluetooth -lhardware_legacy -lwpa_client -lcutils

LOCAL_SHARED_LIBRARIES += libdl libstdc++ libsqlite libcrypto libdbus libhaggle-xml2 libbluetooth libhardware_legacy libwpa_client libcutils

LOCAL_STATIC_LIBRARIES += libcpphaggle

EXTRA_DEFINES:=-DHAVE_CONFIG -DOS_ANDROID -DHAVE_EXCEPTION=0 -DENABLE_ETHERNET -DENABLE_BLUETOOTH -DHAVE_DBUS -DDEBUG -DDEBUG_LEAKS
LOCAL_CFLAGS:=-O2 -g $(EXTRA_DEFINES)
LOCAL_CPPFLAGS +=$(EXTRA_DEFINES)

ifneq ($(BOARD_WLAN_TI_STA_DK_ROOT),)
LOCAL_STATIC_LIBRARIES += libWifiApi
LOCAL_CPPFLAGS +=-DENABLE_TI_WIFI
LOCAL_CFLAGS +=-DENABLE_TI_WIFI
endif

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := haggle

# LOCAL_UNSTRIPPED_PATH := $(TARGET_ROOT_OUT_SBIN_UNSTRIPPED)

include $(BUILD_EXECUTABLE)

endif
