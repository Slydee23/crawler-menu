#include "hooks.h"

#include <atomic>
#include <cstdint>
#include <initializer_list>

#include <shadowhook.h>

#include "feature_flags.h"
#include "il2cpp.h"
#include "run_stats.h"
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

// --- Run stats editor ----------------------------------------------------------------------
// Publishes the player's stat values for the menu and applies queued "set" requests.
// See run_stats.cpp for the list of stats.
using StatGetMaxFn = float (*)(void* stat, const void* method);
using StatSetMaxFn = void (*)(void* stat, float max, bool notify, const void* method);
StatGetMaxFn g_stat_get_max = nullptr;
StatSetMaxFn g_stat_set_max = nullptr;
const void* g_stat_set_max_method = nullptr;
int g_run_stat_offsets[64];
int g_run_stat_frame = 0;  // game thread only
// Set last in install_run_stats(): the game thread may already be running update_run_stats().
std::atomic<bool> g_run_stats_ready{false};

void set_run_stat(void* stat, const run_stats::Def& def, float wanted) {
    const float before = def.max ? g_stat_get_max(stat, nullptr) : g_stat_get_value(stat, g_stat_get_value_method);
    if (def.max) {
        g_stat_set_max(stat, wanted, true, g_stat_set_max_method);
    } else {
        // Original SetValue: bypasses our protection hooks (godmode etc.) on purpose.
        g_orig_stat_set_value(stat, wanted, true, g_stat_set_value_method);
        // If the game capped the value at the stat's maximum, raise the maximum and set again.
        if (g_stat_get_value(stat, g_stat_get_value_method) + 0.001f < wanted) {
            g_stat_set_max(stat, wanted, true, g_stat_set_max_method);
            g_orig_stat_set_value(stat, wanted, true, g_stat_set_value_method);
        }
    }
    const float after = def.max ? g_stat_get_max(stat, nullptr) : g_stat_get_value(stat, g_stat_get_value_method);
    LOGI("Run stat %s: %.2f -> %.2f (wanted %.2f)", def.label, before, after, wanted);
}

void update_run_stats(void* stats) {
    if (!g_run_stats_ready.load(std::memory_order_acquire)) return;
    const bool publish = (g_run_stat_frame++ % 10) == 0;  // the menu polls twice a second
    for (int i = 0; i < run_stats::count(); i++) {
        const run_stats::Def& def = run_stats::def(i);
        void* stat = read_ref(stats, g_run_stat_offsets[i]);
        if (stat == nullptr) continue;
        float wanted;
        if (run_stats::take_request(i, &wanted)) {
            set_run_stat(stat, def, wanted);
        }
        if (publish) {
            run_stats::publish(i, def.max ? g_stat_get_max(stat, nullptr) : g_stat_get_value(stat, g_stat_get_value_method));
        }
    }
}

void install_run_stats() {
    constexpr const char* kLogicNs = "Nosebleed.Pancake.GameLogic";
    g_stat_get_max = reinterpret_cast<StatGetMaxFn>(
        il2cpp::find_method(kGameAssembly, kLogicNs, "SimpleStat", "GetMaxValue", 0));
    g_stat_set_max = reinterpret_cast<StatSetMaxFn>(
        il2cpp::find_method(kGameAssembly, kLogicNs, "SimpleStat", "SetMax", 2, &g_stat_set_max_method));
    int found = 0;
    for (int i = 0; i < run_stats::count(); i++) {
        g_run_stat_offsets[i] = il2cpp::field_offset(kGameAssembly, kLogicNs, "PlayerStats", run_stats::def(i).field);
        if (g_run_stat_offsets[i] >= 0) found++;
    }
    if (g_stat_get_max == nullptr || g_stat_set_max == nullptr) {
        LOGE("Run stats unavailable: SimpleStat.GetMaxValue/SetMax not found");
        return;
    }
    g_run_stats_ready.store(true, std::memory_order_release);
    LOGI("Run stats ready (%d of %d stats found)", found, run_stats::count());
}

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
    update_run_stats(stats);

    // A counter at 0 would leave the game's buttons disabled, so keep at least 1.
    if (!infinite_choices_on()) return;
    for (void* stat : {rerolls, skips, banishes}) {
        if (stat != nullptr && g_stat_get_value(stat, g_stat_get_value_method) < 1.0f) {
            g_orig_stat_set_value(stat, 1.0f, true, g_stat_set_value_method);
        }
    }
}

