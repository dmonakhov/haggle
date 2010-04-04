# Copyright 2008 Uppsala University

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

# Do not build for simulator
ifneq ($(TARGET_SIMULATOR),true)

#
# LuckyMe
#
LOCAL_SRC_FILES := \
	luckyme.c 

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/../utils \
        $(LOCAL_PATH)/../libhaggle/include

LOCAL_LDLIBS := -lpthread -lhaggle

LOCAL_SHARED_LIBRARIES = libdl libhaggle
LOCAL_STATIC_LIBRARIES =

LOCAL_CFLAGS:=-O2 -g
LOCAL_CFLAGS+=-DOS_ANDROID -DDEBUG

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := luckyme

include $(BUILD_EXECUTABLE)

endif
