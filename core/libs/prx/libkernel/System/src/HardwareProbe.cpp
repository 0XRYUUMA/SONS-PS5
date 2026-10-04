#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "prx/common/StderrLog.hpp"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>

namespace {

struct Probe {
    std::uintptr_t address = 0;
    std::string name;
    std::uint8_t original = 0;
    bool indirect = false;
    std::atomic<std::uint64_t> hits{0};
};

constexpr std::size_t MaxProbes = 2048;
Probe* g_table[MaxProbes];
std::atomic<std::size_t> g_tableSize{0};

Probe* FindProbe(std::uintptr_t address) {
    const auto size = g_tableSize.load(std::memory_order_acquire);
    for (std::size_t i = 0; i < size; ++i) {
        if (g_table[i]->address == address) return g_table[i];
    }
    return nullptr;
}

void AddProbe(Probe* probe) {
    const auto size = g_tableSize.load(std::memory_order_relaxed);
    g_table[size] = probe;
    g_tableSize.store(size + 1, std::memory_order_release);
}

std::atomic<std::uint64_t> g_sequence{0};
std::uint64_t g_maxLogged = 300;
thread_local Probe* t_pending = nullptr;

void Arm(Probe* probe) {
    *reinterpret_cast<volatile std::uint8_t*>(probe->address) = 0xCC;
}

void Disarm(Probe* probe) {
    *reinterpret_cast<volatile std::uint8_t*>(probe->address) = probe->original;
}

float LowFloat(const M128A& value) {
    float result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

bool ReadSafe(std::uintptr_t address, void* out, std::size_t bytes) {
    SIZE_T done = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), out, bytes, &done) && done == bytes;
}

std::string ScanLists(std::uintptr_t object) {
    std::string text;
    for (std::size_t offset = 0x10; offset <= 0x90; offset += 8) {
        std::uintptr_t list = 0, items = 0;
        std::int32_t count = 0;
        if (!ReadSafe(object + offset, &list, sizeof(list)) || list < 0x10000) continue;
        if (!ReadSafe(list + 0x10, &items, sizeof(items)) || items < 0x10000) continue;
        if (!ReadSafe(list + 0x18, &count, sizeof(count)) || count < 3 || count > 64) continue;
        float v[12] = {};
        if (!ReadSafe(items + 0x20, v, sizeof(v))) continue;
        char part[300];
        std::snprintf(part, sizeof(part), " [+%zx n=%d: %g,%g,%g | %g,%g,%g | %g,%g,%g | %g,%g,%g]", offset, count, static_cast<double>(v[0]), static_cast<double>(v[1]), static_cast<double>(v[2]), static_cast<double>(v[3]), static_cast<double>(v[4]), static_cast<double>(v[5]), static_cast<double>(v[6]), static_cast<double>(v[7]), static_cast<double>(v[8]), static_cast<double>(v[9]), static_cast<double>(v[10]), static_cast<double>(v[11]));
        text += part;
    }
    return text;
}

