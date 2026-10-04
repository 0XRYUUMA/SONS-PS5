#include <cstdio>
#include "prx/common/StderrLog.hpp"
#include "SceShaders.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include <array>
#include <algorithm>

extern "C" int APS5_VABI sceAgcCreatePrimState(ShaderRegister*, ShaderRegister*, const Shader*, const Shader*, std::uint32_t);
extern "C" int APS5_VABI sceAgcCreateInterpolantMapping(ShaderRegister*, const Shader*, const Shader*);

extern "C" int APS5_VABI sceAgcLinkShaders(ShaderRegister* context, ShaderRegister* primitive,
    const void* reserved, const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType) {
    using namespace ShaderRegs;
    { static int n = 0; if (n++ < 8) aps5::LogErr( "[agc] LinkShaders context=%p primitive=%p vs=%p ps=%p primType=%u ps_inputs=%u\n", static_cast<void*>(context), static_cast<void*>(primitive), static_cast<const void*>(vertex), static_cast<const void*>(pixel), primitiveType, pixel ? pixel->num_input_semantics : 0u); }
    if (!context || !primitive || reserved || !vertex ||
        vertex->type != static_cast<std::uint8_t>(ShaderBinaryType::Gs) ||
        (vertex->num_output_semantics && !vertex->output_semantics) ||
        (pixel && (pixel->type != static_cast<std::uint8_t>(ShaderBinaryType::Ps) ||
                   pixel->num_input_semantics > 32 ||
                   (pixel->num_input_semantics && !pixel->input_semantics))))
        return GRAPHICS5_ERROR_INVALID_SHADER_PROGRAM;

    std::array<ShaderRegister, 34> contextValues{};
    std::array<ShaderRegister, 3> primitiveValues{};
    sceAgcCreatePrimState(contextValues.data(), primitiveValues.data(), nullptr, vertex, primitiveType);
    sceAgcCreateInterpolantMapping(contextValues.data() + 2, vertex, pixel);
    std::copy(contextValues.begin(), contextValues.end(), context);
    std::copy(primitiveValues.begin(), primitiveValues.end(), primitive);
    return 0;
}
