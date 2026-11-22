// CrawlerMenu native library entry point.
#include <jni.h>

#include <shadowhook.h>

#include "log.h"

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) {
    LOGI("libcrawlermenu.so loaded");

    // debuggable=true: ShadowHook logs details under the tag "shadowhook_tag".
    const int rc = shadowhook_init(SHADOWHOOK_MODE_SHARED, true);
    if (rc != 0) {
        LOGE("shadowhook_init failed (%d: %s)", rc, shadowhook_to_errmsg(rc));
    } else {
        LOGI("shadowhook_init ok (version %s)", shadowhook_get_version());
    }
    return JNI_VERSION_1_6;
}