void apply_combo_never_breaks(void* player);  // defined with the combo feature below

// --- PlayerModel.Update() ---------------------------------------------------------------
// Runs every frame on the game thread while a player exists. Carries out menu requests.
using UpdateFn = void (*)(void* self, const void* method);
UpdateFn g_orig_player_update = nullptr;

void hook_player_update(void* self, const void* method) {
    SHADOWHOOK_STACK_SCOPE();
    SHADOWHOOK_CALL_PREV(hook_player_update, self, method);
    features::mark_player_active();
    refresh_player_stats(self);
    apply_combo_never_breaks(self);

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
        install_run_stats();
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

// --- XP multiplier ---------------------------------------------------------------------------
// The multiplier is applied at the entry points (enemy XP, growth XP) so the game's whole
// level-up flow sees the bigger amount. Multiplying inside AddXp raised the level silently
// without offering level-up choices.
using AddSourceXpFn = int (*)(void* self, int base_xp, bool force, const void* method);
AddSourceXpFn g_orig_add_enemy_xp = nullptr;
AddSourceXpFn g_orig_add_growth_xp = nullptr;

int multiply_xp(int base_xp) {
    const int factor = features::value(features::kXpMultiplier);
    if (base_xp <= 0 || factor <= 1 || !features::is_enabled(features::kXpMultiplier)) return base_xp;
    const int64_t multiplied = static_cast<int64_t>(base_xp) * factor;
    return multiplied > INT32_MAX ? INT32_MAX : static_cast<int>(multiplied);
}

int hook_add_enemy_xp(void* self, int base_xp, bool force, const void* method) {
    SHADOWHOOK_STACK_SCOPE();
    const int amount = multiply_xp(base_xp);
    if (amount != base_xp) LOGI("Enemy XP: %d -> %d (multiplier)", base_xp, amount);
    return SHADOWHOOK_CALL_PREV(hook_add_enemy_xp, self, amount, force, method);
}

int hook_add_growth_xp(void* self, int base_xp, bool force, const void* method) {
    SHADOWHOOK_STACK_SCOPE();
    const int amount = multiply_xp(base_xp);
    if (amount != base_xp) LOGI("Growth XP: %d -> %d (multiplier)", base_xp, amount);
    return SHADOWHOOK_CALL_PREV(hook_add_growth_xp, self, amount, force, method);
}

// --- One-hit kill ----------------------------------------------------------------------------
// EnemyModel.ResolveDamage(double damage, CardModel damagingCard, bool eraseItems,
//                          int gorgeousMoonXp, bool isSplashDamage, string damageMetric)
//   -> DamageResult { double ArmorDamage, HealthDamage, OverkillDamage }  (returned in d0-d2)
// While the feature is on, every hit deals at least the enemy's health + armor + 1, so the
// game's own armor and overkill handling still runs.
struct DamageResult {
    double armor_damage;
    double health_damage;
    double overkill_damage;
};
using EnemyResolveDamageFn = DamageResult (*)(void* enemy, double damage, void* card, bool erase_items,
                                              int moon_xp, bool splash, void* metric, const void* method);
using DoubleGetterFn = double (*)(void* self, const void* method);
EnemyResolveDamageFn g_orig_enemy_resolve_damage = nullptr;
DoubleGetterFn g_enemy_get_health = nullptr;
DoubleGetterFn g_enemy_get_armor = nullptr;

DamageResult hook_enemy_resolve_damage(void* enemy, double damage, void* card, bool erase_items, int moon_xp,
                                       bool splash, void* metric, const void* method) {
    SHADOWHOOK_STACK_SCOPE();
    double dealt = damage;
    if (damage > 0 && features::is_enabled(features::kOneHitKill)) {
        const double lethal = g_enemy_get_health(enemy, nullptr) + g_enemy_get_armor(enemy, nullptr) + 1.0;
        if (dealt < lethal) dealt = lethal;
    }
    return SHADOWHOOK_CALL_PREV(hook_enemy_resolve_damage, enemy, dealt, card, erase_items, moon_xp, splash, metric,
                                method);
}

void install_one_hit_kill() {
    g_enemy_get_health = reinterpret_cast<DoubleGetterFn>(
        il2cpp::find_method(kGameAssembly, kModelsNs, "EnemyModel", "get_Health", 0));
    g_enemy_get_armor = reinterpret_cast<DoubleGetterFn>(
        il2cpp::find_method(kGameAssembly, kModelsNs, "EnemyModel", "get_Armor", 0));
    if (g_enemy_get_health == nullptr || g_enemy_get_armor == nullptr) {
        LOGE("One-hit kill unavailable: EnemyModel getters not found");
        return;
    }
    hook("EnemyModel.ResolveDamage", il2cpp::find_method(kGameAssembly, kModelsNs, "EnemyModel", "ResolveDamage", 6),
         reinterpret_cast<void*>(hook_enemy_resolve_damage), reinterpret_cast<void**>(&g_orig_enemy_resolve_damage));
}

// --- Combo never breaks ------------------------------------------------------------------------
// PlayerModel has a developer property DebugIsInCombo: while it is set, every card continues the
// combo (verified in game; ending the turn still ends the combo as usual). While the feature is
// on, the PlayerModel.Update hook keeps it set and clears it again when the feature is turned off.
using BoolGetterFn = bool (*)(void* self, const void* method);
using BoolSetterFn = void (*)(void* self, bool value, const void* method);

BoolGetterFn g_get_debug_in_combo = nullptr;
BoolSetterFn g_set_debug_in_combo = nullptr;
bool g_combo_forced = false;  // game thread only

void apply_combo_never_breaks(void* player) {
    if (g_get_debug_in_combo == nullptr || g_set_debug_in_combo == nullptr) return;
    const bool on = features::is_enabled(features::kComboNeverBreaks);
    if (on && !g_get_debug_in_combo(player, nullptr)) {
        g_set_debug_in_combo(player, true, nullptr);
        g_combo_forced = true;
    } else if (!on && g_combo_forced) {
        g_set_debug_in_combo(player, false, nullptr);
        g_combo_forced = false;
    }
}

void install_combo_never_breaks() {
    g_get_debug_in_combo = reinterpret_cast<BoolGetterFn>(
        il2cpp::find_method(kGameAssembly, kModelsNs, "PlayerModel", "get_DebugIsInCombo", 0));
    g_set_debug_in_combo = reinterpret_cast<BoolSetterFn>(
        il2cpp::find_method(kGameAssembly, kModelsNs, "PlayerModel", "set_DebugIsInCombo", 1));
    if (g_get_debug_in_combo == nullptr || g_set_debug_in_combo == nullptr) {
        LOGE("Combo never breaks unavailable: DebugIsInCombo not found");
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

    install_stat_protection();
    install_no_mana_cost();

    hook("PlayerModel.AddEnemyXp", il2cpp::find_method(kGameAssembly, kModelsNs, "PlayerModel", "AddEnemyXp", 2),
         reinterpret_cast<void*>(hook_add_enemy_xp), reinterpret_cast<void**>(&g_orig_add_enemy_xp));
    hook("PlayerModel.AddGrowthXp", il2cpp::find_method(kGameAssembly, kModelsNs, "PlayerModel", "AddGrowthXp", 2),
         reinterpret_cast<void*>(hook_add_growth_xp), reinterpret_cast<void**>(&g_orig_add_growth_xp));
    install_one_hit_kill();
    install_combo_never_breaks();
}
