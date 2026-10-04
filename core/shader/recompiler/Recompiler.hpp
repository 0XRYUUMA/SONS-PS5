#ifndef CORE_SHADER_RECOMPILIER_INCLUDE_SHADER_RECOMPILIER_RECOMPILER_HPP
#define CORE_SHADER_RECOMPILIER_INCLUDE_SHADER_RECOMPILIER_RECOMPILER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace ShaderRecompiler {

enum class ShaderStage {
    Compute,
    Vertex,
    TessellationControl,
    TessellationEvaluation,
    Geometry,
    Fragment,
    Local,
    Mesh
};

struct MemoryRegion {
    std::uint64_t guestAddress;
    std::span<const std::byte> bytes;
};

struct ShaderBinary {
    ShaderStage stage;
    std::uint64_t codeAddress;
    std::span<const std::uint32_t> code;
    std::uint64_t headerAddress;
    std::span<const std::byte> header;
};

struct ShaderComputeStageInfo {
    std::array<std::uint32_t, 3> numThreads;
    std::uint32_t ldsSizeDwords;
    std::array<bool, 3> groupIdEnable;
    bool tgSizeEnable;
    std::uint32_t threadIdComponentCount;
    std::array<std::uint32_t, 3> partialThreads;

    [[nodiscard]] bool PartialGroups() const {
        return partialThreads != std::array<std::uint32_t, 3>{};
    }
};

enum class PixelInput : std::uint32_t {
    PerspectiveSample,
    PerspectiveCenter,
    PerspectiveCentroid,
    PerspectivePullModel,
    LinearSample,
    LinearCenter,
    LinearCentroid,
    LineStipple,
    PositionX,
    PositionY,
    PositionZ,
    PositionW,
    FrontFace,
    Ancillary,
    SampleCoverage,
    PositionFixedPoint,
    Count
};

constexpr std::uint32_t PixelInputBit(PixelInput input) {
    return 1u << static_cast<std::uint32_t>(input);
}

constexpr std::uint32_t PixelInputVgprCount(PixelInput input) {
    switch (input) {
    case PixelInput::PerspectiveSample:
    case PixelInput::PerspectiveCenter:
    case PixelInput::PerspectiveCentroid:
    case PixelInput::LinearSample:
    case PixelInput::LinearCenter:
    case PixelInput::LinearCentroid:
        return 2u;
    case PixelInput::PerspectivePullModel:
        return 3u;
    default:
        return 1u;
    }
}

constexpr std::uint32_t PixelInputVgpr(std::uint32_t inputAddr, PixelInput input) {
    std::uint32_t vgpr = 0;
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(input); ++i) {
        if ((inputAddr & (1u << i)) != 0u) vgpr += PixelInputVgprCount(static_cast<PixelInput>(i));
    }
    return vgpr;
}

struct ShaderPixelStageInfo {
    std::uint32_t interpolatorCount;
    std::array<std::uint32_t, 32> interpolatorSettings;
    bool wave32;
    std::uint32_t inputAddr;
    bool hasPerspectiveCenterVgpr;
    bool perspectiveCentroid;
    bool posX;
    bool posY;
    bool posZ;
    bool posW;
    bool frontFace;
    bool ancillary;
    bool sampleShading;
    bool noPerspective;
    bool linearCentroid;
    bool pixelKillEnable;
    bool depthExportEnable;
    bool sampleMaskExportEnable;
    bool earlyZ;
    bool executeOnNoop;
    std::array<std::uint8_t, 8> targetOutputMode;
    std::array<std::uint8_t, 8> targetExportMapping;
};

struct ShaderVertexBufferResource {
    std::array<std::uint32_t, 4> fields;
};

struct ShaderVertexResourceDestination {
    std::int32_t registerStart;
    std::int32_t registersNum;
    std::int32_t attrId;
    std::uint32_t fetchIndex;
};

struct ShaderVertexStageInfo {
    static constexpr std::uint32_t MaxResources = 32;
    std::array<ShaderVertexBufferResource, MaxResources> resources;
    std::array<ShaderVertexResourceDestination, MaxResources> resourcesDst;
    std::uint32_t resourcesNum;
    std::uint32_t fetchAttribReg;
    std::uint32_t fetchBufferReg;
    bool fetchEmbedded;
};

struct GuestContext {
    std::uint32_t waveSize;
    std::uint32_t userDataBaseRegister;
    std::span<const std::uint32_t> userData;
    std::optional<ShaderComputeStageInfo> compute;
    std::optional<ShaderPixelStageInfo> pixel;
    std::optional<ShaderVertexStageInfo> vertex;
    std::span<const MemoryRegion> memory;
};

