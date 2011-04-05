LOCAL_PATH := $(call my-dir)

subdirs := $(addprefix $(APP_PROJECT_PATH)/../../,$(addsuffix /Android.mk, \
		extlibs/libxml2-2.6.31 \
		src/libcpphaggle \
		src/hagglekernel \
        ))

EXTRA_LIBS_PATH := $(APP_PROJECT_PATH)/../extlibs/external

include $(subdirs)

#$(call __ndk_info,$(LOCAL_MAKEFILE):$(LOCAL_MODULE): subdirs are : $(subdirs))

#$(call import-module,sqlite/dist)
#$(call import-module,dbus)
#$(call import-module,bluetooth/bluez)
#$(call import-module,zlib)
#$(call import-module,openssl/crypto)
