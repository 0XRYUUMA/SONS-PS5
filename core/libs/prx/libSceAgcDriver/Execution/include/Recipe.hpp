#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_RECIPE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_RECIPE_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace AgcDriver {

struct ComputePipelineObjects;

struct Recipe {

    VkDevice device = VK_NULL_HANDLE;
    std::weak_ptr<Graphics::ShaderResources> templateRef;

    Graphics::ResourceCache::Key key;
    std::weak_ptr<ComputePipelineObjects> objects;
    std::array<std::byte, Graphics::PipelinePushConstantBytes> pushBytes{};
    bool pushes = false;

    std::uint64_t dataWordsHash = 0;

    std::vector<std::pair<std::uint64_t, std::uint64_t>> presyncSurfaces;
    Graphics::HostImportsProof presyncProof;

    bool needsCompletion = false;
    bool holdsLease = false;
};

enum class RecipeOutcome { Recorded, Rebuild };

struct DrawRecipe {
    VkDevice device = VK_NULL_HANDLE;
    std::weak_ptr<Graphics::ShaderResources> templateRef;
    Graphics::ResourceCache::Key key;
    std::weak_ptr<Graphics::Pipeline> pipeline;
    std::weak_ptr<Graphics::Framebuffer> framebuffer;
    std::vector<std::weak_ptr<Graphics::StorageTexture>> targets;
    std::vector<VkImageView> targetViews;
    std::uint64_t passKey = 0;
    Graphics::VertexInputLayout vertexInput;
    std::array<std::byte, Graphics::PipelinePushConstantBytes> pushBytes{};
    VkShaderStageFlags pushStages = 0;
    std::optional<Graphics::State> masked;
    std::set<std::uint32_t> fragmentOutputs;
    VkPipelineStageFlags shaderStages = 0;
};

}

#endif
