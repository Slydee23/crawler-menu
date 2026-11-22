LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := crawlermenu
LOCAL_SRC_FILES := crawlermenu.cpp
LOCAL_LDLIBS    := -llog -ldl
LOCAL_CPPFLAGS  := -std=c++20 -fno-exceptions -fno-rtti -fvisibility=hidden
include $(BUILD_SHARED_LIBRARY)
