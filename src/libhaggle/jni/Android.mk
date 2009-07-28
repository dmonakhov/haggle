LOCAL_PATH := $(my-dir)

include $(CLEAR_VARS)

# Do not build for simulator
ifneq ($(TARGET_SIMULATOR),true)

haggle_dir := \
       java/org/haggle

haggle_src_files := $(call all-java-files-under,$(haggle_dir))

LOCAL_SRC_FILES := $(haggle_src_files)

LOCAL_NO_STANDARD_LIBRARIES := true
LOCAL_JAVA_LIBRARIES := core
LOCAL_MODULE := haggle

include $(BUILD_JAVA_LIBRARY)

include $(call first-makefiles-under,$(LOCAL_PATH))

endif
