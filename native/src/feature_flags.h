#pragma once
#include <atomic>

// Feature switches shared between the menu (JNI, UI thread) and the game hooks (game thread).
// IDs must match crawlermenu.NativeBridge on the Java side.
namespace features {

enum Id : int {
    kCoinMultiplier = 1,  // coin pickups give 10x
};

constexpr int kCoinMultiplierFactor = 10;

void set_enabled(int id, bool enabled);
bool is_enabled(int id);

}  // namespace features
