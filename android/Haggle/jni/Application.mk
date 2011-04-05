APP_PROJECT_PATH := $(shell pwd)
APP_ABI := armeabi
APP_MODULES := \
	libhagglekernel_jni

APP_OPTIM=release
APP_BUILD_SCRIPT=jni/Android.mk
