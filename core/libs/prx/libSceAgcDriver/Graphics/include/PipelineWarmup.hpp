#pragma once

#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "ShaderCacheDirectory.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics::PipelineWarmup {

// Pipeline recipes are internal, versioned recordings of actual Vulkan creates.
// Never interpret the driver's opaque VkPipelineCache as a recipe.
inline constexpr std::uint32_t Magic = 0x31575041u;
inline constexpr std::uint32_t Version = 1;
inline constexpr std::size_t MaxFileBytes = 8u * 1024u * 1024u;
inline constexpr std::size_t MaxRecipes = 1024;

struct Writer {
    std::vector<std::byte> bytes;
    template<class T> void value(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>);
        const auto data = std::as_bytes(std::span(&value, 1));
        bytes.insert(bytes.end(), data.begin(), data.end());
    }
    template<class T> void list(std::span<const T> values) {
        const auto count = static_cast<std::uint32_t>(values.size());
        value(count);
        const auto data = std::as_bytes(values);
        bytes.insert(bytes.end(), data.begin(), data.end());
    }
};

struct Reader {
    std::span<const std::byte> bytes;
    template<class T> bool value(T& out) {
        static_assert(std::is_trivially_copyable_v<T>);
        if (bytes.size() < sizeof(T)) return false;
        std::memcpy(&out, bytes.data(), sizeof(T));
        bytes = bytes.subspan(sizeof(T));
        return true;
    }
    template<class T> bool list(std::vector<T>& out, std::size_t maximum) {
        std::uint32_t count = 0;
        if (!value(count) || count > maximum || count > bytes.size() / sizeof(T)) return false;
        out.resize(count);
        if (count) std::memcpy(out.data(), bytes.data(), count * sizeof(T));
        bytes = bytes.subspan(count * sizeof(T));
        return true;
    }
};

struct Stage {
    VkShaderStageFlagBits stage{};
    std::vector<std::uint32_t> spirv;
};

struct Recipe {
    std::vector<VkDescriptorSetLayoutBinding> descriptors;
    VkShaderStageFlags pushStages = 0;
    std::uint32_t pushBytes = 0;
    std::vector<Stage> stages;
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> colors;
    std::optional<VkAttachmentReference> depth;
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
    std::vector<VkPipelineColorBlendAttachmentState> blends;
    std::vector<VkDynamicState> dynamics;
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    std::uint32_t patchPoints = 0;
    bool negativeOneToOne = false;
    std::uint32_t colorSlots = 0;
};

inline std::filesystem::path& directory() {
    static std::filesystem::path path;
    return path;
}
inline std::mutex& mutex() {
    static std::mutex lock;
    return lock;
}
inline void Configure(const VkPhysicalDeviceProperties& properties) {
    const auto root = ShaderRecompiler::ShaderCacheDirectory();
    if (root.empty()) return;
    char name[100]{};
    char uuid[VK_UUID_SIZE * 2 + 1]{};
    for (std::size_t i = 0; i < VK_UUID_SIZE; ++i)
        std::snprintf(uuid + i * 2, 3, "%02x", properties.pipelineCacheUUID[i]);
    std::snprintf(name, sizeof(name), "vk-recipes-v1-%04x-%04x-%08x-%s", properties.vendorID,
                  properties.deviceID, properties.driverVersion, uuid);
    std::lock_guard lock(mutex());
    directory() = root / name;
}

inline bool Encode(const Recipe& recipe, std::vector<std::byte>& out) {
    Writer writer;
    writer.value(Magic);
    writer.value(Version);
    writer.list(std::span(recipe.descriptors));
    writer.value(recipe.pushStages);
    writer.value(recipe.pushBytes);
    writer.value(static_cast<std::uint32_t>(recipe.stages.size()));
    for (const auto& stage : recipe.stages) {
        writer.value(stage.stage);
        writer.list(std::span(stage.spirv));
    }
    writer.list(std::span(recipe.attachments));
    writer.list(std::span(recipe.colors));
    writer.value(recipe.depth.has_value());
    if (recipe.depth) writer.value(*recipe.depth);
    writer.list(std::span(recipe.bindings));
    writer.list(std::span(recipe.attributes));
    writer.list(std::span(recipe.blends));
    writer.list(std::span(recipe.dynamics));
    writer.value(recipe.assembly);
    writer.value(recipe.raster);
    writer.value(recipe.samples);
    writer.value(recipe.depthStencil);
    writer.value(recipe.blend);
    writer.value(recipe.patchPoints);
    writer.value(recipe.negativeOneToOne);
    writer.value(recipe.colorSlots);
    if (writer.bytes.size() > MaxFileBytes) return false;
    out = std::move(writer.bytes);
    return true;
}

