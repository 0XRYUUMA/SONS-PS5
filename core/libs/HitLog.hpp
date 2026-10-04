#ifndef CORE_LIBS_HITLOG_HPP
#define CORE_LIBS_HITLOG_HPP

#include <atomic>
#include "prx/common/StderrLog.hpp"
#include <cstdio>

#define APS5_HIT(tag, ...) \
    do { \
        static std::atomic<int> aps5HitCount{0}; \
        if (aps5HitCount.fetch_add(1, std::memory_order_relaxed) < 3) { \
            aps5::LogErr( "[" tag "] " __VA_ARGS__); \
            aps5::LogChar(aps5::LogStdErr, '\n'); \
            aps5::LogFlush(aps5::LogStdErr); \
        } \
    } while (0)

#endif
