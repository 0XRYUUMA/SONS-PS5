#include "prx/libc/include/General.hpp"
#include "prx/common/StderrLog.hpp"
#include <nid/NidCompute.hpp>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
thread_local std::array<char, 512> loaderError{};
thread_local bool pendingError = false;
void Error(const char* message) {
    std::snprintf(loaderError.data(), loaderError.size(), "%s", message);
    pendingError = true;
}
struct Module {
    void* native = nullptr;
    bool owned = true;
    bool global = false;
    ~Module() {
        if (owned && native) {
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(native));
#else
            ::dlclose(native);
#endif
        }
    }
};
std::mutex modulesMutex;
std::map<std::uintptr_t, std::shared_ptr<Module>> modules;
std::uintptr_t nextHandle = 0x20000000;

bool TraceLoader() {
    static const bool value = std::getenv("APS5_TRACE_DL") != nullptr;
    return value;
}
void Log(const char* format, ...) {
    if (!TraceLoader()) return;
    va_list args;
    va_start(args, format);
    aps5::LogString(aps5::LogStdErr, "[dl] ");
    aps5::LogWrite(2, format, args);
    aps5::LogChar(aps5::LogStdErr, '\n');
    va_end(args);
}
void* Symbol(Module& module, const char* name) {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module.native), name));
#else
    return ::dlsym(module.native, name);
#endif
}
void* FindSymbol(Module& module, const char* name) {
    if (auto* symbol = Symbol(module, name)) return symbol;
    const auto nid = Nid::ComputeNid(name, "");
    return Symbol(module, nid.c_str());
}
#ifdef _WIN32

void RunDeferredLifecycle(void* native) {
    static std::mutex lifecycleMutex;
    static std::set<void*> started;
    {
        std::lock_guard lock(lifecycleMutex);
        if (!started.insert(native).second) return;
    }
    const auto* base = static_cast<std::uint8_t*>(native);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;
    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (auto* end = section + nt->FileHeader.NumberOfSections; section != end; ++section) {
        if (std::memcmp(section->Name, ".ginit", 6) != 0) continue;
        const auto init = *reinterpret_cast<const std::uint32_t*>(base + section->VirtualAddress);
        if (!init) return;
        Log("running deferred module init at %p", base + init);
        reinterpret_cast<void (*)(std::uint64_t, std::uint64_t, std::uint64_t)>(base + init)(0, 0, 0);
        return;
    }
}
#endif
}

extern "C" {
char* APS5_VABI dlerror_nid_postfix() {
    if (!pendingError) return nullptr;
    pendingError = false;
    return loaderError.data();
}
void* APS5_VABI dlopen_nid_postfix_impl(const char* path, int flags);
void* APS5_VABI dlopen_nid_postfix(const char* path, int flags) {
    void* result = dlopen_nid_postfix_impl(path, flags);
    Log("dlopen(%s, 0x%x) -> %p%s", path ? path : "(null)", flags, result, result ? "" : (pendingError ? (std::string(" error: ") + loaderError.data()).c_str() : ""));
    return result;
}
void* APS5_VABI dlopen_nid_postfix_impl(const char* path, int flags) {
    if ((flags & ~0x103) || (flags & 3) == 0 || (flags & 3) == 3) {
        Error("dlopen: unsupported flags"); return nullptr;
    }
    try {
        auto module = std::make_shared<Module>();
        module->global = (flags & 0x100) != 0 || !path;
#ifdef _WIN32
        if (!path) {
            module->native = GetModuleHandleW(nullptr);
            module->owned = false;
        } else {
            if (!*path) { Error("dlopen: empty module path"); return nullptr; }
            auto resolved = ResolvePath_nid_no_patch(path);

            {
                wchar_t executable[MAX_PATH] = {};
                GetModuleFileNameW(nullptr, executable, MAX_PATH);
                const std::wstring base = std::filesystem::path(resolved).filename().wstring();
                for (const wchar_t* directory : {L"app0\\sce_module", L"sce_module"}) {
                    const auto guest = std::filesystem::path(executable).parent_path() / directory / (base + L".guest.prx");
                    std::error_code ignored;
                    if (std::filesystem::exists(guest, ignored)) { resolved = guest; break; }
                }
            }
            module->native = LoadLibraryExW(resolved.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        }
        if (!module->native) {
            char message[128];
            std::snprintf(message, sizeof(message), "dlopen: Windows loader error %lu (module must be host-compatible)", GetLastError());
            Error(message); return nullptr;
        }
#else
        const auto resolved = path ? ResolvePath_nid_no_patch(path).string() : std::string{};
        const int nativeFlags = ((flags & 3) == 1 ? RTLD_LAZY : RTLD_NOW) |
            ((flags & 0x100) ? RTLD_GLOBAL : RTLD_LOCAL);
        module->native = ::dlopen(path ? resolved.c_str() : nullptr, nativeFlags);
        if (!module->native) { Error(::dlerror()); return nullptr; }
#endif
        void* native = module->native;
        std::uintptr_t handle;
        {
            std::lock_guard lock(modulesMutex);
            handle = nextHandle++;
            modules.emplace(handle, std::move(module));
        }
#ifdef _WIN32

        RunDeferredLifecycle(native);
#endif
        return reinterpret_cast<void*>(handle);
    } catch (const std::exception& error) { Error(error.what()); return nullptr; }
}
void* APS5_VABI dlsym_nid_postfix_impl(void* handle, const char* name);
void* APS5_VABI dlsym_nid_postfix(void* handle, const char* name) {
    void* result = dlsym_nid_postfix_impl(handle, name);
    static int count = 0;
    if (TraceLoader() && (!result || count++ < 40)) Log("dlsym(%p, %s) -> %p", handle, name ? name : "(null)", result);
    return result;
}
void* APS5_VABI dlsym_nid_postfix_impl(void* handle, const char* name) {
    if (!name || !*name) { Error("dlsym: empty symbol name"); return nullptr; }
    try {
        std::vector<std::shared_ptr<Module>> search;
        {
            std::lock_guard lock(modulesMutex);
            if (handle == reinterpret_cast<void*>(static_cast<std::intptr_t>(-2))) {
                for (const auto& [key, module] : modules) if (module->global) search.push_back(module);
            } else {
                auto found = modules.find(reinterpret_cast<std::uintptr_t>(handle));
                if (found == modules.end()) { Error("dlsym: invalid or unsupported module handle"); return nullptr; }
                search.push_back(found->second);
            }
        }
        for (const auto& module : search) if (auto* result = FindSymbol(*module, name)) return result;
        Error("dlsym: symbol not found in supported module scope");
        return nullptr;
    } catch (const std::exception& error) { Error(error.what()); return nullptr; }
}
int APS5_VABI dlclose_nid_postfix(void* handle) {
    std::shared_ptr<Module> module;
    {
        std::lock_guard lock(modulesMutex);
        auto found = modules.find(reinterpret_cast<std::uintptr_t>(handle));
        if (found == modules.end()) { Error("dlclose: invalid module handle"); return -1; }
        module = std::move(found->second);
        modules.erase(found);
    }

    module.reset();
    return 0;
}
}
