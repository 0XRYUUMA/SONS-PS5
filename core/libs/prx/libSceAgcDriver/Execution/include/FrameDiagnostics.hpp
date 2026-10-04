#pragma once

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>

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

inline void Report(const char* category, const char* detail, double value = 0.0) {
    if (!Enabled()) return;
    static std::mutex mutex;
    static std::FILE* file = std::fopen("frame-diagnostics.log", "w");
    if (!file) return;
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    std::lock_guard<std::mutex> lock(mutex);
    std::fprintf(file, "%lld frame=%llu category=%s detail=%s value_ms=%.2f\n",
                 static_cast<long long>(now), FrameCounter().load(std::memory_order_relaxed),
                 category, detail, value);
    std::fflush(file);
}

} // namespace AgcDriver::Diagnostics
