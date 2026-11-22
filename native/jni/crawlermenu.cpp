// CrawlerMenu native library — proof of concept: log a line when loaded.
#include <android/log.h>
#include <jni.h>

#define TAG "CrawlerMenu"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) {
    LOGI("libcrawlermenu.so loaded");
    return JNI_VERSION_1_6;
}

// Also fires when loaded via System.loadLibrary before JNI_OnLoad is used.
__attribute__((constructor)) static void on_load() {
    LOGI("constructor ran");
}
