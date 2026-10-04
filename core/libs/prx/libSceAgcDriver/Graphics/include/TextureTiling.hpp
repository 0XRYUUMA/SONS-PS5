#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURETILING_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURETILING_HPP

#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace AgcDriver::Graphics {

struct TileMipLayout {
    std::uint64_t tiledOffset;
    std::uint64_t tiledSize;
    std::uint64_t linearOffset;
    std::uint64_t linearSize;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t blocksPerRow;
    std::uint32_t pitchBytes;
    bool tail;
    std::uint32_t tailX;
    std::uint32_t tailY;
};

std::vector<TileMipLayout> ComputeMipLayout(TextureTileMode tileMode, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t mipCount);
std::uint64_t ComputeSurfaceSize(const std::vector<TileMipLayout>& mips, std::uint32_t arrayLayers);

std::vector<TileMipLayout> ComputeElementMipLayout(TextureTileMode tileMode, std::uint32_t bytesPerElement, std::uint32_t width, std::uint32_t height, std::uint32_t mipCount);

std::array<std::uint32_t, 3> ThickBlockExtent(TextureTileMode tileMode, std::uint32_t bytesPerElement);

std::array<std::uint32_t, 3> ThinBlockLayout(TextureTileMode tileMode, std::uint32_t bytesPerElement);

struct ThickLayout {
    std::vector<TileMipLayout> mips;
    std::uint32_t depth;
    std::uint32_t blockDepth;
    std::uint64_t slabBytes;
    std::uint64_t sliceLinearBytes;
    std::uint64_t guestBytes;
};
ThickLayout ComputeThickLayout(TextureTileMode tileMode, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t depth, std::uint32_t mipCount);

struct SurfaceGeometry {
    std::vector<TileMipLayout> mips;
    std::uint32_t layers = 1;
    std::uint32_t imageLayers = 1;
    std::uint32_t imageDepth = 1;
    std::uint64_t guestBytes = 0;
    std::uint64_t sliceLinearBytes = 0;
    bool thick = false;
    std::uint32_t blockDepth = 1;
    std::uint64_t layerBytes = 0;

    std::uint64_t GuestLayerOffset(std::uint32_t layer) const { return thick ? static_cast<std::uint64_t>(layer / blockDepth) * layerBytes : static_cast<std::uint64_t>(layer) * layerBytes; }
    std::uint64_t LinearLayerOffset(std::uint32_t layer) const { return static_cast<std::uint64_t>(layer) * sliceLinearBytes; }
    std::uint32_t CopyLayer(std::uint32_t layer) const { return imageDepth > 1 ? 0u : layer; }
    std::int32_t CopyDepth(std::uint32_t layer) const { return imageDepth > 1 ? static_cast<std::int32_t>(layer) : 0; }
    bool HasLayer(std::uint32_t level, std::uint32_t layer) const { return imageDepth <= 1 || layer < std::max(imageDepth >> level, 1u); }
};
SurfaceGeometry DescribeSurface(const GuestTextureResource& descriptor);

}

#endif
