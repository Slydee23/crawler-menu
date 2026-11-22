#pragma once
#include <atomic>

// State shared between the menu (JNI, Android UI thread) and the game hooks (Unity game thread).
// Game objects are only ever touched on the game thread: the menu queues requests here and a
// per-frame hook carries them out.
// IDs must match crawlermenu.NativeBridge on the Java side.
namespace features {

enum Id : int {
    kCoinMultiplier = 1,   // coin pickups give 10x
    kInfiniteChoices = 2,  // rerolls, skips and banishes never go down
};

constexpr int kCoinMultiplierFactor = 10;

void set_enabled(int id, bool enabled);
bool is_enabled(int id);

// --- Player presence: is a run active? ---
// Called by the PlayerModel.Update hook every frame.
void mark_player_active();
// True if PlayerModel.Update ran within the last second.
bool player_active();

// --- Queued coin grants ---
// Run coins (PlayerModel), carried out by the PlayerModel.Update hook.
void queue_coins(int amount);
// Returns the queued amount and resets it to 0.
int take_queued_coins();

// Village gold (ProgressionData.TotalCoins), carried out by a global per-frame hook.
void set_village_gold_available(bool available);
bool village_gold_available();
void queue_village_gold(int amount);
int take_queued_village_gold();

}  // namespace features
