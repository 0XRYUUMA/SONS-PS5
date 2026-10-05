#pragma once

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace AgcDriver::Diagnostics {

inline bool Enabled() {
    static const bool enabled = std::getenv("APS5_FRAME_DIAGNOSTICS") != nullptr;
    return enabled;
}

inline std::atomic<unsigned long long>& FrameCounter() {
    static std::atomic<unsigned long long> frame{0};
    return frame;
}

inline unsigned long long NextFrame() {
    return FrameCounter().fetch_add(1, std::memory_order_relaxed) + 1;
}

// Inline statics are distinct in separate DLLs. Serialize their writes across
// modules and append complete lines instead of opening independent "w" streams.
inline void ReportAtTime(unsigned long long frame, std::chrono::steady_clock::time_point captured,
                         const char* category, const char* detail, double value = 0.0) {
    if (!Enabled()) return;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
#ifdef _WIN32
    static HANDLE shared = CreateMutexA(nullptr, FALSE, "Local\\APS5FrameDiagnosticsLog");
    if (!shared) return;
    const DWORD wait = WaitForSingleObject(shared, INFINITE);
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) return;
#endif
    if (std::FILE* file = std::fopen("frame-diagnostics.log", "a")) {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(captured.time_since_epoch()).count();
        std::fprintf(file, "%lld frame=%llu category=%s detail=%s value_ms=%.2f\n",
                     static_cast<long long>(ms), frame, category, detail, value);
        std::fclose(file);
    }
#ifdef _WIN32
    ReleaseMutex(shared);
#endif
}

inline void ReportAt(unsigned long long frame, const char* category, const char* detail, double value = 0.0) {
    ReportAtTime(frame, std::chrono::steady_clock::now(), category, detail, value);
}

inline void Report(const char* category, const char* detail, double value = 0.0) {
    ReportAt(FrameCounter().load(std::memory_order_relaxed), category, detail, value);
}

} // namespace AgcDriver::Diagnostics