inline bool Decode(std::span<const std::byte> data, Recipe& recipe) {
    Reader reader{data};
    std::uint32_t magic = 0, version = 0, count = 0;
    if (!reader.value(magic) || !reader.value(version) || magic != Magic || version != Version ||
        !reader.list(recipe.descriptors, 128) || !reader.value(recipe.pushStages) ||
        !reader.value(recipe.pushBytes) || !reader.value(count) || count > 5) return false;
    recipe.stages.resize(count);
    for (auto& stage : recipe.stages)
        if (!reader.value(stage.stage) || !reader.list(stage.spirv, 1024 * 1024) || stage.spirv.empty()) return false;
    bool hasDepth = false;
    if (!reader.list(recipe.attachments, 16) || !reader.list(recipe.colors, 16) ||
        !reader.value(hasDepth)) return false;
    if (hasDepth) {
        recipe.depth.emplace();
        if (!reader.value(*recipe.depth)) return false;
    }
    if (!reader.list(recipe.bindings, 64) || !reader.list(recipe.attributes, 64) ||
        !reader.list(recipe.blends, 16) || !reader.list(recipe.dynamics, 16) ||
        !reader.value(recipe.assembly) || !reader.value(recipe.raster) ||
        !reader.value(recipe.samples) || !reader.value(recipe.depthStencil) ||
        !reader.value(recipe.blend) || !reader.value(recipe.patchPoints) ||
        !reader.value(recipe.negativeOneToOne) || !reader.value(recipe.colorSlots) ||
        !reader.bytes.empty()) return false;
    // Saved structs must not contain pointers. Refuse malformed/untrusted files.
    if (recipe.assembly.pNext || recipe.raster.pNext || recipe.samples.pNext || recipe.samples.pSampleMask ||
        recipe.depthStencil.pNext || recipe.blend.pNext || recipe.blend.pAttachments ||
        recipe.descriptors.size() > 128 || recipe.colorSlots != recipe.colors.size()) return false;
    for (const auto& binding : recipe.descriptors) if (binding.pImmutableSamplers) return false;
    return true;
}

