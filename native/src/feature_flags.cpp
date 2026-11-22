#include "feature_flags.h"

#include <jni.h>

#include "log.h"

namespace features {
namespace {

constexpr int kMaxId = 64;
std::atomic<bool> g_enabled[kMaxId];

bool valid(int id) { return id > 0 && id < kMaxId; }

}  // namespace

void set_enabled(int id, bool enabled) {
    if (!valid(id)) {
        LOGW("Unknown feature id %d", id);
        return;
    }
    g_enabled[id].store(enabled, std::memory_order_relaxed);
    LOGI("Feature %d %s", id, enabled ? "ON" : "OFF");
}

bool is_enabled(int id) { return valid(id) && g_enabled[id].load(std::memory_order_relaxed); }

}  // namespace features

// --- JNI: crawlermenu.NativeBridge -------------------------------------------------------

extern "C" JNIEXPORT void JNICALL Java_crawlermenu_NativeBridge_setFeature(JNIEnv*, jclass, jint id,
                                                                          jboolean enabled) {
    features::set_enabled(id, enabled == JNI_TRUE);
}
