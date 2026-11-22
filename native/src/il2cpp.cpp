#include "il2cpp.h"

#include <cstring>
#include <unistd.h>

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
    const Image* (*get_corlib)();
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

    bool ok = resolve(handle, "il2cpp_get_corlib", g_api.get_corlib) &&
              resolve(handle, "il2cpp_domain_get", g_api.domain_get) &&
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

bool wait_until_ready(const char* assembly, int timeout_ms) {
    constexpr int kStepMs = 100;
    bool api_loaded = false;
    for (int waited = 0; waited < timeout_ms; waited += kStepMs, usleep(kStepMs * 1000)) {
        if (!api_loaded) {
            api_loaded = load_api();
            if (!api_loaded) continue;
            LOGI("il2cpp: libil2cpp.so found at 0x%lx after %d ms", static_cast<unsigned long>(g_base), waited);
        }
        // corlib is set during il2cpp_init; before that, the domain must not be touched.
        if (g_api.get_corlib() == nullptr) continue;
        if (find_image(assembly) == nullptr) continue;
        t_thread = g_api.thread_attach(g_api.domain_get());
        LOGI("il2cpp: runtime ready (%s loaded) after %d ms", assembly, waited);
        return true;
    }
    return false;
}

void detach() {
    if (t_thread != nullptr) g_api.thread_detach(t_thread);
    t_thread = nullptr;
}

void* find_method(const char* assembly, const char* name_space, const char* klass, const char* method, int argc) {
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
    return *reinterpret_cast<void* const*>(info);
}

uintptr_t base() { return g_base; }

}  // namespace il2cpp