inline void Record(const ShaderResources& resources, std::span<const CompiledShader> shaders,
                   const VkRenderPassCreateInfo& pass, const VkPipelineLayoutCreateInfo& layout,
                   const VkGraphicsPipelineCreateInfo& pipeline) {
    std::lock_guard lock(mutex());
    if (directory().empty() || pipeline.stageCount != shaders.size() || shaders.empty() ||
        pipeline.pTessellationState || pipeline.pVertexInputState == nullptr ||
        pipeline.pViewportState == nullptr ||
        (pipeline.pViewportState->pNext != nullptr &&
         static_cast<const VkBaseInStructure*>(pipeline.pViewportState->pNext)->sType !=
             VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT)) return;
    const auto& key = resources.LayoutKey();
    if (key.size() % 4) return;
    Recipe recipe;
    for (std::size_t i = 0; i < key.size(); i += 4)
        recipe.descriptors.push_back({key[i], static_cast<VkDescriptorType>(key[i + 1]), key[i + 2], key[i + 3], nullptr});
    if (layout.pushConstantRangeCount > 1) return;
    recipe.pushStages = layout.pushConstantRangeCount ? layout.pPushConstantRanges[0].stageFlags : 0;
    recipe.pushBytes = layout.pushConstantRangeCount ? layout.pPushConstantRanges[0].size : 0;
    for (std::uint32_t i = 0; i < pipeline.stageCount; ++i) {
        const auto& source = shaders[i].program->spirv;
        recipe.stages.push_back({pipeline.pStages[i].stage, source});
    }
    if (pass.attachmentCount)
        recipe.attachments.assign(pass.pAttachments, pass.pAttachments + pass.attachmentCount);
    const auto& subpass = pass.pSubpasses[0];
    if (subpass.colorAttachmentCount)
        recipe.colors.assign(subpass.pColorAttachments, subpass.pColorAttachments + subpass.colorAttachmentCount);
    if (subpass.pDepthStencilAttachment) recipe.depth = *subpass.pDepthStencilAttachment;
    const auto& input = *pipeline.pVertexInputState;
    if (input.vertexBindingDescriptionCount)
        recipe.bindings.assign(input.pVertexBindingDescriptions, input.pVertexBindingDescriptions + input.vertexBindingDescriptionCount);
    if (input.vertexAttributeDescriptionCount)
        recipe.attributes.assign(input.pVertexAttributeDescriptions, input.pVertexAttributeDescriptions + input.vertexAttributeDescriptionCount);
    const auto& color = *pipeline.pColorBlendState;
    if (color.attachmentCount) recipe.blends.assign(color.pAttachments, color.pAttachments + color.attachmentCount);
    const auto& dynamic = *pipeline.pDynamicState;
    recipe.dynamics.assign(dynamic.pDynamicStates, dynamic.pDynamicStates + dynamic.dynamicStateCount);
    recipe.assembly = *pipeline.pInputAssemblyState;
    recipe.assembly.pNext = nullptr;
    recipe.raster = *pipeline.pRasterizationState;
    recipe.raster.pNext = nullptr;
    recipe.samples = *pipeline.pMultisampleState;
    recipe.samples.pNext = nullptr;
    recipe.samples.pSampleMask = nullptr;
    if (pipeline.pDepthStencilState) {
        recipe.depthStencil = *pipeline.pDepthStencilState;
        recipe.depthStencil.pNext = nullptr;
    }
    recipe.blend = color;
    recipe.blend.pNext = nullptr;
    recipe.blend.pAttachments = nullptr;
    recipe.patchPoints = 0;
    recipe.negativeOneToOne = pipeline.pViewportState->pNext != nullptr;
    recipe.colorSlots = subpass.colorAttachmentCount;
    std::vector<std::byte> encoded;
    if (!Encode(recipe, encoded)) return;
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto byte : encoded) hash = (hash ^ static_cast<std::uint8_t>(byte)) * 1099511628211ull;
    char filename[32]{};
    std::snprintf(filename, sizeof(filename), "%016llx.bin", static_cast<unsigned long long>(hash));
    std::error_code error;
    if (std::filesystem::exists(directory() / filename, error)) return;
    // Bound disk use and startup duration. Additional recipes can be supported later.
    std::size_t files = 0;
    for (std::filesystem::directory_iterator it(directory(), error), end; !error && it != end; it.increment(error))
        if (++files >= MaxRecipes) return;
    ShaderRecompiler::WriteFileAtomically(directory() / filename, encoded);
}

