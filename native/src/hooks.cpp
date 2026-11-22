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
    const int factor = features::value(features::kCoinMultiplier);
    if (coins > 0 && factor > 1 && features::is_enabled(features::kCoinMultiplier)) {
        const int64_t multiplied = static_cast<int64_t>(coins) * factor;
        amount = multiplied > INT32_MAX ? INT32_MAX : static_cast<int>(multiplied);
        LOGI("AddCoins: %d -> %d (coin multiplier x%d)", coins, amount, factor);
    }
    SHADOWHOOK_CALL_PREV(hook_add_coins, self, amount, use_greed, method);
}

// --- Protected player stats: infinite rerolls/skips/banishes, godmode ----------------------
// The live values are SimpleStat objects in PlayerStats (fields Rerolls, CardSkips, Banishes,
// Health). While the matching feature is on, any decrease of that stat is dropped; increases
// (level-ups, healing) pass through. Damage itself is still calculated by the game (it lowers
// Health point by point afterwards), so on-hit effects keep working.
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
int g_off_health = -1;

// The stat objects of the current player, refreshed every frame by PlayerModel.Update.
std::atomic<void*> g_stat_rerolls{nullptr};
std::atomic<void*> g_stat_skips{nullptr};
std::atomic<void*> g_stat_banishes{nullptr};
std::atomic<void*> g_stat_health{nullptr};

bool infinite_choices_on() { return features::is_enabled(features::kInfiniteChoices); }
bool godmode_on() { return features::is_enabled(features::kGodmode); }

bool is_protected_stat(void* stat) {
    if (stat == nullptr) return false;
    if (infinite_choices_on() && (stat == g_stat_rerolls.load(std::memory_order_relaxed) ||
                                  stat == g_stat_skips.load(std::memory_order_relaxed) ||
                                  stat == g_stat_banishes.load(std::memory_order_relaxed))) {
        return true;
    }
    return godmode_on() && stat == g_stat_health.load(std::memory_order_relaxed);
}

float hook_stat_add_value(void* stat, float value, bool notify, const void* method) {
    SHADOWHOOK_STACK_SCOPE();
    if (value < 0 && is_protected_stat(stat)) {
        return g_stat_get_value(stat, g_stat_get_value_method);  // keep the value as it is
    }
    return SHADOWHOOK_CALL_PREV(hook_stat_add_value, stat, value, notify, method);
}

void hook_stat_set_value(void* stat, float value, bool notify, const void* method) {
    SHADOWHOOK_STACK_SCOPE();
    if (is_protected_stat(stat) && value < g_stat_get_value(stat, g_stat_get_value_method)) {
        return;  // drop decreases
    }
    SHADOWHOOK_CALL_PREV(hook_stat_set_value, stat, value, notify, method);
}

void* read_ref(void* object, int offset) {
    return offset < 0 ? nullptr : *reinterpret_cast<void**>(static_cast<char*>(object) + offset);
}

bool g_stat_hooks_ready = false;  // set once all of the above is resolved and hooked

// Called every frame from PlayerModel.Update.
void refresh_player_stats(void* player) {
    if (!g_stat_hooks_ready) return;
    void* stats = g_get_player_stats(player, g_get_player_stats_method);
    if (stats == nullptr) return;
    void* rerolls = read_ref(stats, g_off_rerolls);
    void* skips = read_ref(stats, g_off_skips);
    void* banishes = read_ref(stats, g_off_banishes);
    g_stat_rerolls.store(rerolls, std::memory_order_relaxed);
    g_stat_skips.store(skips, std::memory_order_relaxed);
    g_stat_banishes.store(banishes, std::memory_order_relaxed);
    g_stat_health.store(read_ref(stats, g_off_health), std::memory_order_relaxed);

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
    refresh_player_stats(self);

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

void install_stat_protection() {
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
    g_off_health = il2cpp::field_offset(kGameAssembly, kLogicNs, "PlayerStats", "Health");

    if (g_get_player_stats == nullptr || g_stat_get_value == nullptr || g_off_rerolls < 0 || g_off_skips < 0 ||
        g_off_banishes < 0 || g_off_health < 0) {
        LOGE("Stat protection unavailable: game methods/fields not found");
        return;
    }
    // Both hooks are needed; if only one were active, protected stats could still go down.
    if (hook("SimpleStat.AddValue", add_value, reinterpret_cast<void*>(hook_stat_add_value),
             reinterpret_cast<void**>(&g_orig_stat_add_value)) &&
        hook("SimpleStat.SetValue", set_value, reinterpret_cast<void*>(hook_stat_set_value),
             reinterpret_cast<void**>(&g_orig_stat_set_value))) {
        g_stat_hooks_ready = true;
        LOGI("Stat protection ready (offsets rerolls=0x%x skips=0x%x banishes=0x%x health=0x%x)", g_off_rerolls,
             g_off_skips, g_off_banishes, g_off_health);
    }
}

// --- No mana cost --------------------------------------------------------------------------
// Observed: playing a card calls PlayerModel.TryUseMana(cost), which lowers PlayerStats.Mana;
// whether a card is playable is decided by PlayerModel.CanAffordManaCost(cost) (asked constantly
// for every card in hand). While the feature is on, every card is affordable and using mana
// succeeds without spending any.
using CanAffordFn = bool (*)(void* self, int count, const void* method);
using TryUseManaFn = bool (*)(void* self, int count, bool update_view, const void* method);
CanAffordFn g_orig_can_afford = nullptr;
TryUseManaFn g_orig_try_use_mana = nullptr;

bool no_mana_cost_on() { return features::is_enabled(features::kNoManaCost); }

bool hook_can_afford_mana(void* self, int count, const void* method) {
    SHADOWHOOK_STACK_SCOPE();
    if (no_mana_cost_on()) return true;
    return SHADOWHOOK_CALL_PREV(hook_can_afford_mana, self, count, method);
}

bool hook_try_use_mana(void* self, int count, bool update_view, const void* method) {
    SHADOWHOOK_STACK_SCOPE();
    if (no_mana_cost_on() && count > 0) return true;  // pretend it was paid
    return SHADOWHOOK_CALL_PREV(hook_try_use_mana, self, count, update_view, method);
}

void install_no_mana_cost() {
    hook("PlayerModel.CanAffordManaCost",
         il2cpp::find_method(kGameAssembly, kModelsNs, "PlayerModel", "CanAffordManaCost", 1),
         reinterpret_cast<void*>(hook_can_afford_mana), reinterpret_cast<void**>(&g_orig_can_afford));
    hook("PlayerModel.TryUseMana", il2cpp::find_method(kGameAssembly, kModelsNs, "PlayerModel", "TryUseMana", 2),
         reinterpret_cast<void*>(hook_try_use_mana), reinterpret_cast<void**>(&g_orig_try_use_mana));
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

    install_stat_protection();
    install_no_mana_cost();

}
