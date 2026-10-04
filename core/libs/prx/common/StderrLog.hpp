#ifndef CORE_LIBS_PRX_COMMON_STDERRLOG_HPP
#define CORE_LIBS_PRX_COMMON_STDERRLOG_HPP

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <io.h>

struct _OVERLAPPED;
extern "C" {
__declspec(dllimport) void* __stdcall GetStdHandle(unsigned long nStdHandle);
__declspec(dllimport) int __stdcall WriteFile(void* file, const void* buffer, unsigned long bytes, unsigned long* written, struct _OVERLAPPED* overlapped);
}
#else
#include <unistd.h>
#endif

namespace aps5 {

#ifdef _WIN32

inline void RawWrite(int descriptor, const void* data, unsigned bytes) {
    void* handle = GetStdHandle(descriptor == 1 ? static_cast<unsigned long>(-11) : static_cast<unsigned long>(-12));
    if (handle == nullptr || handle == reinterpret_cast<void*>(static_cast<intptr_t>(-1))) return;
    unsigned long written = 0;
    WriteFile(handle, data, bytes, &written, nullptr);
}
#endif

constexpr int LogStdOut = 1;
constexpr int LogStdErr = 2;

inline void LogWrite(int descriptor, const char* format, va_list args) {
    char buffer[4096];
    const int length = vsnprintf(buffer, sizeof(buffer), format, args);
    if (length <= 0) return;
    const unsigned bytes = length < static_cast<int>(sizeof(buffer)) ? static_cast<unsigned>(length) : static_cast<unsigned>(sizeof(buffer) - 1);
#ifdef _WIN32
    RawWrite(descriptor, buffer, bytes);
#else
    ::write(descriptor, buffer, bytes);
#endif
}

inline void LogErr(const char* format, ...) {
    va_list args;
    va_start(args, format);
    LogWrite(2, format, args);
    va_end(args);
}

inline void LogOut(const char* format, ...) {
    va_list args;
    va_start(args, format);
    LogWrite(1, format, args);
    va_end(args);
}

inline void LogBytes(int descriptor, const void* data, size_t bytes) {
    if (descriptor < 0 || data == nullptr || bytes == 0) return;
#ifdef _WIN32
    RawWrite(descriptor, data, static_cast<unsigned>(bytes));
#else
    ::write(descriptor, data, bytes);
#endif
}

inline void LogChar(int descriptor, char value) { LogBytes(descriptor, &value, 1); }

inline void LogString(int descriptor, const char* text) {
    if (text != nullptr) LogBytes(descriptor, text, strlen(text));
}

inline void LogFlush(int) {}

}

#endif
