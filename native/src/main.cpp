// CrawlerMenu native library entry point.
#include <jni.h>
#include <pthread.h>

#include <shadowhook.h>

#include "hooks.h"
#include "il2cpp.h"
#include "log.h"

namespace {

constexpr const char* kGameAssembly = "Pancake";  // assembly with the game logic

// Runs off the main thread: waits for the IL2CPP runtime, then hooks game methods.
void* game_thread(void*) {
    if (!il2cpp::wait_until_ready(kGameAssembly, 60 * 1000)) {
        LOGE("il2cpp runtime not ready after 60 s, giving up");
        return nullptr;
    }

    install_game_hooks();

    il2cpp::detach();
    return nullptr;
}

}  // namespace

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) {
    LOGI("libcrawlermenu.so loaded");

    // debuggable=true: ShadowHook logs details under the tag "shadowhook_tag".
    const int rc = shadowhook_init(SHADOWHOOK_MODE_SHARED, true);
    if (rc != 0) {
        LOGE("shadowhook_init failed (%d: %s)", rc, shadowhook_to_errmsg(rc));
    } else {
        LOGI("shadowhook_init ok (version %s)", shadowhook_get_version());
        il2cpp::install_init_hook();
    }

    pthread_t thread;
    if (pthread_create(&thread, nullptr, game_thread, nullptr) == 0) {
        pthread_detach(thread);
    } else {
        LOGE("could not start game thread");
    }
    return JNI_VERSION_1_6;
}
