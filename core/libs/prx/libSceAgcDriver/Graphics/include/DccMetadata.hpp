#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DCCMETADATA_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DCCMETADATA_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include <cstddef>
#include <cstdint>
#include <span>

namespace AgcDriver::Graphics {

struct Context;

enum class DccKeys { Uncompressed, Clear0000, Clear0001, Clear1110, Clear1111, ClearRegister, Mixed, Unreadable };

const char* DccKeysName(DccKeys keys);

DccKeys ReadDccKeys(std::uint64_t metaAddress, std::uint64_t surfaceBytes);
bool IsDccClear(DccKeys keys);
DccKeys CurrentDccKeys(std::uint64_t metaAddress, std::uint64_t surfaceBytes);

void MarkDccUncompressed(std::uint64_t metaAddress, std::uint64_t surfaceBytes);

void MarkDccUncompressed(const Context& context, std::uint64_t metaAddress, std::uint64_t surfaceBytes);

bool FillDccClear(VkFormat format, DccKeys keys, bool alphaOnMsb, std::span<std::byte> bytes);

bool DccAlphaOnMsb(VkFormat format, std::uint32_t componentSwap);

DccKeys TextureClearKeys(const GuestTextureResource& resource, std::uint64_t guestBytes);

struct DccKeyProof {
    DccKeys keys = DccKeys::Uncompressed;
    std::uint64_t generation = 0;
};

DccKeys ProvedClearKeys(const GuestTextureResource& resource, std::uint64_t guestBytes, DccKeyProof& proof);
bool KeyFastPath();

struct DccKeyProofCounts {
    std::uint64_t proved;
    std::uint64_t scanned;
    std::uint64_t unstable;
};
DccKeyProofCounts KeyProofCounts();

void ReadTextureSurface(const GuestTextureResource& resource, DccKeys keys, std::span<std::byte> bytes);
void NoteKeysFillOnGpu(std::uint64_t begin, std::size_t count, DccKeys keys);

}

#endif
