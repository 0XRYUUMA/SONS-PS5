#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PRESENTATIONSCALER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PRESENTATIONSCALER_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <cstdint>

namespace AgcDriver {

class PresentationScaler {
public:
    PresentationScaler(const Graphics::Context& context, VkFormat sourceFormat, VkFormat destinationFormat);
    ~PresentationScaler();
    PresentationScaler(const PresentationScaler&) = delete;
    PresentationScaler& operator=(const PresentationScaler&) = delete;

    void EnsureSourceImage(std::uint32_t width, std::uint32_t height);
    void RecordUpload(VkCommandBuffer commands, VkBuffer uploadBuffer);
    void RecordImage(VkCommandBuffer commands, VkImage image);

    void RecordBlitInto(VkCommandBuffer commands, VkImage image, VkImageLayout layout, VkFilter filter);
    void RecordBlitInto(VkCommandBuffer commands, VkImage image, VkImageLayout layout, VkFilter filter, std::uint32_t width, std::uint32_t height);

    void RecordCopyInto(VkCommandBuffer commands, VkImage image, VkImageLayout layout);

    void RecordReadback(VkCommandBuffer commands, VkBuffer destination);
    void RecordBlit(VkCommandBuffer commands, VkImage destinationImage, std::uint32_t destinationWidth, std::uint32_t destinationHeight);

    static void RecordBlitFrom(const Graphics::Context& context, VkCommandBuffer commands, VkImage image, VkImageLayout layout, std::uint32_t width, std::uint32_t height, VkFilter filter, VkImage destinationImage, std::uint32_t destinationWidth, std::uint32_t destinationHeight);
    std::uint32_t SourceWidth() const { return sourceWidth; }
    std::uint32_t SourceHeight() const { return sourceHeight; }

private:
    void release() noexcept;

    Graphics::Context context;
    VkFormat sourceFormat;
    std::uint32_t sourceWidth = 0;
    std::uint32_t sourceHeight = 0;
    VkImage sourceImage = VK_NULL_HANDLE;
    VkDeviceMemory sourceMemory = VK_NULL_HANDLE;
};

}

#endif
