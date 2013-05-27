LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)



LOCAL_MODULE := su_server
LOCAL_SRC_FILES := su_server.cpp

include $(BUILD_EXECUTABLE)

