#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_ASYNCDRAW_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_ASYNCDRAW_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"

namespace AgcDriver::DriverDetail {

struct Driver::AsyncDraw {
    std::shared_ptr<const DrawDecode> decode;
    Pm4::DrawParameters drawParameters{};
    std::vector<DrawProgram> programs;
    std::vector<ShaderRecompiler::RecompileResult> results;
    std::vector<Graphics::CompiledShader> stages;
    std::vector<std::shared_ptr<DispatchVariant>> matched, fresh;
    std::vector<std::vector<ShaderRecompiler::MemoryRegion>> matchedRegions;
    std::vector<std::vector<Graphics::DecodeRead>> decodeReads;
    std::unique_ptr<ShaderMemory> shaderMemory;
    std::vector<ShaderRecompiler::MemoryRegion> memory;
    std::vector<Graphics::GuestMemorySnapshot> snapshots;
    std::shared_ptr<const DrawRecipe> recipe;
    std::vector<std::shared_ptr<DispatchVariant>> recipeStages;
    std::shared_ptr<VulkanDevice> device;
    std::uint64_t drawKey = 0;
    std::uint64_t epochSeq = 0;
    std::uint32_t queue = 0;
    std::uint64_t color = 0;
    std::uint64_t frame = 0;
};

}

#endif
