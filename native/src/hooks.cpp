#include "hooks.h"

#include <atomic>
#include <cstdint>
#include <initializer_list>

#include <shadowhook.h>

#include "feature_flags.h"
#include "il2cpp.h"
#include "log.h"

namespace {

constexpr const char* kGameAssembly = "Pancake";
constexpr const char* kModelsNs = "Nosebleed.Pancake.Models";

// --- PlayerModel.AddCoins(int coins, bool useGreed) -------------------------------------
// IL2CPP calling convention: (this, args..., const MethodInfo*)
using AddCoinsFn = void (*)(void* self, int coins, bool use_greed, const void* method);
AddCoinsFn g_orig_add_coins = nullptr;      // original code, bypasses our hook
const void* g_add_coins_method = nullptr;   // MethodInfo* for calls we make ourselves

void hook_add_coins(void* self, int coins, bool use_greed, const void* method) {
    SHADOWHOOK_STACK_SCOPE();
    int amount = coins;
    // Only gains are multiplied; the game also calls AddCoins(0) all the time.
    if (coins > 0 && features::is_enabled(features::kCoinMultiplier)) {
        amount = coins * features::kCoinMultiplierFactor;
        LOGI("AddCoins: %d -> %d (coin multiplier)", coins, amount);
    }
    SHADOWHOOK_CALL_PREV(hook_add_coins, self, amount, use_greed, method);
}

// --- Infinite rerolls / skips / banishes ---------------------------------------------------
// The live counters are SimpleStat objects in PlayerStats (fields Rerolls, CardSkips, Banishes).
// While the feature is on, any decrease of exactly these three stats is dropped.
using GetPlayerStatsFn = void* (*)(void* player, const void* method);
using StatAddValueFn = float (*)(void* stat, float value, bool notify, const void* method);
using StatSetValueFn = void (*)(void* stat, float value, bool notify, const void* method);
using StatGetValueFn = float (*)(void* stat, const void* method);

GetPlayerStatsFn g_get_player_stats = nullptr;
const void* g_get_player_stats_method = nullptr;
StatGetValueFn g_stat_get_value = nullptr;
const void* g_stat_get_value_method = nullptr;
StatAddValueFn g_orig_stat_add_value = nullptr;   // original code, bypasses our hook
StatSetValueFn g_orig_stat_set_value = nullptr;   // original code, bypasses our hook
const void* g_stat_set_value_method = nullptr;

int g_off_rerolls = -1;
int g_off_skips = -1;
int g_off_banishes = -1;

// The three stat objects of the current player, refreshed every frame by PlayerModel.Update.
std::atomic<void*> g_stat_rerolls{nullptr};
std::atomic<void*> g_stat_skips{nullptr};
std::atomic<void*> g_stat_banishes{nullptr};

bool is_protected_stat(void* stat) {
    return stat != nullptr && (stat == g_stat_rerolls.load(std::memory_order_relaxed) ||
                               stat == g_stat_skips.load(std::memory_order_relaxed) ||
                               stat == g_stat_banishes.load(std::memory_order_relaxed));
}

bool infinite_choices_on() { return features::is_enabled(features::kInfiniteChoices); }

float hook_stat_add_value(void* stat, float value, bool notify, const void* method) {
    SHADOWHOOK_STACK_SCOPE();
    if (value < 0 && infinite_choices_on() && is_protected_stat(stat)) {
        return g_stat_get_value(stat, g_stat_get_value_method);  // keep the counter as it is
    }
    return SHADOWHOOK_CALL_PREV(hook_stat_add_value, stat, value, notify, method);
}

void hook_stat_set_value(void* stat, float value, bool notify, const void* method) {
    SHADOWHOOK_STACK_SCOPE();
    if (infinite_choices_on() && is_protected_stat(stat) && value < g_stat_get_value(stat, g_stat_get_value_method)) {
        return;  // drop decreases
    }
    SHADOWHOOK_CALL_PREV(hook_stat_set_value, stat, value, notify, method);
}

void* read_ref(void* object, int offset) {
    return offset < 0 ? nullptr : *reinterpret_cast<void**>(static_cast<char*>(object) + offset);
}

bool g_choice_hooks_ready = false;  // set once all of the above is resolved and hooked

// Called every frame from PlayerModel.Update.
void refresh_choice_stats(void* player) {
    if (!g_choice_hooks_ready) return;
    void* stats = g_get_player_stats(player, g_get_player_stats_method);
    if (stats == nullptr) return;
    void* rerolls = read_ref(stats, g_off_rerolls);
    void* skips = read_ref(stats, g_off_skips);
    void* banishes = read_ref(stats, g_off_banishes);
    g_stat_rerolls.store(rerolls, std::memory_order_relaxed);
    g_stat_skips.store(skips, std::memory_order_relaxed);
    g_stat_banishes.store(banishes, std::memory_order_relaxed);

    // A counter at 0 would leave the game's buttons disabled, so keep at least 1.
    if (!infinite_choices_on()) return;
    for (void* stat : {rerolls, skips, banishes}) {
        if (stat != nullptr && g_stat_get_value(stat, g_stat_get_value_method) < 1.0f) {
            g_orig_stat_set_value(stat, 1.0f, true, g_stat_set_value_method);
        }
    }
}

// --- PlayerModel.Update() ---------------------------------------------------------------
// Runs every frame on the game thread while a player exists. Carries out menu requests.
using UpdateFn = void (*)(void* self, const void* method);
UpdateFn g_orig_player_update = nullptr;

void hook_player_update(void* self, const void* method) {
    SHADOWHOOK_STACK_SCOPE();
    SHADOWHOOK_CALL_PREV(hook_player_update, self, method);
    features::mark_player_active();
    refresh_choice_stats(self);

    const int coins = features::take_queued_coins();
    if (coins > 0 && g_orig_add_coins != nullptr) {
        // Original AddCoins: no Greed scaling and no coin multiplier for menu grants.
        g_orig_add_coins(self, coins, false, g_add_coins_method);
        LOGI("Menu: added %d coins", coins);
    }
}

// --- Village gold: Globals.Progression (static) -> ProgressionData.AddCoins(long) ---------
using GetProgressionFn = void* (*)(const void* method);
using AddVillageGoldFn = void (*)(void* progression, int64_t coins, const void* method);
GetProgressionFn g_get_progression = nullptr;
const void* g_get_progression_method = nullptr;
AddVillageGoldFn g_add_village_gold = nullptr;
const void* g_add_village_gold_method = nullptr;

// --- InputManager.Update() --------------------------------------------------------------
// A global singleton that ticks every frame everywhere (menus, village, runs).
UpdateFn g_orig_input_update = nullptr;

void hook_input_update(void* self, const void* method) {
    SHADOWHOOK_STACK_SCOPE();
    SHADOWHOOK_CALL_PREV(hook_input_update, self, method);

    const int gold = features::take_queued_village_gold();
    if (gold <= 0) return;
    void* progression = g_get_progression(g_get_progression_method);
    if (progression == nullptr) {
        LOGW("Menu: village gold not added, ProgressionData not available");
        return;
    }
    g_add_village_gold(progression, gold, g_add_village_gold_method);
    LOGI("Menu: added %d village gold", gold);
}

bool hook(const char* name, void* target, void* proxy, void** orig) {
    if (target == nullptr) return false;
    if (shadowhook_hook_func_addr(target, proxy, orig) == nullptr) {
        const int err = shadowhook_get_errno();
        LOGE("hook %s failed (%d: %s)", name, err, shadowhook_to_errmsg(err));
        return false;
    }
    LOGI("hooked %s", name);
    return true;
}

void install_infinite_choices() {
    constexpr const char* kLogicNs = "Nosebleed.Pancake.GameLogic";
    g_get_player_stats = reinterpret_cast<GetPlayerStatsFn>(il2cpp::find_method(
        kGameAssembly, kModelsNs, "PlayerModel", "get_PlayerStats", 0, &g_get_player_stats_method));
    g_stat_get_value = reinterpret_cast<StatGetValueFn>(
        il2cpp::find_method(kGameAssembly, kLogicNs, "SimpleStat", "get_Value", 0, &g_stat_get_value_method));
    void* add_value = il2cpp::find_method(kGameAssembly, kLogicNs, "SimpleStat", "AddValue", 2);
    void* set_value =
        il2cpp::find_method(kGameAssembly, kLogicNs, "SimpleStat", "SetValue", 2, &g_stat_set_value_method);
    g_off_rerolls = il2cpp::field_offset(kGameAssembly, kLogicNs, "PlayerStats", "Rerolls");
    g_off_skips = il2cpp::field_offset(kGameAssembly, kLogicNs, "PlayerStats", "CardSkips");
    g_off_banishes = il2cpp::field_offset(kGameAssembly, kLogicNs, "PlayerStats", "Banishes");

    if (g_get_player_stats == nullptr || g_stat_get_value == nullptr || g_off_rerolls < 0 || g_off_skips < 0 ||
        g_off_banishes < 0) {
        LOGE("Infinite choices unavailable: game methods/fields not found");
        return;
    }
    // Both hooks are needed; if only one were active, the counters could still go down.
    if (hook("SimpleStat.AddValue", add_value, reinterpret_cast<void*>(hook_stat_add_value),
             reinterpret_cast<void**>(&g_orig_stat_add_value)) &&
        hook("SimpleStat.SetValue", set_value, reinterpret_cast<void*>(hook_stat_set_value),
             reinterpret_cast<void**>(&g_orig_stat_set_value))) {
        g_choice_hooks_ready = true;
        LOGI("Infinite choices ready (offsets rerolls=0x%x skips=0x%x banishes=0x%x)", g_off_rerolls, g_off_skips,
             g_off_banishes);
    }
}

}  // namespace

