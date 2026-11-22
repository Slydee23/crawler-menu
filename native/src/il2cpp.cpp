#include "il2cpp.h"

#include <atomic>
#include <cstring>
#include <unistd.h>

#include <shadowhook.h>
#include <xdl.h>

#include "log.h"

namespace il2cpp {
namespace {

struct Domain;
struct Assembly;
struct Image;
struct Class;
struct MethodInfo;  // first field is the native code pointer
struct Thread;

struct Api {
    Domain* (*domain_get)();
    const Assembly** (*domain_get_assemblies)(const Domain*, size_t* count);
    const Image* (*assembly_get_image)(const Assembly*);
    const char* (*image_get_name)(const Image*);
    Class* (*class_from_name)(const Image*, const char* name_space, const char* name);
    const MethodInfo* (*class_get_method_from_name)(Class*, const char* name, int argc);
    Thread* (*thread_attach)(Domain*);
    void (*thread_detach)(Thread*);
};

Api g_api{};
std::atomic<bool> g_init_done{false};

using InitFn = int (*)(const char* domain_name);
InitFn g_orig_init = nullptr;

int hook_il2cpp_init(const char* domain_name) {
    SHADOWHOOK_STACK_SCOPE();
    const int result = SHADOWHOOK_CALL_PREV(hook_il2cpp_init, domain_name);
    g_init_done.store(true, std::memory_order_release);
    LOGI("il2cpp: il2cpp_init(%s) finished", domain_name != nullptr ? domain_name : "null");
    return result;
}
uintptr_t g_base = 0;
thread_local Thread* t_thread = nullptr;

template <typename T>
bool resolve(void* handle, const char* sym, T& out) {
    out = reinterpret_cast<T>(xdl_sym(handle, sym, nullptr));
    if (out == nullptr) LOGE("il2cpp: export %s not found", sym);
    return out != nullptr;
}

bool load_api() {
    void* handle = xdl_open("libil2cpp.so", XDL_DEFAULT);  // nullptr until the game loaded it
    if (handle == nullptr) return false;

    xdl_info_t info{};
    if (xdl_info(handle, XDL_DI_DLINFO, &info) == 0) g_base = reinterpret_cast<uintptr_t>(info.dli_fbase);

    bool ok = resolve(handle, "il2cpp_domain_get", g_api.domain_get) &&
              resolve(handle, "il2cpp_domain_get_assemblies", g_api.domain_get_assemblies) &&
              resolve(handle, "il2cpp_assembly_get_image", g_api.assembly_get_image) &&
              resolve(handle, "il2cpp_image_get_name", g_api.image_get_name) &&
              resolve(handle, "il2cpp_class_from_name", g_api.class_from_name) &&
              resolve(handle, "il2cpp_class_get_method_from_name", g_api.class_get_method_from_name) &&
              resolve(handle, "il2cpp_thread_attach", g_api.thread_attach) &&
              resolve(handle, "il2cpp_thread_detach", g_api.thread_detach);
    xdl_close(handle);
    return ok;
}

// Image names are "Pancake.dll"; accept the name with or without the extension.
bool image_matches(const char* image_name, const char* assembly) {
    const size_t n = std::strlen(assembly);
    return std::strncmp(image_name, assembly, n) == 0 &&
           (image_name[n] == '\0' || std::strcmp(image_name + n, ".dll") == 0);
}

const Image* find_image(const char* assembly) {
    size_t count = 0;
    const Assembly** assemblies = g_api.domain_get_assemblies(g_api.domain_get(), &count);
    for (size_t i = 0; assemblies != nullptr && i < count; ++i) {
        const Image* image = g_api.assembly_get_image(assemblies[i]);
        if (image != nullptr && image_matches(g_api.image_get_name(image), assembly)) return image;
    }
    return nullptr;
}

}  // namespace

bool install_init_hook() {
    // Returns a stub even while libil2cpp.so is not loaded yet (the hook stays pending).
    void* stub = shadowhook_hook_sym_name("libil2cpp.so", "il2cpp_init", reinterpret_cast<void*>(hook_il2cpp_init),
                                          reinterpret_cast<void**>(&g_orig_init));
    if (stub == nullptr) {
        const int err = shadowhook_get_errno();
        LOGE("il2cpp: hook il2cpp_init failed (%d: %s)", err, shadowhook_to_errmsg(err));
        return false;
    }
    return true;
}

bool wait_until_ready(const char* assembly, int timeout_ms) {
    constexpr int kStepMs = 100;
    for (int waited = 0; waited < timeout_ms; waited += kStepMs, usleep(kStepMs * 1000)) {
        // Touching the domain before il2cpp_init() returns aborts the process.
        if (!g_init_done.load(std::memory_order_acquire)) continue;
        if (g_api.domain_get == nullptr && !load_api()) {
            LOGE("il2cpp: API exports missing");
            return false;
        }
        if (find_image(assembly) == nullptr) continue;
        t_thread = g_api.thread_attach(g_api.domain_get());
        LOGI("il2cpp: runtime ready (%s loaded, libil2cpp at 0x%lx) after %d ms", assembly,
             static_cast<unsigned long>(g_base), waited);
        return true;
    }
    return false;
}

void detach() {
    if (t_thread != nullptr) g_api.thread_detach(t_thread);
    t_thread = nullptr;
}

void* find_method(const char* assembly, const char* name_space, const char* klass, const char* method, int argc,
                  const void** method_info) {
    const Image* image = find_image(assembly);
    if (image == nullptr) {
        LOGE("il2cpp: assembly %s not found", assembly);
        return nullptr;
    }
    Class* cls = g_api.class_from_name(image, name_space, klass);
    if (cls == nullptr) {
        LOGE("il2cpp: class %s.%s not found", name_space, klass);
        return nullptr;
    }
    const MethodInfo* info = g_api.class_get_method_from_name(cls, method, argc);
    if (info == nullptr) {
        LOGE("il2cpp: method %s.%s::%s(%d args) not found", name_space, klass, method, argc);
        return nullptr;
    }
    if (method_info != nullptr) *method_info = info;
    return *reinterpret_cast<void* const*>(info);
}

uintptr_t base() { return g_base; }

}  // namespace il2cpp
