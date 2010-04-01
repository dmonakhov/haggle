# Copyright 2008 Uppsala University

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

# Do not build for simulator
ifneq ($(TARGET_SIMULATOR),true)

#
# clitool
#
LOCAL_SRC_FILES := \
	clitool.cpp \
	../utils/thread.c

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/../utils \
        $(LOCAL_PATH)/../libhaggle/include

LOCAL_SHARED_LIBRARIES := \
	libhaggle
LOCAL_LDLIBS := -lpthread -lhaggle

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
LOCAL_LDLIBS += -ldl -lstdc++
endif
ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl libstdc++ libhaggle

LOCAL_STATIC_LIBRARIES +=
endif

LOCAL_CFLAGS:=-O2 -g
LOCAL_CFLAGS+=-DOS_ANDROID -DOS_LINUX -DDEBUG

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := clitool

LOCAL_UNSTRIPPED_PATH := $(TARGET_ROOT_OUT_SBIN_UNSTRIPPED)

include $(BUILD_EXECUTABLE)

endif