void install_game_hooks() {
    void* add_coins =
        il2cpp::find_method(kGameAssembly, kModelsNs, "PlayerModel", "AddCoins", 2, &g_add_coins_method);
    hook("PlayerModel.AddCoins", add_coins, reinterpret_cast<void*>(hook_add_coins),
         reinterpret_cast<void**>(&g_orig_add_coins));

    void* player_update = il2cpp::find_method(kGameAssembly, kModelsNs, "PlayerModel", "Update", 0);
    hook("PlayerModel.Update", player_update, reinterpret_cast<void*>(hook_player_update),
         reinterpret_cast<void**>(&g_orig_player_update));

    // Village gold: only offered when both methods and the per-frame hook are in place.
    g_get_progression = reinterpret_cast<GetProgressionFn>(il2cpp::find_method(
        kGameAssembly, "Nosebleed.Pancake.GameConfig.Accessors", "Globals", "get_Progression", 0,
        &g_get_progression_method));
    g_add_village_gold = reinterpret_cast<AddVillageGoldFn>(il2cpp::find_method(
        kGameAssembly, "Nosebleed.Pancake.MetaProgression", "ProgressionData", "AddCoins", 1,
        &g_add_village_gold_method));
    void* input_update =
        il2cpp::find_method(kGameAssembly, "Nosebleed.Pancake.InputSystem", "InputManager", "Update", 0);
    if (g_get_progression != nullptr && g_add_village_gold != nullptr &&
        hook("InputManager.Update", input_update, reinterpret_cast<void*>(hook_input_update),
             reinterpret_cast<void**>(&g_orig_input_update))) {
        features::set_village_gold_available(true);
    }

    install_infinite_choices();
}
