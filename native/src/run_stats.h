#pragma once

// Run stats editor: the menu shows the player's PlayerStats values and can set them.
// The list of stats lives here (single source of truth for the Java side, via JNI).
// Values are read and written on the game thread only (from the PlayerModel.Update hook);
// the menu reads a cached snapshot and queues set requests.
namespace run_stats {

struct Def {
    const char* field;  // PlayerStats field (a SimpleStat)
    const char* label;  // shown in the menu
    bool max;           // edit the stat's maximum instead of its current value
};

int count();
const Def& def(int index);

// Cached value of stat `index` (NaN = not available).
float value(int index);

// --- game thread ---
void publish(int index, float value);
// Returns true and the requested value if the menu asked to set stat `index`.
bool take_request(int index, float* value);

// --- menu thread ---
void request(int index, float value);

}  // namespace run_stats
