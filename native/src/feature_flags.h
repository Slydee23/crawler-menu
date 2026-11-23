#pragma once
#include <atomic>

// State shared between the menu (JNI, Android UI thread) and the game hooks (Unity game thread).
// Game objects are only ever touched on the game thread: the menu queues requests here and a
// per-frame hook carries them out.
// IDs must match crawlermenu.NativeBridge on the Java side.
namespace features {

enum Id : int {
    kCoinMultiplier = 1,   // coin pickups are multiplied by the feature's value
    kInfiniteRerolls = 2,  // rerolls never go down (id kept from the former combined switch)
    kGodmode = 3,          // player health never goes down
    kNoManaCost = 4,       // cards are always affordable and cost no mana
    kXpMultiplier = 5,     // XP gains are multiplied by the feature's value
    kOneHitKill = 6,       // every hit on an enemy is lethal
    kComboNeverBreaks = 7, // the card combo never resets
    kInfiniteSkips = 8,    // card skips never go down
    kInfiniteBanishes = 9, // banishes never go down
};

void set_enabled(int id, bool enabled);
bool is_enabled(int id);

// Optional numeric setting per feature (e.g. the coin multiplier). Defaults to 0.
void set_value(int id, int value);
int value(int id);

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
