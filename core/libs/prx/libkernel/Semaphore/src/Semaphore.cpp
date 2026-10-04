#include "prx/libkernel/Semaphore/include/Semaphore.hpp"
#include "prx/common/StderrLog.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include <chrono>

#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

bool TraceSema() {
    static const bool value = std::getenv("APS5_TRACE_SEMA") != nullptr;
    return value;
}

bool AllowSemaLine(const std::string& name) {
    static std::mutex mutex;
    static std::unordered_map<std::string, int> printed;
    static int total = 0;
    std::lock_guard lock(mutex);
    if (total >= 400) return false;
    int& count = printed[name];
    if (count >= 16) return false;
    ++count;
    ++total;
    return true;
}

std::string SemaSite(const void* caller) {
#ifdef _WIN32
    HMODULE module = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<LPCWSTR>(caller), &module) && module != nullptr) {
        wchar_t path[MAX_PATH];
        if (GetModuleFileNameW(module, path, MAX_PATH) > 0) {
            wchar_t leaf[MAX_PATH];

            const wchar_t* slash = wcsrchr(path, L'\\');
            wcscpy(leaf, slash ? slash + 1 : path);
            char narrow[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, leaf, -1, narrow, MAX_PATH, nullptr, nullptr);
            return std::string(narrow) + "+0x" + [&] {
                char hex[24];
                std::snprintf(hex, sizeof(hex), "%llx", static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(caller) - reinterpret_cast<std::uintptr_t>(module)));
                return std::string(hex);
            }();
        }
    }
    char hex[24];
    std::snprintf(hex, sizeof(hex), "%p", caller);
    return std::string(hex);
#else
    char hex[24];
    std::snprintf(hex, sizeof(hex), "%p", caller);
    return std::string(hex);
#endif
}

void SemaLine(const std::string& name, const char* format, ...) {
    if (!AllowSemaLine(name)) return;
    va_list args;
    va_start(args, format);
    aps5::LogString(aps5::LogStdErr, "[sema] ");
    aps5::LogWrite(2, format, args);
    va_end(args);
    aps5::LogString(aps5::LogStdErr, "\n");
    aps5::LogFlush(aps5::LogStdErr);
}

std::uint64_t SemaThread() {
#ifdef _WIN32
    return static_cast<std::uint64_t>(GetCurrentThreadId());
#else
    return 0;
#endif
}

}

KernelSemaPrivate::KernelSemaPrivate(std::int32_t initCount, std::int32_t maxCount, std::string name, bool isFifo)
 : name(std::move(name)), tokenCount(initCount), maxCount(maxCount), isFifo(isFifo) {
}

extern "C" {

int APS5_VABI sceKernelCreateSema(KernelSema* sem, const char* name, uint32_t attr, int init, int max, void* opt) {
 (void)opt;
 if (sem == nullptr || name == nullptr || attr > 2 || init < 0 || max <= 0 || init > max) {
  APS5_INVALID_ARG_EX;
 }

 *sem = new KernelSemaPrivate(init, max, std::string(name), attr == 1);
 if (TraceSema()) {
  SemaLine((*sem)->name, "create %p name=%s init=%d max=%d tid=%llu caller=%s", *sem, (*sem)->name.c_str(), init, max,
           static_cast<unsigned long long>(SemaThread()), SemaSite(__builtin_return_address(0)).c_str());
 }
 return KERNEL_SEMA_OK;
}

int APS5_VABI sceKernelPollSema(KernelSema sem, int need) {
 if (sem == nullptr || need <= 0) {
  APS5_INVALID_ARG_EX;
 }

 std::lock_guard<std::mutex> lock(sem->mutex);
 if (sem->tokenCount < need) {
  return SCE_KERNEL_ERROR_EBUSY;
 }
 sem->tokenCount -= need;
 return KERNEL_SEMA_OK;
}

int APS5_VABI sceKernelSignalSema(KernelSema sem, int count) {
 if (sem == nullptr || count <= 0) {
  APS5_INVALID_ARG_EX;
 }

 const bool trace = TraceSema();
 std::lock_guard<std::mutex> lock(sem->mutex);
 if (count > sem->maxCount - sem->tokenCount) {
  return SCE_KERNEL_ERROR_EINVAL;
 }
 sem->tokenCount += count;
 sem->condition.NotifyAll();

 if (trace) {
  SemaLine(sem->name, "signal %p name=%s post=%d -> %d waiters=%d tid=%llu caller=%s", sem, sem->name.c_str(), count, sem->tokenCount, sem->waiterCount,
           static_cast<unsigned long long>(SemaThread()), SemaSite(__builtin_return_address(0)).c_str());
 }
 return KERNEL_SEMA_OK;
}

int APS5_VABI sceKernelWaitSema(KernelSema sem, int need, KernelUseconds* time) {
 if (sem == nullptr || need <= 0) {
  APS5_INVALID_ARG_EX;
 }

 std::unique_lock<std::mutex> lock(sem->mutex);
 ++sem->waiterCount;
 struct WaiterGuard {
  KernelSemaPrivate* sem;
  ~WaiterGuard() {
   --sem->waiterCount;
   sem->condition.NotifyAll();
  }
 } waiterGuard{sem};

 if (TraceSema() && sem->tokenCount < need && !sem->deleted) {
  SemaLine(sem->name, "block %p name=%s need=%d have=%d tid=%llu caller=%s %s", sem, sem->name.c_str(), need, sem->tokenCount,
           static_cast<unsigned long long>(SemaThread()), SemaSite(__builtin_return_address(0)).c_str(),
           time == nullptr ? "infinite" : "timed");
 }

 const auto waitStart = std::chrono::steady_clock::now();
 const auto traceWait = [&](bool timedOut) {
  KernelParkLeave_nid_postfix();
  KernelTraceWait_nid_postfix("sema", __builtin_return_address(0), static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - waitStart).count()), timedOut);
 };
 KernelParkEnter_nid_postfix("sema", sem->name.c_str(), __builtin_return_address(0));
 if (time == nullptr) {
  sem->condition.Wait(lock, [&] { return sem->tokenCount >= need || sem->deleted; });
  traceWait(false);
  if (sem->deleted) {
   return SCE_KERNEL_ERROR_EACCES;
  }
  sem->tokenCount -= need;
  return KERNEL_SEMA_OK;
 }

 const bool acquired = sem->condition.WaitUntil(lock, TimedWait::DeadlineNanos(*time), [&] { return sem->tokenCount >= need || sem->deleted; });
 traceWait(!acquired);
 if (sem->deleted) {
  return SCE_KERNEL_ERROR_EACCES;
 }
 if (!acquired) {
  return SCE_KERNEL_ERROR_ETIMEDOUT;
 }
 sem->tokenCount -= need;
 return KERNEL_SEMA_OK;
}

int APS5_VABI sceKernelCancelSema(KernelSema sem, int count, int* threads) {
 (void)sem;
 (void)count;
 (void)threads;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelDeleteSema(KernelSema sem) {
 if (sem == nullptr) {
  APS5_INVALID_ARG_EX;
 }

 std::unique_lock<std::mutex> lock(sem->mutex);
 sem->deleted = true;
 sem->condition.NotifyAll();
 sem->condition.Wait(lock, [&] { return sem->waiterCount == 0; });
 lock.unlock();
 delete sem;
 return KERNEL_SEMA_OK;
}

}
