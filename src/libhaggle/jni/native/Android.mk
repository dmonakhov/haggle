LOCAL_PATH := $(my-dir)
include $(CLEAR_VARS)

# Do not build for simulator
ifneq ($(TARGET_SIMULATOR),true)

LOCAL_SRC_FILES := \
	javaclass.c \
	common.c \
	org_haggle_Attribute.c \
	org_haggle_DataObject.c \
	org_haggle_Handle.c \
	org_haggle_Interface.c \
	org_haggle_Node.c

LOCAL_C_INCLUDES += \
	$(JNI_H_INCLUDE) \
	$(LOCAL_PATH)/../../include \


LOCAL_CFLAGS :=-O2 -g
LOCAL_CFLAGS +=-DOS_ANDROID -DDEBUG -DDEBUG_LEAKS

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
LOCAL_LDLIBS += -ldl
endif
ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

LOCAL_UNSTRIPPED_PATH := $(TARGET_ROOT_OUT_LIB_UNSTRIPPED)

LOCAL_SHARED_LIBRARIES += libhaggle
LOCAL_PRELINK_MODULE := false
LOCAL_LDLIBS := -lhaggle

LOCAL_MODULE := libhaggle_jni

include $(BUILD_SHARED_LIBRARY)

endif
