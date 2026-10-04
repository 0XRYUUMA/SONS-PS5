#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PM4_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PM4_HPP

#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4Opcodes.hpp"
#include <cstddef>
#include <vector>
#include <optional>
#include <array>
#include <span>
#include <string>

namespace AgcDriver::Pm4 {

constexpr std::size_t GdsBytes = 0x10000;
std::uint64_t GdsAddress();

struct DrawParameters {
    std::uint64_t indexAddress;
    std::uint32_t indexCount;
    std::uint32_t indexSize;
    std::uint32_t instanceCount;
    std::uint32_t flags;
    bool indexed = true;
    std::uint32_t firstVertex = 0;
    std::uint32_t firstInstance = 0;

    struct IndirectDraw {
        std::uint64_t arguments;
        std::uint32_t opcode;
        std::uint32_t recordBytes;
        std::uint32_t stride;
        std::uint32_t count;
        bool countIndirect;
        std::uint64_t countAddress;
        std::uint32_t baseVertexLocation;
        std::uint32_t startInstanceLocation;
        std::uint32_t drawIndexLocation;
        bool drawIndexEnabled;
        std::uint32_t indxOffset;
        enum class Rule : std::uint8_t { InPlace, Constant };
        Rule vertexRule = Rule::Constant;
        Rule instanceRule = Rule::Constant;
        std::uint32_t vertexConstant = 0;
        std::uint32_t instanceConstant = 0;
        std::int32_t baseVertexSgpr = -1;
        std::int32_t startInstanceSgpr = -1;
        std::int32_t drawIndexSgpr = -1;

        std::uint64_t RangeBytes() const { return count == 0 ? 0 : static_cast<std::uint64_t>(count - 1) * stride + recordBytes; }
        std::uint32_t VertexDwordOffset() const { return recordBytes == 20 ? 12u : 8u; }
        std::uint32_t InstanceDwordOffset() const { return recordBytes == 20 ? 16u : 12u; }
    };
    std::optional<IndirectDraw> indirect;
};

struct DrawArguments {
    std::uint32_t count;
    std::uint32_t instances;
    std::uint32_t firstVertexOrIndex;
    std::uint32_t vertexOffset;
    std::uint32_t firstInstance;
};

DrawArguments ReadDrawArguments(const DrawParameters::IndirectDraw& indirect, std::uint32_t record);
std::uint32_t ReadDrawCount(const DrawParameters::IndirectDraw& indirect);
inline bool IndirectDrawOpcode(std::uint32_t opcode) { return opcode == 0x24 || opcode == 0x25 || opcode == 0x2c || opcode == 0x38; }
inline bool DrawOpcode(std::uint32_t opcode) { return opcode == 0x27 || opcode == 0x2d || opcode == 0x35 || IndirectDrawOpcode(opcode); }

std::string Name(std::uint32_t header);

inline bool FillerPacket(std::uint32_t header) { return (header >> 30u) == 2u; }
inline std::size_t PacketWords(std::uint32_t header) { return FillerPacket(header) ? 1u : static_cast<std::size_t>((header >> 16u) & 0x3fffu) + 2u; }
std::string_view UnsupportedReason(std::uint32_t header);
void Validate(std::span<const std::uint32_t> packet, std::uint32_t queue);
void Execute(std::span<const std::uint32_t> packet, QueueState& queue);
bool AccessesMemory(std::uint32_t header);

bool UsesGpuCacheBarrier(std::span<const std::uint32_t> packet);
bool IsTagMarker(std::span<const std::uint32_t> packet);
bool WaitSatisfied(std::span<const std::uint32_t> packet);

bool WaitSatisfiedUnchecked(std::span<const std::uint32_t> packet);

bool WaitComparesValue(std::span<const std::uint32_t> packet, std::uint64_t value);
std::size_t WaitAwaitedBytes(std::span<const std::uint32_t> packet);

struct LabelWrite {
    std::uint64_t address;
    std::span<const std::byte> packetBytes;
    std::array<std::byte, 8> inlineBytes{};
    std::size_t inlineSize = 0;
    std::span<const std::byte> Bytes() const { return inlineSize != 0 ? std::span<const std::byte>(inlineBytes).first(inlineSize) : packetBytes; }
};
std::optional<LabelWrite> DecodeLabelWrite(std::span<const std::uint32_t> packet);

struct StoreWrite {
    std::uint64_t address;
    std::span<const std::byte> viewBytes;
    std::vector<std::byte> ownedBytes;
    std::span<const std::byte> Bytes() const { return ownedBytes.empty() ? viewBytes : std::span<const std::byte>(ownedBytes); }
};
std::optional<StoreWrite> ResolveStore(std::span<const std::uint32_t> packet, const QueueState& queue, std::size_t limit);

std::uint64_t DispatchArgumentAddress(std::span<const std::uint32_t> packet, const QueueState& queue);
std::array<std::uint32_t, 5> ReadDispatchArguments(std::uint64_t arguments, std::uint32_t initiator);
std::array<std::uint32_t, 5> ResolveDispatch(std::span<const std::uint32_t> packet, const QueueState& queue);
DrawParameters ResolveDraw(std::span<const std::uint32_t> packet, const QueueState& queue);

}

#endif
