#pragma once
#include <android/log.h>

#define CM_TAG "CrawlerMenu"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, CM_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, CM_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, CM_TAG, __VA_ARGS__)
