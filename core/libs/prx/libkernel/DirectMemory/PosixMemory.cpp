#include "DirectMemory.hpp"
#include "prx/common/StderrLog.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <limits>
#include <new>
#include <stdexcept>

extern "C" int* APS5_VABI __error_nid_postfix();

namespace {

constexpr int GuestInvalid = 22;
constexpr int GuestNoMemory = 12;
constexpr int GuestNotSupported = 45;
constexpr int GuestPrivate = 0x2;
constexpr int GuestAnonymous = 0x1000;

bool RoundLength(std::size_t length, std::size_t& rounded) {
    constexpr auto mask = PS5_PAGE_SIZE - 1;
    if (length == 0 || length > std::numeric_limits<std::size_t>::max() - mask)
        return false;
    rounded = (length + mask) & ~mask;
    return true;
}

void SetError(int error) {
    *__error_nid_postfix() = error;
}
}

extern "C" {

void* APS5_VABI mmap_nid_postfix(void* address, std::size_t length, int protection,
                                int flags, int descriptor, std::int64_t offset) noexcept {
    const auto failed = [&](int error) -> void* {
        aps5::LogErr( "[KMEM] mmap FAILED addr=%p len=0x%zx prot=%d flags=0x%x fd=%d off=%lld error=%d\n", address, length, protection, flags, descriptor, static_cast<long long>(offset), error);
        SetError(error);
        return reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));
    };
    std::size_t rounded;
    if (!RoundLength(length, rounded) || (protection & ~7) != 0)
        return failed(GuestInvalid);

    constexpr int GuestFixed = 0x10;
    constexpr int GuestIgnored = 0x20000 | 0x800 | 0x400 | 0xff000000;
    if ((flags & (GuestPrivate | GuestAnonymous)) != (GuestPrivate | GuestAnonymous) || (flags & ~(GuestPrivate | GuestAnonymous | GuestFixed | GuestIgnored)) != 0)
        return failed(GuestNotSupported);
    if (descriptor != -1 || offset != 0)
        return failed(GuestInvalid);

    void* mapped = (flags & GuestFixed) != 0 ? address : nullptr;
    try {
        const auto result = DoMapAnon(&mapped, rounded, protection, (flags & GuestFixed) != 0 ? GuestFixed : 0);
        if (result != 0) return failed(GuestNoMemory);
        return mapped;
    } catch (const std::bad_alloc&) {
        return failed(GuestNoMemory);
    } catch (const std::exception&) {
        return failed(GuestNoMemory);
    }
}

int APS5_VABI munmap_nid_postfix(void* address, std::size_t length) noexcept {
    const auto failed = [](int error) {
        SetError(error);
        return -1;
    };
    std::size_t rounded;
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    if (!RoundLength(length, rounded) || start == 0 ||
        (start & (PS5_PAGE_SIZE - 1)) != 0 ||
        rounded > std::numeric_limits<std::uintptr_t>::max() - start)
        return failed(GuestInvalid);
    try {
        if (DoMunmap(address, rounded) != 0) return failed(GuestInvalid);
        return 0;
    } catch (const std::bad_alloc&) {
        return failed(GuestNoMemory);
    } catch (const std::exception&) {

        return failed(GuestInvalid);
    }
}

}
