#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BUFFERPOOL_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BUFFERPOOL_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace AgcDriver::Graphics {

struct BufferAllocation {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* mapping;
    VkDeviceAddress address;
    VkDeviceSize allocationBytes;

    std::size_t bytes;
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
};

class BufferPool {
public:
    explicit BufferPool(const Context& context);
    ~BufferPool();
    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;

    static std::size_t Capacity(std::size_t bytes);
    std::optional<BufferAllocation> Take(std::size_t bytes, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void Put(const BufferAllocation& allocation) noexcept;

private:
    struct Slot {
        BufferAllocation allocation;
        std::uint64_t lastUse;
    };

    struct Tier {
        std::vector<Slot> free;
        VkDeviceSize retainedBytes = 0;
        VkDeviceSize budget = 0;
        std::uint64_t hits = 0;
        std::uint64_t misses = 0;
        std::uint64_t evictions = 0;
    };

    Tier& tierFor(std::size_t capacity, VkMemoryPropertyFlags properties);

    static VkDeviceSize DeviceBudget();
    void destroy(const BufferAllocation& allocation) noexcept;

    void evictOldest(Tier& tier, std::vector<BufferAllocation>& evicted);

    static std::size_t MaxSlots();
    VkDevice device;
    PFN_vkUnmapMemory unmap;
    PFN_vkDestroyBuffer destroyBuffer;
    PFN_vkFreeMemory freeMemory;
    std::mutex mutex;

    Tier smallTier;
    Tier largeTier;
    Tier deviceTier;
    std::uint64_t clock = 0;
    static constexpr VkDeviceSize budget = 512ull * 1024 * 1024;

    static constexpr VkDeviceSize smallBudget = 64ull * 1024 * 1024;

    static constexpr std::size_t classLimit = std::size_t{1} << 20u;

    static constexpr std::size_t defaultSlots = 2048;
};

std::shared_ptr<BufferPool> GetBufferPool(const Context& context);

}

#endif
