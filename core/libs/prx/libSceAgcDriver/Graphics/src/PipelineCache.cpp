#include "prx/libSceAgcDriver/Graphics/include/PipelineCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineWarmup.hpp"
#include "prx/common/StderrLog.hpp"
#include "ShaderCacheDirectory.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace AgcDriver::Graphics {

namespace {

constexpr std::uint32_t FileMagic = 0x50565041u;
constexpr std::uint32_t FileFormat = 1;

struct FileHeader {
    std::uint32_t magic;
    std::uint32_t format;
    std::uint64_t dataBytes;
    std::uint64_t dataHash;
};
static_assert(sizeof(FileHeader) == 24);

bool profiling() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return profile;
}

}

PipelineCache::PipelineCache(const Context& context, const VkPhysicalDeviceProperties& properties) : context(context), properties(properties) {
    const auto directory = ShaderRecompiler::ShaderCacheDirectory();
    if (!directory.empty()) {
        std::string uuid;
        for (const auto byte : properties.pipelineCacheUUID) {
            char digits[3];
            std::snprintf(digits, sizeof(digits), "%02x", static_cast<unsigned>(byte));
            uuid += digits;
        }
        char name[160];
        std::snprintf(name, sizeof(name), "vk-pipelines-%04x-%04x-%08x-%s.bin", properties.vendorID, properties.deviceID, properties.driverVersion, uuid.c_str());
        path = directory / name;
    }
    std::vector<std::byte> initialData;
    if (!path.empty()) load(initialData);
    VkPipelineCacheCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    info.initialDataSize = initialData.size();
    info.pInitialData = initialData.empty() ? nullptr : initialData.data();
    const auto create = context.Function<PFN_vkCreatePipelineCache>("vkCreatePipelineCache");
    VkResult result = create(context.device, &info, nullptr, &cache);
    if (result != VK_SUCCESS && !initialData.empty()) {
        aps5::LogErr( "[pipeline-cache] the driver refused %s (Vulkan result %d); starting empty\n", path.string().c_str(), static_cast<int>(result));
        info.initialDataSize = 0;
        info.pInitialData = nullptr;
        savedBytes = 0;
        result = create(context.device, &info, nullptr, &cache);
    }
    Check(result, "vkCreatePipelineCache");
    PipelineWarmup::Configure(properties);
    if (std::getenv("APS5_PRECOMPILE_PIPELINES") != nullptr) {
        const auto started = std::chrono::steady_clock::now();
        const auto [warmed, failed] = PipelineWarmup::Warm(context, cache);
        aps5::LogErr("[precompile] rebuilt %zu known graphics pipelines, %zu failed in %.1f s\n",
                     warmed, failed, std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
    }
    if (!path.empty()) saver = std::thread([this] { run(); });
}

PipelineCache::~PipelineCache() {
    if (saver.joinable()) {
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        wake.notify_all();
        saver.join();
        save(true);
    }
    context.Function<PFN_vkDestroyPipelineCache>("vkDestroyPipelineCache")(context.device, cache, nullptr);
}

void PipelineCache::load(std::vector<std::byte>& initialData) {
    std::vector<std::byte> file;
    if (!ShaderRecompiler::ReadWholeFile(path, file)) return;
    const auto reject = [&](const char* why) {
        aps5::LogErr( "[pipeline-cache] ignoring %s: %s\n", path.string().c_str(), why);
    };
    FileHeader header{};
    if (file.size() < sizeof(header)) return reject("truncated");
    std::memcpy(&header, file.data(), sizeof(header));
    if (header.magic != FileMagic || header.format != FileFormat) return reject("another format");
    if (header.dataBytes != file.size() - sizeof(header)) return reject("truncated");
    const auto data = std::span(file).subspan(sizeof(header));
    if (ShaderRecompiler::HashBytes(data) != header.dataHash) return reject("checksum mismatch");
    struct VulkanHeader {
        std::uint32_t headerSize;
        std::uint32_t headerVersion;
        std::uint32_t vendorID;
        std::uint32_t deviceID;
        std::uint8_t uuid[VK_UUID_SIZE];
    } vulkan{};
    static_assert(sizeof(VulkanHeader) == 32);
    if (data.size() < sizeof(vulkan)) return reject("no Vulkan header");
    std::memcpy(&vulkan, data.data(), sizeof(vulkan));
    if (vulkan.headerSize < sizeof(vulkan) || vulkan.headerVersion != VK_PIPELINE_CACHE_HEADER_VERSION_ONE || vulkan.vendorID != properties.vendorID || vulkan.deviceID != properties.deviceID || std::memcmp(vulkan.uuid, properties.pipelineCacheUUID, VK_UUID_SIZE) != 0) return reject("made for another device or driver");
    initialData.assign(data.begin(), data.end());
    savedBytes = initialData.size();
    savedHash = header.dataHash;
    aps5::LogErr( "[pipeline-cache] loaded %.1f KiB from %s\n", static_cast<double>(initialData.size()) / 1024.0, path.string().c_str());
}

void PipelineCache::save(bool final) {
    const auto getData = context.Function<PFN_vkGetPipelineCacheData>("vkGetPipelineCacheData");
    std::size_t size = 0;
    if (getData(context.device, cache, &size, nullptr) != VK_SUCCESS || size == 0) return;
    std::vector<std::byte> file(sizeof(FileHeader) + size);
    const auto result = getData(context.device, cache, &size, file.data() + sizeof(FileHeader));
    if (result != VK_SUCCESS) return;
    file.resize(sizeof(FileHeader) + size);
    const auto hash = ShaderRecompiler::HashBytes(std::span(file).subspan(sizeof(FileHeader)));
    if (size == savedBytes && hash == savedHash) return;
    const FileHeader header{FileMagic, FileFormat, size, hash};
    std::memcpy(file.data(), &header, sizeof(header));
    const auto started = std::chrono::steady_clock::now();
    if (!ShaderRecompiler::WriteFileAtomically(path, file)) {
        aps5::LogErr( "[pipeline-cache] cannot write %s\n", path.string().c_str());
        return;
    }
    savedBytes = size;
    savedHash = hash;
    if (final || profiling()) aps5::LogErr( "[pipeline-cache] saved %.1f KiB to %s in %.1f ms%s\n", static_cast<double>(size) / 1024.0, path.string().c_str(), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count(), final ? " (teardown)" : "");
}

void PipelineCache::run() {
    std::unique_lock lock(mutex);
    while (!wake.wait_for(lock, std::chrono::seconds(SaveIntervalSeconds), [&] { return stopping; })) {
        lock.unlock();
        save(false);
        lock.lock();
    }
}

}
