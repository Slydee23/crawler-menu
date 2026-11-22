#include "feature_flags.h"

#include <climits>
#include <cstdint>
#include <ctime>

#include <jni.h>

#include "log.h"

namespace features {
namespace {

constexpr int kMaxId = 64;
std::atomic<bool> g_enabled[kMaxId];

std::atomic<int64_t> g_last_player_update_ms{0};
std::atomic<int> g_queued_coins{0};
std::atomic<int> g_queued_village_gold{0};
std::atomic<bool> g_village_gold_available{false};

bool valid(int id) { return id > 0 && id < kMaxId; }

// Saturating add so repeated taps can't overflow.
void saturating_add(std::atomic<int>& target, int amount) {
    int current = target.load();
    int next;
    do {
        next = (current > INT_MAX - amount) ? INT_MAX : current + amount;
    } while (!target.compare_exchange_weak(current, next));
}

int64_t now_ms() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

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

void mark_player_active() { g_last_player_update_ms.store(now_ms(), std::memory_order_relaxed); }

bool player_active() { return now_ms() - g_last_player_update_ms.load(std::memory_order_relaxed) < 1000; }

void queue_coins(int amount) { saturating_add(g_queued_coins, amount); }

int take_queued_coins() { return g_queued_coins.exchange(0); }

void set_village_gold_available(bool available) { g_village_gold_available.store(available); }

bool village_gold_available() { return g_village_gold_available.load(); }

void queue_village_gold(int amount) { saturating_add(g_queued_village_gold, amount); }

int take_queued_village_gold() { return g_queued_village_gold.exchange(0); }

}  // namespace features

// --- JNI: crawlermenu.NativeBridge -------------------------------------------------------

extern "C" JNIEXPORT void JNICALL Java_crawlermenu_NativeBridge_setFeature(JNIEnv*, jclass, jint id,
                                                                          jboolean enabled) {
    features::set_enabled(id, enabled == JNI_TRUE);
}

// Queues coins for the next game frame: run coins during a run, village gold otherwise.
// Returns 1 = run coins, 2 = village gold, 0 = nothing queued.
// Must match NativeBridge.ADDED_* on the Java side.
extern "C" JNIEXPORT jint JNICALL Java_crawlermenu_NativeBridge_addCoins(JNIEnv*, jclass, jint amount) {
    if (amount <= 0) return 0;
    if (features::player_active()) {
        features::queue_coins(amount);
        LOGI("addCoins(%d) queued for the run", amount);
        return 1;
    }
    if (features::village_gold_available()) {
        features::queue_village_gold(amount);
        LOGI("addCoins(%d) queued as village gold", amount);
        return 2;
    }
    LOGI("addCoins(%d) ignored: no run and village gold unavailable", amount);
    return 0;
}
