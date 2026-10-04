#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERMEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERMEMORY_HPP

#include "Recompiler.hpp"
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace ShaderRecompiler {
struct SourceHandle;
}

namespace AgcDriver {

class ShaderMemory {
public:

    enum class PendingWrite : std::uint8_t { None, Sync, Raw, VerifyRaw, KnownValue, VerifyKnownValue };
    using PendingWriteQuery = PendingWrite (*)(std::uint64_t address, std::size_t bytes, std::span<std::byte> known);

    struct KnownValueCounts {
        std::uint64_t served, verified, mismatches;
    };
    static KnownValueCounts KnownValues();

    using PendingWriteObserver = void (*)(std::uint64_t address, bool unchanged);

    using HookWaitCounter = std::uint64_t (*)();

    using WaitedMsProvider = double (*)();
    static void SetWaitedMsProvider(WaitedMsProvider provider);
    explicit ShaderMemory(std::span<const ShaderRecompiler::MemoryRegion> initial, PendingWriteQuery pendingWrite = nullptr, PendingWriteObserver observe = nullptr, HookWaitCounter hookWaits = nullptr);

    std::shared_ptr<const ShaderRecompiler::ResourceCapture> Capture(const ShaderRecompiler::RecompileRequest& request, const ShaderRecompiler::SourceHandle* handle = nullptr);
    [[nodiscard]] std::vector<ShaderRecompiler::MemoryRegion> Regions() const;

    [[nodiscard]] std::vector<ShaderRecompiler::MemoryRegion> TakeRecentRegions();

    static void CountHandleMemo(bool hit);

private:
    static constexpr std::size_t PageBytes = 4096;
    static constexpr std::size_t PageWords = PageBytes / sizeof(std::uint32_t);

    struct Page {
        std::array<std::uint32_t, PageWords> words{};
        std::bitset<PageWords> valid;
        std::bitset<PageWords> read;
        std::bitset<PageWords> recent;
        bool wordwise = false;
    };

    static bool read(void* context, std::uint64_t address, std::uint32_t* value);
    Page& page(std::uint64_t base);

    std::map<std::uint64_t, std::span<const std::byte>> initial;
    std::map<std::uint64_t, Page> pages;

    struct PageMemo {
        std::uint64_t base = ~0ull;
        Page* page = nullptr;
        PageMemo() = default;
        PageMemo(const PageMemo&) {}
        PageMemo& operator=(const PageMemo&) {
            base = ~0ull;
            page = nullptr;
            return *this;
        }
    } lastPage;
    PendingWriteQuery pendingWrite = nullptr;
    PendingWriteObserver observe = nullptr;
    HookWaitCounter hookWaits = nullptr;
};

struct DataWordPositionCounts {
    std::uint64_t unmapped = 0;
    std::uint64_t mismatched = 0;
    std::uint64_t aliased = 0;
};
DataWordPositionCounts DataWordPositions(std::span<const std::pair<std::uint64_t, std::uint64_t>> runs, std::span<const std::pair<std::uint32_t, std::uint64_t>> leaves, std::span<const std::uint64_t> otherReads, std::span<const std::uint32_t> words, std::span<const std::uint32_t> flattenedSrt, std::vector<std::uint32_t>& positions, std::vector<std::uint32_t>& slots);

}

#endif
