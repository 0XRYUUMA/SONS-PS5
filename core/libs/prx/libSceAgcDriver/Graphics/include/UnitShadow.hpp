#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_UNITSHADOW_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_UNITSHADOW_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

struct HostImport;

bool UnitShadowEnabled();

enum class PublishScope : std::uint8_t { None, Whole, PartialUnits };
enum class PublishReason : std::uint8_t { Hook, Region, Indirect, CopySource, CopyDestination, Fill, FillClear, Label, Keys, Scanout, Validate, Upload, TailMip, Evict, Retire, Teardown, Count };

PublishReason PublishReasonFor(const char* flushReason);

struct ShadowSlab {
    ShadowSlab(const Context& context, VkBuffer buffer, VkDeviceMemory memory, std::uint64_t firstUnit, std::uint32_t units);
    ShadowSlab(const ShadowSlab&) = delete;
    ShadowSlab& operator=(const ShadowSlab&) = delete;

    ~ShadowSlab();
    Context context;
    VkBuffer buffer;
    VkDeviceMemory memory;
    std::uint64_t firstUnit;
    std::uint32_t units;
    std::uint64_t lastUse = 0;

    std::atomic<std::uint32_t> pins{0};
};

struct ShadowSlabPin {
    explicit ShadowSlabPin(std::shared_ptr<ShadowSlab> slab);
    ShadowSlabPin(const ShadowSlabPin&) = delete;
    ShadowSlabPin& operator=(const ShadowSlabPin&) = delete;
    ~ShadowSlabPin();
    std::shared_ptr<ShadowSlab> slab;
};

struct ShadowedRange {
    std::uint64_t begin;
    std::uint64_t end;
    std::shared_ptr<ShadowSlab> slab;
};

std::size_t PublishShadow(std::uint64_t address, std::size_t bytes, PublishScope scope, PublishReason reason);

bool AnyShadowedOverlaps(std::uint64_t address, std::size_t bytes);
bool AnyShadowedOverlaps(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges);

struct ShadowRun {
    std::uint64_t begin;
    std::uint64_t end;
    VkBuffer buffer;
    VkDeviceSize offset;
    bool shadow;
    std::shared_ptr<ShadowSlab> slab;
};
std::vector<ShadowRun> ShadowSources(const Context& context, const HostImport& import, std::uint64_t surfaceBase, std::span<const std::pair<std::uint64_t, std::uint64_t>> runs, std::span<const std::pair<std::uint64_t, std::uint64_t>> tailBlocks);

struct ShadowDestination {
    VkBuffer buffer;
    VkDeviceSize offset;
    std::shared_ptr<ShadowSlab> slab;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> seedUnits;
    std::shared_ptr<ShadowSlabPin> pin;
};
std::optional<ShadowDestination> ShadowDestinationFor(const Context& context, const HostImport& import, std::uint64_t begin, std::uint64_t end);

void NoteShadowSeed(std::uint64_t begin, std::uint64_t end);

std::uint64_t SlabBoundary(const HostImport& import, std::uint64_t address);

VkDeviceSize SlabOffset(const HostImport& import, const ShadowSlab& slab, std::uint64_t address);

void MarkShadowed(const HostImport& import, std::span<const ShadowedRange> ranges, std::uint64_t generation);

void RetireShadow(const Context& context, const HostImport& import, const std::function<bool(std::uint64_t, std::uint64_t)>& registered);

void PublishAllShadows(const Context& context, PublishReason reason);
void DestroyShadows(VkDevice device);

std::string ShadowReport();

bool ShadowVerify();

}

#endif
