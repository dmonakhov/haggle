LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_JAVA_LIBRARIES := haggle

# Build all java files in the src subdirectory
LOCAL_SRC_FILES := $(call all-java-files-under,src)

# Name of the APK to build
LOCAL_PACKAGE_NAME := LuckyMe

# Tell it to build an APK
include $(BUILD_PACKAGE)