struct MeshTargetLimits {
    std::array<std::uint32_t, 3> maxWorkgroupSize;
    std::uint32_t maxWorkgroupInvocations;
    std::uint32_t maxSharedMemoryBytes;
    std::uint32_t maxOutputVertices;
    std::uint32_t maxOutputPrimitives;
    std::uint32_t maxOutputComponents;
    std::uint32_t maxOutputMemoryBytes;
    std::uint32_t outputPerVertexGranularity;
    std::uint32_t outputPerPrimitiveGranularity;
};

struct TessellationTargetLimits {
    std::uint32_t maxPatchSize;
    std::uint32_t maxControlPerVertexInputComponents;
    std::uint32_t maxControlPerVertexOutputComponents;
    std::uint32_t maxControlPerPatchOutputComponents;
    std::uint32_t maxControlTotalOutputComponents;
    std::uint32_t maxEvaluationInputComponents;
    std::uint32_t maxEvaluationOutputComponents;
};

struct SpirvTarget {
    std::uint32_t vulkanVersion;
    std::uint32_t spirvVersion;
    std::uint32_t subgroupSize;
    std::uint32_t bdaAbiVersion;
    std::span<const std::uint32_t> supportedCapabilities;
    std::span<const std::string_view> supportedExtensions;
    bool fragmentShaderBarycentricEnabled;
    std::array<std::uint32_t, 3> maxWorkgroupSize;
    std::uint32_t maxWorkgroupInvocations;
    std::uint32_t maxWorkgroupSharedMemoryBytes;
    std::optional<MeshTargetLimits> mesh;
    std::optional<TessellationTargetLimits> tessellation;
    bool nonConstantImageOffsets = false;
};

struct BindingLayout {
    std::uint32_t descriptorSet;
    std::uint32_t firstBinding;
    std::uint32_t pushConstantOffsetBytes;
    std::uint32_t pushConstantSizeBytes;
};

enum class ProgramRole {
    Main,
    GeometryBack,
    Local,
    Hull,
    Domain,
    Fragment
};

struct LinkedProgram {
    ProgramRole role;
    ShaderBinary binary;
    std::uint32_t userDataBaseRegister;
    std::uint32_t firstUserSgpr;
    std::span<const std::uint32_t> userData;
};

struct MeshConfiguration {
    std::uint32_t inputPrimitive;
    std::uint32_t primitivesPerGroup;
    std::uint32_t verticesPerGroup;
    std::uint32_t maxVertices;
    std::uint32_t maxPrimitives;
    std::uint32_t threadsPerGroup;
    std::uint32_t ldsSizeDwords;
    std::uint32_t provokingVertex;
    std::uint32_t esgsItemSize = 0;
};

struct TessellationConfiguration {
    std::uint32_t inputControlPoints;
    std::uint32_t outputControlPoints;
    std::uint32_t domain;
    std::uint32_t partitioning;
    std::uint32_t outputTopology;
};

inline constexpr std::uint32_t MeshDrawPushOffsetBytes = 104;
inline constexpr std::uint32_t MeshDrawPushBytes = 24;
inline constexpr std::uint32_t MeshIndexBufferUserWord = 4;

struct GraphicsDrawParameters {
    std::uint64_t indexAddress;
    std::uint32_t indexCount;
    std::uint32_t indexElementBytes;
    std::uint32_t instanceCount;
};

struct GraphicsCompileContext {
    std::uint32_t firstUserSgpr;
    std::span<const LinkedProgram> linkedPrograms;
    std::optional<MeshConfiguration> mesh;
    std::optional<TessellationConfiguration> tessellation;
    GraphicsDrawParameters draw;
};

struct RecompileRequest {
    ShaderBinary shader;
    GuestContext context;
    SpirvTarget target;
    BindingLayout layout;
    std::optional<GraphicsCompileContext> graphics;
    bool useCache = true;
};

enum class DescriptorKind {
    UniformBuffer,
    StorageBuffer,
    UniformTexelBuffer,
    StorageTexelBuffer,
    SampledImage,
    StorageImage,
    Sampler
};

enum class DescriptorImageShape {
    Image1D,
    Image2D,
    Image2DArray,
    ImageCube,
    Image3D
};

enum class DescriptorRole {
    GuestBuffers,
    GuestImages,
    GuestSamplers,
    Gds,
    BdaPagetable,
    FaultBuffer,
    FlattenedSrt,
    ShaderData
};

