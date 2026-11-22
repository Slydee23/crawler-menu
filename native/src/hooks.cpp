#include "hooks.h"

#include <cstdint>

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

// --- PlayerModel.Update() ---------------------------------------------------------------
// Runs every frame on the game thread while a player exists. Carries out menu requests.
using UpdateFn = void (*)(void* self, const void* method);
UpdateFn g_orig_player_update = nullptr;

void hook_player_update(void* self, const void* method) {
    SHADOWHOOK_STACK_SCOPE();
    SHADOWHOOK_CALL_PREV(hook_player_update, self, method);
    features::mark_player_active();

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
}
