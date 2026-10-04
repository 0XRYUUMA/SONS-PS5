#include "prx/libSceAgcDriver/Execution/include/WorkerSampler.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <timeapi.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <stop_token>
#include <system_error>
#include <thread>
#include <vector>
#endif

namespace AgcDriver {

#ifdef _WIN32
namespace {

std::jthread processSamplerThread;
std::jthread workerSamplerThread;

struct Sampler {
    HANDLE target = nullptr;
    std::string path;
    std::mutex mutex;
    std::map<std::uint64_t, std::pair<std::uint64_t, std::uint64_t>> counts;
    std::uint64_t samples = 0;
    std::map<std::vector<std::uint64_t>, std::uint64_t> stacks;

    ~Sampler() noexcept(false) {
        if (target != nullptr && !CloseHandle(target)) throw std::system_error(GetLastError(), std::system_category(), "Closing sampler thread handle");
    }

    void sample() {
        CONTEXT context{};
        context.ContextFlags = CONTEXT_FULL;

        std::vector<std::uint64_t> frames;
        frames.reserve(32);
        if (SuspendThread(target) == static_cast<DWORD>(-1)) return;
        const bool captured = GetThreadContext(target, &context) != 0;
        const std::uint64_t sampleRsp = captured ? context.Rsp : 0;
        if (captured) {

            for (int depth = 0; depth < 12 && context.Rip != 0; ++depth) {
                frames.push_back(context.Rip);
                DWORD64 imageBase = 0;
                auto* entry = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr);
                if (entry == nullptr) break;
                PVOID handler = nullptr;
                DWORD64 establisher = 0;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, entry, &context, &handler, &establisher, nullptr);
            }
        }
        if (captured && frames.size() <= 2) {

            static const auto driver = reinterpret_cast<std::uintptr_t>(GetModuleHandleA("libSceAgcDriver.prx"));
            const auto* sp = reinterpret_cast<const std::uintptr_t*>(captured ? sampleRsp : 0);
            std::size_t found = 0;
            for (int i = 0; i < 256 && found < 6 && sp != nullptr; ++i) {
                std::uintptr_t value = 0;
                if (!ReadProcessMemory(GetCurrentProcess(), sp + i, &value, sizeof(value), nullptr)) break;
                if (driver != 0 && value > driver && value < driver + 0x2000000) { frames.push_back(value); ++found; }
            }
        }
        ResumeThread(target);
        std::lock_guard lock(mutex);
        ++samples;
        if (!frames.empty()) ++stacks[frames];
        for (std::size_t i = 0; i < frames.size(); ++i) {
            auto& count = counts[frames[i]];
            if (i == 0) ++count.first;
            ++count.second;
        }
    }

    void write() {
        std::lock_guard lock(mutex);
        std::FILE* file = std::fopen(path.c_str(), "w");
        if (file == nullptr) return;
        std::fprintf(file, "# %llu samples\n", static_cast<unsigned long long>(samples));
        for (const auto& [address, count] : counts) {
            HMODULE module = nullptr;
            char name[MAX_PATH] = "?";
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(address), &module)) GetModuleFileNameA(module, name, sizeof(name));
            const char* base = std::strrchr(name, '\\');
            std::fprintf(file, "%s 0x%llx %llu %llu\n", base ? base + 1 : name, static_cast<unsigned long long>(address - reinterpret_cast<std::uint64_t>(module)), static_cast<unsigned long long>(count.first), static_cast<unsigned long long>(count.second));
        }
        std::fclose(file);
        {
            std::FILE* sf = std::fopen((path + ".stacks").c_str(), "w");
            if (sf != nullptr) {
                for (const auto& [stack, n] : stacks) {
                    std::fprintf(sf, "%llu", static_cast<unsigned long long>(n));
                    for (const auto address : stack) {
                        HMODULE module = nullptr;
                        char name[MAX_PATH] = "?";
                        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(address), &module)) GetModuleFileNameA(module, name, MAX_PATH);
                        const char* base = std::strrchr(name, '\\');
                        std::fprintf(sf, " %s:0x%llx", base ? base + 1 : name, static_cast<unsigned long long>(address - reinterpret_cast<std::uint64_t>(module)));
                    }
                    std::fprintf(sf, "\n");
                }
                std::fclose(sf);
            }
        }
        if (std::getenv("APS5_SAMPLE_WINDOW") != nullptr) { counts.clear(); stacks.clear(); samples = 0; }
    }
};

