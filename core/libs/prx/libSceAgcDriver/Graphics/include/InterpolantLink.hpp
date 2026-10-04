#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_INTERPOLANTLINK_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_INTERPOLANTLINK_HPP

#include "SceShaders.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <array>
#include <cstdint>

namespace AgcDriver::Graphics {

inline std::uint32_t InterpolantSemanticWord(const ShaderSemantic& s) {
    return ((s.semantic & 0xFFu) << 0u) | ((s.hardware_mapping & 0xFFu) << 8u) | ((s.size_in_elements & 0xFu) << 16u) | ((s.is_f16 & 0x3u) << 20u)
         | ((s.is_flat_shaded & 0x1u) << 22u) | ((s.is_linear & 0x1u) << 23u) | ((s.is_custom & 0x1u) << 24u) | ((s.static_vb_index & 0x1u) << 25u)
         | ((s.static_attribute & 0x1u) << 26u) | ((s.reserved & 0x1u) << 27u) | ((s.default_value & 0x3u) << 28u) | ((s.default_value_hi & 0x3u) << 30u);
}

inline std::array<std::uint32_t, 32> LinkInterpolants(const Shader* vertex, const Shader* pixel) {
    std::array<std::uint32_t, 32> out{};
    for (std::uint32_t i = 0; i < out.size(); ++i) out[i] = i;
    if (pixel == nullptr || pixel->num_input_semantics == 0 || pixel->num_input_semantics > 32u || pixel->input_semantics == nullptr) return out;
    if (!GuestMemory::Accessible(pixel->input_semantics, pixel->num_input_semantics * sizeof(ShaderSemantic))) return out;
    const ShaderSemantic* outputs = nullptr;
    std::uint32_t outputCount = 0;
    if (vertex != nullptr && vertex->output_semantics != nullptr && vertex->num_output_semantics != 0 &&
        GuestMemory::Accessible(vertex->output_semantics, vertex->num_output_semantics * sizeof(ShaderSemantic))) {
        outputs = vertex->output_semantics;
        outputCount = vertex->num_output_semantics;
    }
    const auto applyDefault = [](std::uint32_t value, std::uint32_t psWord) { value &= ~0x00000300u; value |= ((psWord >> 28u) & 0x3u) << 8u; return value; };
    for (std::uint32_t i = 0; i < pixel->num_input_semantics; ++i) {
        const ShaderSemantic& ps = pixel->input_semantics[i];
        const ShaderSemantic* gs = nullptr;
        for (std::uint32_t k = 0; k < outputCount; ++k) if (outputs[k].semantic == ps.semantic) { gs = &outputs[k]; break; }
        const std::uint32_t psWord = InterpolantSemanticWord(ps);
        std::uint32_t value;
        if ((psWord & 0x00300000u) != 0) {
            value = (psWord << 4u) & 0x03000000u;
            if (gs == nullptr) {
                value |= 0x00180020u;
            } else {
                const std::uint32_t common = psWord & InterpolantSemanticWord(*gs);
                value &= 0xFFF7FFDFu;
                value |= (common >> 15u) & 0x20u;
                value ^= 0x00080020u;
                value &= ~0x00100000u;
                value |= (~common >> 1u) & 0x00100000u;
            }
            value &= ~0x00600000u;
            value |= ((psWord >> 30u) & 0x3u) << 21u;
        } else {
            value = ((psWord & 0x01000000u) != 0 || gs == nullptr) ? 0x20u : 0u;
        }
        if (gs == nullptr) {
            value &= ~0x0000001Fu;
            value &= ~0x00000400u;
            value = applyDefault(value, psWord);
        } else {
            const std::uint32_t flat = ((psWord & 0x00400000u) != 0 || (psWord & 0x01000000u) != 0) ? 0x00000400u : 0u;
            value &= ~0x0000001Fu;
            value |= (InterpolantSemanticWord(*gs) >> 8u) & 0x1Fu;
            value &= ~0x00000400u;
            value |= flat;
            value = applyDefault(value, psWord);
        }
        out[i] = value;
    }
    return out;
}

}

#endif
