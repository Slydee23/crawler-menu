#include "hooks.h"

#include <atomic>

#include <shadowhook.h>

#include "il2cpp.h"
#include "log.h"

namespace {

constexpr const char* kGameAssembly = "Pancake";
constexpr const char* kModelsNs = "Nosebleed.Pancake.Models";

// --- PlayerModel.AddCoins(int coins, bool useGreed) -------------------------------------
// IL2CPP calling convention: (this, args..., const MethodInfo*)
using AddCoinsFn = void (*)(void* self, int coins, bool use_greed, const void* method);
AddCoinsFn g_orig_add_coins = nullptr;

// Proof of concept for "Edit money": on the first real coin pickup, grant a one-time bonus
// through the game's own AddCoins (no Greed scaling), so UI and save state stay consistent.
constexpr int kTestBonusCoins = 1000;
std::atomic<bool> g_test_bonus_pending{true};

void hook_add_coins(void* self, int coins, bool use_greed, const void* method) {
    SHADOWHOOK_STACK_SCOPE();
    SHADOWHOOK_CALL_PREV(hook_add_coins, self, coins, use_greed, method);
    if (coins == 0) return;  // the game calls AddCoins(0) all the time

    LOGI("AddCoins(coins=%d, useGreed=%d)", coins, use_greed ? 1 : 0);
    if (coins > 0 && g_test_bonus_pending.exchange(false)) {
        SHADOWHOOK_CALL_PREV(hook_add_coins, self, kTestBonusCoins, false, method);
        LOGI("Granted one-time test bonus of %d coins", kTestBonusCoins);
    }
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
    void* add_coins = il2cpp::find_method(kGameAssembly, kModelsNs, "PlayerModel", "AddCoins", 2);
    hook("PlayerModel.AddCoins", add_coins, reinterpret_cast<void*>(hook_add_coins),
         reinterpret_cast<void**>(&g_orig_add_coins));
}
