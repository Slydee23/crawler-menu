#include "run_stats.h"

#include <atomic>
#include <cmath>

#include <jni.h>

#include "feature_flags.h"
#include "log.h"

namespace run_stats {
namespace {

// Order = order in the menu.
// Charm is left out on purpose: a developer leftover without effect (same finding as in the
// InventoryCrawler save editor, where PowerUp_Charm is hidden).
constexpr Def kDefs[] = {
    {"Health", "Health", false},
    {"Health", "Max health", true},
    {"Armor", "Armor", false},
    {"Might", "Might", false},
    {"Amount", "Amount", false},
    {"Area", "Area", false},
    {"Duration", "Duration", false},
    {"Growth", "Growth", false},
    {"Greed", "Greed", false},
    {"Luck", "Luck", false},
    {"Recovery", "Recovery", false},
    {"Magnet", "Magnet", false},
    {"Mana", "Mana", false},
    {"DealCount", "Deal count", false},
    {"Curse", "Curse", false},
    {"Risk", "Risk", false},  // raises enemy damage and health (RiskDamage/HealthMultiplier)
    {"Scavenge", "Scavenge", false},
    {"RevivalStacks", "Revivals", false},
    {"Rerolls", "Rerolls", false},
    {"CardSkips", "Skips", false},
    {"Banishes", "Banishes", false},
};
constexpr int kCount = sizeof(kDefs) / sizeof(kDefs[0]);

struct Slot {
    std::atomic<float> value{NAN};
    std::atomic<bool> requested{false};
    std::atomic<float> request_value{0.0f};
};
Slot g_slots[kCount];

bool valid(int index) { return index >= 0 && index < kCount; }

}  // namespace

int count() { return kCount; }

const Def& def(int index) { return kDefs[index]; }

float value(int index) { return valid(index) ? g_slots[index].value.load(std::memory_order_relaxed) : NAN; }

void publish(int index, float v) {
    if (valid(index)) g_slots[index].value.store(v, std::memory_order_relaxed);
}

bool take_request(int index, float* v) {
    if (!valid(index) || !g_slots[index].requested.exchange(false)) return false;
    *v = g_slots[index].request_value.load();
    return true;
}

void request(int index, float v) {
    if (!valid(index)) return;
    g_slots[index].request_value.store(v);
    g_slots[index].requested.store(true);
}

}  // namespace run_stats

// --- JNI: crawlermenu.NativeBridge -------------------------------------------------------

// Labels of all editable stats, in menu order.
extern "C" JNIEXPORT jobjectArray JNICALL Java_crawlermenu_NativeBridge_getStatLabels(JNIEnv* env, jclass) {
    jclass string_class = env->FindClass("java/lang/String");
    jobjectArray labels = env->NewObjectArray(run_stats::count(), string_class, nullptr);
    for (int i = 0; i < run_stats::count(); i++) {
        jstring s = env->NewStringUTF(run_stats::def(i).label);
        env->SetObjectArrayElement(labels, i, s);
        env->DeleteLocalRef(s);
    }
    return labels;
}

// Current values in menu order, or null when no run is active. NaN = stat not available.
extern "C" JNIEXPORT jfloatArray JNICALL Java_crawlermenu_NativeBridge_getStatValues(JNIEnv* env, jclass) {
    if (!features::player_active()) return nullptr;
    const int n = run_stats::count();
    jfloatArray out = env->NewFloatArray(n);
    for (int i = 0; i < n; i++) {
        const float v = run_stats::value(i);
        env->SetFloatArrayRegion(out, i, 1, &v);
    }
    return out;
}

// Queues "set stat `index` to `value`" for the next game frame. False if no run is active.
extern "C" JNIEXPORT jboolean JNICALL Java_crawlermenu_NativeBridge_setStat(JNIEnv*, jclass, jint index,
                                                                           jfloat value) {
    if (index < 0 || index >= run_stats::count() || std::isnan(value)) return JNI_FALSE;
    if (!features::player_active()) return JNI_FALSE;
    run_stats::request(index, value);
    LOGI("setStat(%s = %.2f) queued", run_stats::def(index).label, value);
    return JNI_TRUE;
}