struct DescriptorBinding {
    DescriptorKind kind;
    DescriptorRole role;
    std::uint32_t descriptorSet;
    std::uint32_t binding;
    std::uint32_t count;
    std::vector<std::uint32_t> guestDescriptor;
    bool readOnly = false;
    std::optional<DescriptorImageShape> imageShape;
    std::vector<bool> samplerDepthCompare;

    std::vector<bool> imageWritten;
    std::vector<bool> imageDepthCompare;

    std::vector<bool> bufferAtomic;

    std::vector<bool> bufferWritten;
};

struct VertexAttribute {
    std::uint32_t location;
    std::uint32_t components;
    ShaderVertexBufferResource resource;
    std::uint32_t fetchIndex;
};

struct FragmentParameter {
    std::uint32_t location;
    std::uint32_t sourceLocation;
    bool flat;
    bool perVertex;
    bool custom = false;
};

class SharedSpirv {
public:
    SharedSpirv() = default;
    SharedSpirv(std::vector<std::uint32_t> words) : words(std::make_shared<std::vector<std::uint32_t>>(std::move(words))) {}
    SharedSpirv& operator=(std::vector<std::uint32_t> other) {
        words = std::make_shared<std::vector<std::uint32_t>>(std::move(other));
        return *this;
    }

    [[nodiscard]] const std::vector<std::uint32_t>& Words() const { return words ? *words : Empty(); }
    operator const std::vector<std::uint32_t>&() const { return Words(); }
    [[nodiscard]] std::size_t size() const { return Words().size(); }
    [[nodiscard]] bool empty() const { return Words().empty(); }
    [[nodiscard]] const std::uint32_t* data() const { return Words().data(); }
    [[nodiscard]] std::uint32_t* data() { return Mutable().data(); }
    [[nodiscard]] const std::uint32_t& operator[](std::size_t index) const { return Words()[index]; }
    [[nodiscard]] std::uint32_t& operator[](std::size_t index) { return Mutable()[index]; }
    [[nodiscard]] std::vector<std::uint32_t>::const_iterator begin() const { return Words().begin(); }
    [[nodiscard]] std::vector<std::uint32_t>::const_iterator end() const { return Words().end(); }
    [[nodiscard]] std::vector<std::uint32_t>::iterator begin() { return Mutable().begin(); }
    [[nodiscard]] std::vector<std::uint32_t>::iterator end() { return Mutable().end(); }
    void resize(std::size_t count) { Mutable().resize(count); }
    std::vector<std::uint32_t>::iterator insert(std::vector<std::uint32_t>::const_iterator where, std::initializer_list<std::uint32_t> values) { return Mutable().insert(where, values); }
    friend bool operator==(const SharedSpirv& left, const SharedSpirv& right) { return left.words == right.words || left.Words() == right.Words(); }

private:
    static const std::vector<std::uint32_t>& Empty() {
        static const std::vector<std::uint32_t> empty;
        return empty;
    }

    std::vector<std::uint32_t>& Mutable() {
        if (words == nullptr || words.use_count() != 1) words = std::make_shared<std::vector<std::uint32_t>>(Words());
        return *words;
    }

    std::shared_ptr<std::vector<std::uint32_t>> words;
};

struct RecompileResult {
    SharedSpirv spirv;
    std::vector<DescriptorBinding> bindings;
    std::vector<std::byte> pushConstants;
    std::uint32_t memoryOffsetDword = 0;
    std::uint32_t bdaAbiVersion = 0;
    std::vector<VertexAttribute> vertexAttributes;
    std::int32_t vertexOffsetSgpr = -1;
    std::int32_t instanceOffsetSgpr = -1;

    bool vertexOffsetShared = false;
    bool instanceOffsetShared = false;
    bool vertexOffsetConflict = false;
    bool instanceOffsetConflict = false;
    std::uint32_t hostSubgroupSize = 0;
    std::vector<std::uint32_t> parameterExports;
    std::vector<FragmentParameter> fragmentParameters;
    bool cacheHit = false;

    std::uint64_t variantId = 0;
};

[[nodiscard]] RecompileResult Recompile(const RecompileRequest& request);

struct ResourceCapture;
[[nodiscard]] std::shared_ptr<const RecompileResult> Recompile(const RecompileRequest& request, const ResourceCapture& capture, bool* memoHit = nullptr);

void SetDebugProbeActive(bool active);
[[nodiscard]] bool DebugProbeActive();
[[nodiscard]] bool RayTracingStrict();
[[nodiscard]] bool RayTracingMiss();

struct RectListShaders {
    RecompileResult control;
    RecompileResult evaluation;
};

[[nodiscard]] RectListShaders BuildRectListShaders(const RecompileResult& vertex, const RecompileResult& fragment, const SpirvTarget& target);

}

#endif