inline std::pair<std::size_t, std::size_t> Warm(const Context& context, VkPipelineCache cache) {
    std::filesystem::path root;
    { std::lock_guard lock(mutex()); root = directory(); }
    if (root.empty() || cache == VK_NULL_HANDLE) return {0, 0};
    std::error_code error;
    std::size_t warmed = 0, failed = 0, seen = 0;
    for (std::filesystem::directory_iterator it(root, error), end; !error && it != end && seen < MaxRecipes; it.increment(error)) {
        if (it->path().extension() != ".bin") continue;
        ++seen;
        if (it->file_size(error) > MaxFileBytes || error) { ++failed; error.clear(); continue; }
        std::vector<std::byte> data;
        Recipe recipe;
        if (!ShaderRecompiler::ReadWholeFile(it->path(), data) || data.size() > MaxFileBytes || !Decode(data, recipe)) { ++failed; continue; }
        auto createModule = context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule");
        auto destroyModule = context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule");
        auto createSet = context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout");
        auto destroySet = context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout");
        auto createLayout = context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout");
        auto destroyLayout = context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout");
        auto createPass = context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass");
        auto destroyPass = context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass");
        VkDescriptorSetLayout set = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkRenderPass pass = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        std::vector<VkShaderModule> modules;
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        bool ok = true;
        VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        setInfo.bindingCount = static_cast<std::uint32_t>(recipe.descriptors.size());
        setInfo.pBindings = recipe.descriptors.data();
        ok = createSet(context.device, &setInfo, nullptr, &set) == VK_SUCCESS;
        const VkPushConstantRange push{recipe.pushStages, 0, recipe.pushBytes};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &set;
        layoutInfo.pushConstantRangeCount = recipe.pushBytes ? 1 : 0;
        layoutInfo.pPushConstantRanges = recipe.pushBytes ? &push : nullptr;
        if (ok) ok = createLayout(context.device, &layoutInfo, nullptr, &layout) == VK_SUCCESS;
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = recipe.colorSlots;
        subpass.pColorAttachments = recipe.colors.data();
        if (recipe.depth) subpass.pDepthStencilAttachment = &*recipe.depth;
        VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        passInfo.attachmentCount = static_cast<std::uint32_t>(recipe.attachments.size());
        passInfo.pAttachments = recipe.attachments.data();
        passInfo.subpassCount = 1;
        passInfo.pSubpasses = &subpass;
        if (ok) ok = createPass(context.device, &passInfo, nullptr, &pass) == VK_SUCCESS;
        for (const auto& stage : recipe.stages) {
            if (!ok) break;
            VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            moduleInfo.codeSize = stage.spirv.size() * sizeof(std::uint32_t);
            moduleInfo.pCode = stage.spirv.data();
            VkShaderModule module = VK_NULL_HANDLE;
            ok = createModule(context.device, &moduleInfo, nullptr, &module) == VK_SUCCESS;
            if (ok) {
                modules.push_back(module);
                VkPipelineShaderStageCreateInfo stageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
                stageInfo.stage = stage.stage;
                stageInfo.module = module;
                stageInfo.pName = "main";
                stages.push_back(stageInfo);
            }
        }
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        input.vertexBindingDescriptionCount = static_cast<std::uint32_t>(recipe.bindings.size());
        input.pVertexBindingDescriptions = recipe.bindings.data();
        input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(recipe.attributes.size());
        input.pVertexAttributeDescriptions = recipe.attributes.data();
        VkPipelineViewportDepthClipControlCreateInfoEXT clip{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT};
        clip.negativeOneToOne = recipe.negativeOneToOne;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;
        if (recipe.negativeOneToOne) viewport.pNext = &clip;
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = static_cast<std::uint32_t>(recipe.dynamics.size());
        dynamic.pDynamicStates = recipe.dynamics.data();
        recipe.blend.attachmentCount = static_cast<std::uint32_t>(recipe.blends.size());
        recipe.blend.pAttachments = recipe.blends.data();
        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.stageCount = static_cast<std::uint32_t>(stages.size());
        info.pStages = stages.data();
        info.pVertexInputState = &input;
        info.pInputAssemblyState = &recipe.assembly;
        info.pViewportState = &viewport;
        info.pRasterizationState = &recipe.raster;
        info.pMultisampleState = &recipe.samples;
        info.pDepthStencilState = recipe.depth ? &recipe.depthStencil : nullptr;
        info.pColorBlendState = &recipe.blend;
        info.pDynamicState = &dynamic;
        info.layout = layout;
        info.renderPass = pass;
        if (ok) ok = context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(
            context.device, cache, 1, &info, nullptr, &pipeline) == VK_SUCCESS;
        if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        for (const auto module : modules) destroyModule(context.device, module, nullptr);
        if (pass) destroyPass(context.device, pass, nullptr);
        if (layout) destroyLayout(context.device, layout, nullptr);
        if (set) destroySet(context.device, set, nullptr);
        if (ok) ++warmed; else ++failed;
    }
    return {warmed, failed};
}

} // namespace AgcDriver::Graphics::PipelineWarmup
