#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_STATE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_STATE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include <vector>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include "Recompiler.hpp"

namespace AgcDriver::Graphics {

enum class ShaderPath {
    Vertex,
    Geometry,
    Tessellation,
    TessellationGeometry
};

struct ShaderStages {
    ShaderPath path;
    std::uint32_t registerValue;
    std::uint32_t vertexWaveSize;
    std::uint32_t fragmentWaveSize;
    std::optional<ShaderRecompiler::MeshConfiguration> mesh;
    std::optional<ShaderRecompiler::TessellationConfiguration> tessellation;
};

struct ColorTarget {
    std::uint64_t address;
    VkExtent2D extent;
    VkFormat format;
    std::size_t bytes;
    std::uint8_t componentMapping;
    ColorTileMode tileMode = ColorTileMode::Linear;
    std::uint32_t elementBytes = 4;

    std::uint64_t dccAddress = 0;
    bool dccAlphaOnMsb = false;
    std::uint64_t surfaceAddress = 0;
    VkExtent2D surfaceExtent{};
    std::uint32_t mipCount = 1;
    std::uint32_t mip = 0;
    bool mipTail = false;
    std::array<std::uint32_t, 2> clearWords{};
    std::uint32_t slot = 0;
};

struct DepthTarget {
    std::uint64_t address;
    std::uint64_t stencilAddress;
    VkExtent2D extent;
    VkFormat format;
    float clearDepth;
    std::uint8_t clearStencil;

    bool implicitStencil = false;
    std::uint64_t htileAddress = 0;
};

struct State {
    ShaderStages stages;
    std::optional<DepthTarget> depth;
    bool depthTest = false;
    bool depthWrite = false;
    VkCompareOp depthCompare = VK_COMPARE_OP_ALWAYS;
    bool depthBoundsTest = false;
    float minDepthBounds = 0.0f;
    float maxDepthBounds = 1.0f;
    bool depthBias = false;
    float depthBiasConstant = 0.0f;
    float depthBiasSlope = 0.0f;
    float depthBiasClamp = 0.0f;
    bool stencilTest = false;
    std::uint32_t rawDepthControl = 0, rawZInfo = 0, rawStencilInfo = 0, rawStencilControl = 0, rawStencilRef = 0;
    VkStencilOpState stencilFront{};
    VkStencilOpState stencilBack{};
    ColorTarget color;
    std::vector<ColorTarget> colors;
    std::vector<VkPipelineColorBlendAttachmentState> blends;
    bool hasColorTarget;
    bool rectList = false;
    VkExtent2D renderExtent;
    VkPrimitiveTopology topology;
    bool primitiveRestart = false;
    VkViewport viewport;
    bool negativeOneToOne;
    bool depthClamp = false;
    VkRect2D scissor;
    VkCullModeFlags cullMode;
    VkFrontFace frontFace;
    VkPipelineColorBlendAttachmentState blend;
    std::array<float, 4> blendConstants;
};

ShaderStages DecodeShaderStages(const QueueState& queue);
State DecodeState(const QueueState& queue);
std::array<std::uint8_t, 8> ExportMappings(const State& state);
ColorTarget DecodeColorBuffer(const Registers& context, std::uint32_t slot);

struct ColorMetadataPass {
    enum class Mode { EliminateFastClear, DccDecompress };
    Mode mode;
    std::vector<ColorTarget> targets;
};
std::optional<ColorMetadataPass> DecodeColorMetadataPass(const QueueState& queue);

std::string DrawRejection(const QueueState& queue, bool indexed);

enum class RegisterBank : std::uint8_t { Context, Shader, UserConfig, Count };
struct RegisterRead {
    RegisterBank bank;
    std::uint32_t offset;
};
std::vector<RegisterRead>*& RegisterReadLog();
inline void NoteRegisterRead(RegisterBank bank, std::uint32_t offset) {
    if (auto* log = RegisterReadLog()) log->push_back({bank, offset});
}
inline const char* RegisterBankName(RegisterBank bank) {
    return bank == RegisterBank::Context ? "context" : bank == RegisterBank::Shader ? "shader" : "user-config";
}

struct DrawKeyRange {
    RegisterBank bank;
    std::uint32_t first;
    std::uint32_t count;
};
inline constexpr std::array<DrawKeyRange, 46> DrawKeyRegisters{{
    {RegisterBank::Context, 0x000, 1}, {RegisterBank::Context, 0x002, 1}, {RegisterBank::Context, 0x005, 1}, {RegisterBank::Context, 0x007, 7}, {RegisterBank::Context, 0x010, 6}, {RegisterBank::Context, 0x01a, 5},
    {RegisterBank::Context, 0x080, 4}, {RegisterBank::Context, 0x08c, 4}, {RegisterBank::Context, 0x090, 2}, {RegisterBank::Context, 0x094, 2}, {RegisterBank::Context, 0x0b4, 2}, {RegisterBank::Context, 0x105, 4}, {RegisterBank::Context, 0x10b, 3}, {RegisterBank::Context, 0x10f, 6},

    {RegisterBank::Context, 0x191, 32}, {RegisterBank::Context, 0x1b3, 2}, {RegisterBank::Context, 0x1b6, 1}, {RegisterBank::Context, 0x1c3, 3}, {RegisterBank::Context, 0x1e0, 8}, {RegisterBank::Context, 0x1ff, 1},

    {RegisterBank::Context, 0x200, 8}, {RegisterBank::Context, 0x292, 2}, {RegisterBank::Context, 0x29b, 1}, {RegisterBank::Context, 0x2ab, 1}, {RegisterBank::Context, 0x2ce, 1}, {RegisterBank::Context, 0x2d5, 2}, {RegisterBank::Context, 0x2db, 2}, {RegisterBank::Context, 0x2de, 6}, {RegisterBank::Context, 0x2f8, 2}, {RegisterBank::Context, 0x30e, 2}, {RegisterBank::Context, 0x313, 1},

    {RegisterBank::Context, 0x318, 0x78}, {RegisterBank::Context, 0x390, 8}, {RegisterBank::Context, 0x3a8, 0x18},

    {RegisterBank::Shader, 0x008, 0x24}, {RegisterBank::Shader, 0x082, 2}, {RegisterBank::Shader, 0x088, 2}, {RegisterBank::Shader, 0x08a, 0x22}, {RegisterBank::Shader, 0x0c8, 2}, {RegisterBank::Shader, 0x102, 2}, {RegisterBank::Shader, 0x108, 2}, {RegisterBank::Shader, 0x10b, 0x21}, {RegisterBank::Shader, 0x148, 2},

    {RegisterBank::UserConfig, 0x242, 1}, {RegisterBank::UserConfig, 0x24b, 1}, {RegisterBank::UserConfig, 0x25b, 1},
}};

bool DrawKeyCovers(RegisterRead read);

}

#endif
