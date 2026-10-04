#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SAMPLER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SAMPLER_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestSamplerResource.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <span>

namespace AgcDriver::Graphics {

class Sampler {
public:
    Sampler(const Context& context, const GuestSamplerResource& descriptor);
    ~Sampler();
    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;

    VkSampler Handle() const;

private:
    void release() noexcept;

    Context context;
    VkSampler sampler = VK_NULL_HANDLE;
};

class SamplerCache {
public:
    explicit SamplerCache(std::size_t capacity = 1024);
    SamplerCache(const SamplerCache&) = delete;
    SamplerCache& operator=(const SamplerCache&) = delete;
    std::shared_ptr<Sampler> Get(const Context& context, std::span<const std::uint32_t> words, bool compareEnable);

    std::uint64_t Hits() const { return hits; }
    std::uint64_t Misses() const { return misses; }

private:
    struct Entry {
        std::shared_ptr<Sampler> sampler;
        std::uint64_t lastUse;
    };
    std::mutex mutex;
    std::map<std::array<std::uint32_t, 5>, Entry> entries;
    std::uint64_t clock = 0;
    std::size_t capacity;
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
};

}

#endif
