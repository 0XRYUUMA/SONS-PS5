#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTUREDETILER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTUREDETILER_HPP

#include <array>
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include <cstdint>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

    struct DetileWindow {
        std::uint32_t rangeBegin = 0;
        std::uint32_t rangeEnd = 0xffffffffu;
        std::uint32_t tiledBase = 0;
        std::uint32_t linearBase = 0;
        std::uint64_t linearBytes = 0;
        std::uint32_t columnBegin = 0;
        std::uint32_t columnEnd = 0;
        std::uint32_t rowBegin = 0;
        std::uint32_t rowEnd = 0;
    };

    class TextureDetiler {
    public:
        explicit TextureDetiler(const Context& context);
        ~TextureDetiler();
        TextureDetiler(const TextureDetiler&) = delete;
        TextureDetiler& operator=(const TextureDetiler&) = delete;

        void Dispatch(VkCommandBuffer commands, TextureTileMode tileMode, std::uint32_t elementBytes, VkBuffer source, std::uint64_t sourceOffset, VkBuffer destination, std::uint64_t destinationOffset, const TileMipLayout& layout, bool retile = false, std::uint32_t slice = 0, bool thick = false, const DetileWindow& window = {});

        void BeginBatch();

    private:
        VkPipeline pipeline(TextureTileMode tileMode, std::uint32_t elementBytes, bool retile, bool thick);
        void release() noexcept;
        VkDescriptorSet allocateSet();

        const Context context;
        VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkShaderModule module = VK_NULL_HANDLE;
        std::vector<std::pair<std::uint32_t, VkPipeline>> pipelines;
        std::vector<VkDescriptorPool> descriptorPools;
        std::size_t allocatedSets = 0;
    };

}

#endif
