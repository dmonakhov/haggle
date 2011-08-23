LOCAL_PATH := $(call my-dir)

subdirs := $(addprefix $(APP_PROJECT_PATH)/../../,$(addsuffix /Android.mk, \
		extlibs/libxml2-2.6.31 \
		src/libcpphaggle \
		src/hagglekernel \
        ))

EXTRA_LIBS_PATH := $(APP_PROJECT_PATH)/../extlibs/external
EXTRA_DEFINES=

include $(subdirs)
