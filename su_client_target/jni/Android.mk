LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := su_client
LOCAL_SRC_FILES := su_client.cpp

include $(BUILD_EXECUTABLE)

