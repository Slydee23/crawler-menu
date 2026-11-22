#pragma once
#include <cstdint>

// Minimal access to the game's IL2CPP runtime through its exported C API.
// Everything is resolved by name at runtime, so no hard-coded offsets.
namespace il2cpp {

// Blocks until libil2cpp.so is loaded and `assembly` (e.g. "Pancake") is initialized,
// then attaches the calling thread to the IL2CPP domain. Returns false on timeout.
bool wait_until_ready(const char* assembly, int timeout_ms);

// Detaches the calling thread again (call before the thread exits).
void detach();

// Returns the native code address of a managed method, or nullptr if not found.
// `argc` is the number of parameters (-1 = any overload).
void* find_method(const char* assembly, const char* name_space, const char* klass, const char* method, int argc);

// Load address of libil2cpp.so (for logging offsets that can be compared with a dump).
uintptr_t base();

}  // namespace il2cpp
