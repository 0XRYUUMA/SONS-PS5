#include <cstdint>
#include "prx/common/StderrLog.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {
using GuestHandler = void (APS5_VABI *)(int, void*);
constexpr int MaxSignals = 128;
std::atomic<GuestHandler> handlers[MaxSignals];

constexpr std::size_t XStateBytes = 1024;
struct alignas(64) ExceptionFrame {
    alignas(64) char xstate[XStateBytes];
    CONTEXT context;
    int signum;
    void* mcontext;
    char mcontextStorage[0x400];
};
struct NtAlert { using Fn = LONG (NTAPI *)(HANDLE); };
bool IsGuestAddress(DWORD64 address) {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(address), &module) || !module) return false;
    if (module == GetModuleHandleW(nullptr)) return true;
    wchar_t name[MAX_PATH] = {};
    GetModuleFileNameW(module, name, MAX_PATH);
    const std::wstring path(name);
    return path.find(L".guest.prx") != std::wstring::npos;
}
void NudgeThread(Pthread thread) {
    static const auto alertById = reinterpret_cast<LONG (NTAPI *)(ULONG_PTR)>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtAlertThreadByThreadId"));
    if (alertById) alertById(GetThreadId(static_cast<HANDLE>(thread->nativeHandle)));
}

unsigned XMaskLow() {
    unsigned a = 1, b = 0, c = 0, d = 0;
    __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
    if ((c & (1u << 27)) == 0) return 0;
    unsigned low = 0, high = 0;
    __asm__ volatile("xgetbv" : "=a"(low), "=d"(high) : "c"(0));
    return low & 7u;
}
}
extern "C" unsigned DeliverXMaskLow;
extern "C" unsigned DeliverXMaskHigh;
unsigned DeliverXMaskLow = XMaskLow();
unsigned DeliverXMaskHigh = 0;
namespace {
void DeliverExceptionImpl(ExceptionFrame* frame) {
    auto handler = handlers[frame->signum].load();
    if (handler) handler(frame->signum, frame->mcontext);
    if (DeliverXMaskLow != 0) {
        const unsigned low = DeliverXMaskLow, high = DeliverXMaskHigh;
        __asm__ volatile("xrstor64 (%0)" : : "r"(frame->xstate), "a"(low), "d"(high) : "memory");
    }
    RtlRestoreContext(&frame->context, nullptr);
}

}

extern "C" void DeliverExceptionThunk();
extern "C" void* DeliverExceptionTarget;
void* DeliverExceptionTarget = reinterpret_cast<void*>(&DeliverExceptionImpl);
__asm__(".text\n"
        ".globl DeliverExceptionThunk\n"
        "DeliverExceptionThunk:\n"
        "movl DeliverXMaskLow(%rip), %eax\n"
        "testl %eax, %eax\n"
        "jz 1f\n"
        "movl DeliverXMaskHigh(%rip), %edx\n"
        "xsave64 (%rbx)\n"
        "1:\n"
        "movq %rbx, %rcx\n"
        "jmp *DeliverExceptionTarget(%rip)\n");

static std::atomic<unsigned> gcedHanded, gcedInjected, gcedSkipped, gcedRun;

static unsigned gcWaitMillis() {
    static const unsigned value = [] { const char* text = std::getenv("APS5_GC_WAIT_MS"); return text ? static_cast<unsigned>(std::strtoul(text, nullptr, 10)) : 1000u; }();
    return value;
}

void RunExceptionHandlerInline(int signum) {
    const auto handler = handlers[signum].load();
    if (!handler) return;
    aps5::LogErr("[gced] ran signum=%d run=%u\n", signum, gcedRun.fetch_add(1) + 1);
    alignas(16) char mcontext[0x400] = {};
    *reinterpret_cast<void**>(mcontext + 0xf8) = mcontext;
    handler(signum, mcontext);
}

