#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINE_HPP

#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include <set>

namespace AgcDriver::Graphics {

struct VertexInputLayout;

class Framebuffer {
public:
    Framebuffer(const Context& context, VkRenderPass renderPass, std::span<const VkImageView> targets, VkExtent2D extent);
    ~Framebuffer();
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    VkFramebuffer Handle() const { return framebuffer; }

    void Abandon() noexcept;

private:
    Context context;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
};

class Pipeline {
public:

    Pipeline(const Context& context, const State& state, const VertexInputLayout& vertexInput, const ShaderResources& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    ~Pipeline();
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    VkPipelineLayout Layout() const;
    std::shared_ptr<Framebuffer> AcquireFramebuffer(std::span<const VkImageView> targets, std::span<const std::shared_ptr<StorageTexture>> owners, VkExtent2D extent);

    void Begin(VkCommandBuffer commands, const Framebuffer& framebuffer, VkExtent2D extent, const State& state) const;

    void Continue(VkCommandBuffer commands, const State& state) const;
    void PushConstants(VkCommandBuffer commands, VkShaderStageFlags stages, std::span<const std::byte, PipelinePushConstantBytes> bytes) const;

    void Abandon() noexcept;

private:
    struct CachedFramebuffer {
        std::vector<VkImageView> views;
        std::vector<std::weak_ptr<StorageTexture>> owners;
        VkExtent2D extent;
        std::shared_ptr<Framebuffer> framebuffer;
    };
    void release() noexcept;
    Context context;
    std::vector<VkShaderModule> _modules;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    std::size_t attachments = 0;
    std::size_t colorAttachments = 0;
    bool depthBounds = false;
    bool depthBias = false;
    std::vector<CachedFramebuffer> framebuffers;
};

std::shared_ptr<Pipeline> CachedPipeline(const Context& context, const State& state, const VertexInputLayout& vertexInput, const ShaderResources& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

void ClearCachedPipelines(VkDevice device);

void ValidateViewport(const Context& context, const VkViewport& viewport);

void ValidateShaderPair(const ShaderRecompiler::RecompileResult& vertex, const ShaderRecompiler::RecompileResult& fragment);

std::set<std::uint32_t> ValidateShaders(std::span<const CompiledShader> shaders, const State& state, const VkPhysicalDeviceSubgroupProperties& subgroup, bool fragmentShaderBarycentric, bool descriptorIndexing = false);

}

#endif