LONG CALLBACK ProbeHandler(EXCEPTION_POINTERS* info) {
    const auto code = info->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_BREAKPOINT) {
        const auto address = reinterpret_cast<std::uintptr_t>(info->ExceptionRecord->ExceptionAddress);
        Probe* probe = FindProbe(address);
        if (probe == nullptr) return EXCEPTION_CONTINUE_SEARCH;
        const auto count = ++probe->hits;
        if (count <= g_maxLogged) {
            const auto& c = *info->ContextRecord;
            const auto sequence = ++g_sequence;
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() % 10000000;
            aps5::LogErr("[probe] #%llu t=%lld %s n=%llu tid=%lu rdi=%llx rsi=%llx rdx=%llx xmm0=%g xmm1=%g\n", static_cast<unsigned long long>(sequence), static_cast<long long>(ms), probe->name.c_str(), static_cast<unsigned long long>(count), static_cast<unsigned long>(GetCurrentThreadId()), static_cast<unsigned long long>(c.Rdi), static_cast<unsigned long long>(c.Rsi), static_cast<unsigned long long>(c.Rdx), static_cast<double>(LowFloat(c.Xmm0)), static_cast<double>(LowFloat(c.Xmm1)));
        }

        if ((count <= 40 || count % 40 == 0) && probe->name.size() > 4 && probe->name.compare(probe->name.size() - 4, 4, "@obj") == 0) {
            std::uint64_t words[36] = {};
            std::uint64_t tail[16] = {};
            double time = 0;
            std::memcpy(&time, &info->ContextRecord->Xmm0, sizeof(time));
            if (ReadSafe(static_cast<std::uintptr_t>(info->ContextRecord->Rdi), words, sizeof(words))) {
                std::string line;
                char part[24];
                for (int i = 0; i < 36; ++i) { std::snprintf(part, sizeof(part), " %llx", static_cast<unsigned long long>(words[i])); line += part; }
                ReadSafe(static_cast<std::uintptr_t>(info->ContextRecord->Rdi) + 0x5d0, tail, sizeof(tail));
                std::string tailText;
                for (int i = 0; i < 16; ++i) { std::snprintf(part, sizeof(part), " %llx", static_cast<unsigned long long>(tail[i])); tailText += part; }
                aps5::LogErr("[probe-obj] %s n=%llu rdi=%llx xmm0=%.6f:%s | tail:%s\n", probe->name.c_str(), static_cast<unsigned long long>(count), static_cast<unsigned long long>(info->ContextRecord->Rdi), time, line.c_str(), tailText.c_str());
            }
        }
        if (count <= g_maxLogged && probe->name.size() > 5 && probe->name.compare(probe->name.size() - 5, 5, "@scan") == 0) {
            const auto lists = ScanLists(static_cast<std::uintptr_t>(info->ContextRecord->Rdi));
            aps5::LogErr("[probe-scan] %s n=%llu%s\n", probe->name.c_str(), static_cast<unsigned long long>(count), lists.c_str());
        }
        Disarm(probe);
        info->ContextRecord->Rip = address;
        info->ContextRecord->EFlags |= 0x100;
        t_pending = probe;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (code == EXCEPTION_SINGLE_STEP && t_pending != nullptr) {
        Arm(t_pending);
        t_pending = nullptr;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void Watcher(std::string module, std::vector<Probe*> probes) {
    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        const HMODULE base = GetModuleHandleA(module.c_str());
        if (base == nullptr) continue;

        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        AddVectoredExceptionHandler(1, ProbeHandler);

        std::vector<Probe*> ready;
        std::vector<Probe*> late;
        for (auto* probe : probes) {
            probe->address += reinterpret_cast<std::uintptr_t>(base);
            if (probe->indirect) { late.push_back(probe); continue; }
            DWORD old = 0;
            if (!VirtualProtect(reinterpret_cast<void*>(probe->address), 1, PAGE_EXECUTE_READWRITE, &old)) continue;
            probe->original = *reinterpret_cast<std::uint8_t*>(probe->address);
            AddProbe(probe);
            ready.push_back(probe);
        }
        std::size_t armed = 0;
        for (auto* probe : ready) { Arm(probe); ++armed; }

        const auto started = std::chrono::steady_clock::now();
        while (!late.empty() && std::chrono::steady_clock::now() - started < std::chrono::seconds(600)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            for (auto it = late.begin(); it != late.end();) {
                std::uintptr_t target = 0;
                if (!ReadSafe((*it)->address, &target, sizeof(target)) || target < 0x10000) { ++it; continue; }
                aps5::LogErr("[probe] indirect %s resolved to %p (exe %p)\n", (*it)->name.c_str(), reinterpret_cast<void*>(target), static_cast<void*>(GetModuleHandleA(nullptr)));
                (*it)->address = target;
                DWORD old = 0;
                if (VirtualProtect(reinterpret_cast<void*>(target), 1, PAGE_EXECUTE_READWRITE, &old)) {
                    (*it)->original = *reinterpret_cast<std::uint8_t*>(target);
                    AddProbe(*it);
                    Arm(*it);
                    ++armed;
                }
                it = late.erase(it);
            }
        }
        aps5::LogErr("[probe] module %s at %p, %zu of %zu probes armed\n", module.c_str(), static_cast<void*>(base), armed, probes.size());
        return;
    }
}

struct Starter {
    Starter() {
        std::string module;
        std::vector<Probe*> probes;
        if (const char* path = std::getenv("APS5_PROBE_FILE")) {
            std::ifstream file(path);
            std::string line;
            bool first = true;
            while (std::getline(file, line)) {
                while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
                if (line.empty()) continue;
                if (first) { module = line; first = false; continue; }
                const auto space = line.find(' ');
                if (space == std::string::npos) continue;
                auto* probe = new Probe;
                auto number = line.substr(0, space);
                if (!number.empty() && number[0] == '*') { probe->indirect = true; number.erase(0, 1); }
                probe->address = static_cast<std::uintptr_t>(std::strtoull(number.c_str(), nullptr, 16));
                probe->name = line.substr(space + 1);
                probes.push_back(probe);
            }
        } else if (const char* spec = std::getenv("APS5_PROBES")) {
            std::string text = spec;
            std::size_t at = 0;
            bool first = true;
            while (at <= text.size()) {
                auto next = text.find(';', at);
                if (next == std::string::npos) next = text.size();
                const auto part = text.substr(at, next - at);
                at = next + 1;
                if (first) { module = part; first = false; continue; }
                const auto colon = part.find(':');
                if (colon == std::string::npos) continue;
                auto* probe = new Probe;
                probe->address = static_cast<std::uintptr_t>(std::strtoull(part.substr(0, colon).c_str(), nullptr, 16));
                probe->name = part.substr(colon + 1);
                probes.push_back(probe);
            }
        }
        if (module.empty() || probes.empty()) return;
        if (const char* limit = std::getenv("APS5_PROBE_MAX")) g_maxLogged = static_cast<std::uint64_t>(std::strtoull(limit, nullptr, 10));
        aps5::LogErr("[probe] %zu probes requested for %s\n", probes.size(), module.c_str());
        std::thread(Watcher, module, probes).detach();
    }
} starter;

}
#endif