extern "C" {

int APS5_VABI sceKernelInstallExceptionHandler(int signum, void* handler) {
    if (signum < 0 || signum >= MaxSignals) return static_cast<int>(0x80020016);
    aps5::LogErr( "[EXC] install handler signum=%d handler=%p\n", signum, handler);
    handlers[signum].store(reinterpret_cast<GuestHandler>(handler));
    return 0;
}

int APS5_VABI sceKernelRemoveExceptionHandler(int signum) {
    if (signum < 0 || signum >= MaxSignals) return static_cast<int>(0x80020016);
    handlers[signum].store(nullptr);
    return 0;
}

int APS5_VABI sceKernelRaiseException(Pthread thread, int signum) {
    if (signum < 0 || signum >= MaxSignals) return static_cast<int>(0x80020016);
    const auto handler = handlers[signum].load();
    if (!thread || thread->threadId == std::this_thread::get_id()) {
        if (handler) { alignas(16) char mcontext[0x400] = {}; *reinterpret_cast<void**>(mcontext + 0xf8) = mcontext; handler(signum, mcontext); }
        return 0;
    }
    if (!handler) return 0;
    HANDLE native = static_cast<HANDLE>(thread->nativeHandle);

    CONTEXT context{};
    bool interrupted = false;
    bool handed = false;

    const auto giveUpAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(gcWaitMillis());
    for (int attempt = 0;; ++attempt) {
        if (attempt >= 4096 && std::chrono::steady_clock::now() >= giveUpAt) break;
        if (SuspendThread(native) == static_cast<DWORD>(-1)) return static_cast<int>(0x80020003);
        context = {};
        context.ContextFlags = CONTEXT_ALL;
        const bool readable = GetThreadContext(native, &context) != 0;

        thread->pendingException.store(signum);
        if (thread->inWait.load()) {
            handed = true;
            break;
        }
        thread->pendingException.store(0);
        if (readable && IsGuestAddress(context.Rip)) {
            interrupted = true;
            break;
        }
        ResumeThread(native);
        if (!readable) return static_cast<int>(0x80020003);
        if (attempt < 64) SwitchToThread(); else if (attempt < 4096) Sleep(0); else Sleep(1);
    }

    if (handed) {
        ResumeThread(native);
        if (thread->wakeEvent) SetEvent(static_cast<HANDLE>(thread->wakeEvent));
        aps5::LogErr("[gced] hand tid=%u h=%u inj=%u skip=%u\n", GetThreadId(native),
                     gcedHanded.fetch_add(1) + 1, gcedInjected.load(), gcedSkipped.load());
        return 0;
    }
    if (!interrupted) {

        {
            char where[400] = {};
            int used = 0;
            DWORD64 candidates[8] = {context.Rip};
            int found = 1;
            const auto* stack = reinterpret_cast<const DWORD64*>(context.Rsp);
            for (int slot = 0; slot < 128 && found < 8; ++slot) {
                DWORD64 value = 0;
                if (!ReadProcessMemory(GetCurrentProcess(), stack + slot, &value, sizeof(value), nullptr)) break;
                HMODULE owner = nullptr;
                if (value > 0x10000 && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(value), &owner) && owner != nullptr) candidates[found++] = value;
            }
            for (int index = 0; index < found && used < static_cast<int>(sizeof(where)) - 80; ++index) {
                HMODULE owner = nullptr;
                wchar_t name[MAX_PATH] = {};
                if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(candidates[index]), &owner) && owner != nullptr) GetModuleFileNameW(owner, name, MAX_PATH);
                const wchar_t* base = wcsrchr(name, L'\\');
                used += std::snprintf(where + used, sizeof(where) - used, " %ls+0x%llx", base ? base + 1 : name, static_cast<unsigned long long>(candidates[index] - reinterpret_cast<DWORD64>(owner)));
            }
            aps5::LogErr("[gced] skipped tid=%u at%s\n", GetThreadId(native), where);
        }
        aps5::LogErr("[gced] skip tid=%u h=%u inj=%u skip=%u\n", GetThreadId(native),
                     gcedHanded.load(), gcedInjected.load(), gcedSkipped.fetch_add(1) + 1);
        return static_cast<int>(0x80020003);
    }

    auto top = (context.Rsp - 0x200 - sizeof(ExceptionFrame)) & ~static_cast<DWORD64>(0x3f);
    auto* frame = reinterpret_cast<ExceptionFrame*>(top);
    std::memset(&frame->context, 0, sizeof(*frame) - offsetof(ExceptionFrame, context));
    std::memset(frame->xstate, 0, 576);
    frame->context = context;
    frame->signum = signum;
    frame->mcontext = frame->mcontextStorage;

    *reinterpret_cast<void**>(frame->mcontextStorage + 0xf8) = frame;
    CONTEXT redirected = context;
    redirected.Rsp = top - 0x28;
    redirected.Rip = reinterpret_cast<DWORD64>(&DeliverExceptionThunk);
    redirected.Rbx = reinterpret_cast<DWORD64>(frame);
    const bool ok = SetThreadContext(native, &redirected) != 0;
    ResumeThread(native);

    if (ok) NudgeThread(thread);
    const unsigned injections = gcedInjected.fetch_add(1) + 1;
    if ((injections & 63) == 1)
        aps5::LogErr("[gced] inj n=%u h=%u skip=%u\n", injections, gcedHanded.load(), gcedSkipped.load());
    return ok ? 0 : static_cast<int>(0x80020003);
}

void APS5_VABI sceKernelDebugRaiseException(int c1, int c2) {
  APS5_LOG_OUT("sceKernelDebugRaiseException c1=%d c2=%d", c1, c2);
}

void APS5_VABI sceKernelDebugRaiseExceptionOnReleaseMode(int c1, int c2) {
  APS5_LOG_OUT("sceKernelDebugRaiseExceptionOnReleaseMode c1=%d c2=%d", c1, c2);
}

}
