#include "prx/libSceAgcDriver/Eq/include/Query.hpp"
#include "prx/common/StderrLog.hpp"

#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

bool TraceEq() {
    static const bool value = std::getenv("APS5_TRACE_SEMA") != nullptr;
    return value;
}

void TraceEqEvent(const char* what, const KernelEvent* ev) {
    if (!TraceEq()) return;
    static int printed = 0;
    if (++printed > 24) return;
    aps5::LogErr( "[eq] %s ident=0x%llx filter=%d flags=0x%x fflags=0x%x data=0x%llx udata=%p\n", what,
                 static_cast<unsigned long long>(ev->ident), static_cast<int>(ev->filter), ev->flags, ev->fflags,
                 static_cast<unsigned long long>(ev->data), ev->udata);
}

}

extern "C" {

uint32_t APS5_VABI sceAgcDriverGetEqContextId(const KernelEvent* ev) {
 if (ev == nullptr) {
  return 0;
 }
 TraceEqEvent("context id", ev);
 return 0;
}

int APS5_VABI sceAgcDriverGetEqEventType(const KernelEvent* ev) {
    if (ev == nullptr || reinterpret_cast<std::uintptr_t>(ev) % alignof(KernelEvent) != 0) {
        throw std::runtime_error(std::string(__func__) + ": null or misaligned event");
    }
    if (ev->filter == -14) {
        if (ev->ident > static_cast<std::uintptr_t>(std::numeric_limits<int>::max())) {
            throw std::runtime_error(std::string(__func__) + ": graphics event type overflow");
        }
        return static_cast<int>(ev->ident);
    }
    if (ev->data < std::numeric_limits<int>::min() || ev->data > std::numeric_limits<int>::max()) {
        throw std::runtime_error(std::string(__func__) + ": event type overflow");
    }
    return static_cast<int>(ev->data);
}

}