struct ProcessSampler {
    std::string path;
    DWORD self = 0;
    std::map<DWORD, Sampler> threads;
    std::uint64_t rounds = 0;

    std::chrono::steady_clock::time_point enumerated{};

    void enumerate() {
        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return;
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        const DWORD process = GetCurrentProcessId();
        for (BOOL more = Thread32First(snapshot, &entry); more; more = Thread32Next(snapshot, &entry)) {
            if (entry.th32OwnerProcessID != process || entry.th32ThreadID == self) continue;
            auto& thread = threads[entry.th32ThreadID];
            if (thread.target == nullptr) thread.target = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
        }
        CloseHandle(snapshot);
        enumerated = std::chrono::steady_clock::now();
    }

    void sample() {
        if (threads.empty() || std::chrono::steady_clock::now() - enumerated > std::chrono::seconds(5)) enumerate();
        for (auto& [id, thread] : threads) {
            if (thread.target != nullptr) thread.sample();
        }
        ++rounds;
    }

    void write() {
        std::FILE* file = std::fopen(path.c_str(), "w");
        if (file == nullptr) return;
        std::fprintf(file, "# %llu rounds\n", static_cast<unsigned long long>(rounds));
        for (auto& [id, thread] : threads) {
            std::lock_guard lock(thread.mutex);
            std::vector<std::pair<std::uint64_t, std::pair<std::uint64_t, std::uint64_t>>> hot(thread.counts.begin(), thread.counts.end());
            std::sort(hot.begin(), hot.end(), [](const auto& a, const auto& b) { return a.second.second > b.second.second; });
            std::uint64_t leaves = 0;
            for (const auto& [address, count] : hot) leaves += count.first;
            if (leaves == 0) continue;
            std::fprintf(file, "thread %lu samples %llu\n", static_cast<unsigned long>(id), static_cast<unsigned long long>(thread.samples));
            for (std::size_t i = 0; i < hot.size() && i < 40; ++i) {
                const auto address = hot[i].first;
                HMODULE module = nullptr;
                char name[MAX_PATH] = "?";
                if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(address), &module)) GetModuleFileNameA(module, name, sizeof(name));
                const char* base = std::strrchr(name, '\\');
                std::fprintf(file, "  %s 0x%llx %llu %llu\n", base ? base + 1 : name, static_cast<unsigned long long>(address - reinterpret_cast<std::uint64_t>(module)), static_cast<unsigned long long>(hot[i].second.first), static_cast<unsigned long long>(hot[i].second.second));
            }
        }
        std::fclose(file);
    }
};

void StartProcessSampler() {
    const char* path = std::getenv("APS5_SAMPLE_THREADS");
    if (path == nullptr) return;
    processSamplerThread = std::jthread([path = std::string(path)](std::stop_token token) {
        auto sampler = std::make_unique<ProcessSampler>();
        sampler->path = path;
        sampler->self = GetCurrentThreadId();
        auto flushed = std::chrono::steady_clock::now();
        while (!token.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            sampler->sample();
            if (std::chrono::steady_clock::now() - flushed > std::chrono::seconds(20)) {
                sampler->write();
                flushed = std::chrono::steady_clock::now();
            }
        }
        sampler->write();
    });
}

}

void StartWorkerSampler() {
    StartProcessSampler();
    if (std::getenv("APS5_SAMPLE_WORKER") != nullptr) timeBeginPeriod(1);
    const char* path = std::getenv("APS5_SAMPLE_WORKER");
    if (path == nullptr) return;
    auto sampler = std::make_unique<Sampler>();
    sampler->path = path;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &sampler->target, THREAD_ALL_ACCESS, FALSE, 0)) throw std::system_error(GetLastError(), std::system_category(), "Duplicating sampler thread handle");
    workerSamplerThread = std::jthread([sampler = std::move(sampler)](std::stop_token token) {
        auto flushed = std::chrono::steady_clock::now();
        while (!token.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            sampler->sample();
            if (std::chrono::steady_clock::now() - flushed > std::chrono::seconds(20)) {
                sampler->write();
                flushed = std::chrono::steady_clock::now();
            }
        }
        sampler->write();
    });
}

void StopWorkerSampler() {
    processSamplerThread.request_stop();
    workerSamplerThread.request_stop();
    if (processSamplerThread.joinable()) processSamplerThread.join();
    if (workerSamplerThread.joinable()) workerSamplerThread.join();
}
#else
void StartWorkerSampler() {}
void StopWorkerSampler() {}
#endif

}
