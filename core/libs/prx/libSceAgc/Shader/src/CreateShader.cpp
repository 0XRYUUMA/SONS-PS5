#include "prx/common/StderrLog.hpp"
#include <cstdlib>
#include "prx/libSceAgc/Shader/include/CreateShader.hpp"

#include <cstdio>
#include <stdexcept>
#include <prx/libc/include/General.hpp>

#include "SceShaders.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgc/Shader/include/ShaderUtils.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"

#ifndef APS5_AGC_CREATE_LOG
#define APS5_AGC_CREATE_LOG 1
#endif

extern "C" {

int APS5_VABI sceAgcCreateShader(Shader** dst, void* header, const volatile void* code) {
    constexpr auto fn = __func__;
    if (dst == nullptr) {
        throw std::runtime_error(std::string(fn) + ": dst is null");
    }
    if (header == nullptr) {
        throw std::runtime_error(std::string(fn) + ": header is null");
    }
    if (code == nullptr) {
        throw std::runtime_error(std::string(fn) + ": code is null");
    }

    auto* h = static_cast<Shader*>(header);

    if (h->file_header != ShaderRegs::SHADER_FILE_HEADER_MAGIC || h->version != ShaderRegs::SHADER_VERSION) {
        throw std::runtime_error(std::string(fn) + ": invalid shader header or version");
    }
    const auto base = reinterpret_cast<std::uint64_t>(code);
    if ((base & ShaderRegs::SHADER_BASE_ALIGN_MASK) != 0 || h->shader_size == 0 || (h->shader_size & 3u) != 0) {
        throw std::runtime_error(std::string(fn) + ": invalid shader code address or size");
    }
    std::uint32_t programOffset = 0;
    (void)GetProgramAddressRegisterOffset(h->type, programOffset);

    ResolveRelativePtr(h->cx_registers);
    ResolveRelativePtr(h->sh_registers);
    ResolveRelativePtr(h->user_data);
    ResolveRelativePtr(h->specials);
    ResolveRelativePtr(h->input_semantics);
    ResolveRelativePtr(h->output_semantics);

    if (h->user_data != nullptr) {
        ResolveRelativePtr(h->user_data->direct_resource_offset);
        ResolveRelativePtr(h->user_data->sharp_resource_offset[0]);
        ResolveRelativePtr(h->user_data->sharp_resource_offset[1]);
        ResolveRelativePtr(h->user_data->sharp_resource_offset[2]);
        ResolveRelativePtr(h->user_data->sharp_resource_offset[3]);
    }

    h->code = code;

    int result = PatchProgramAddressRegister(h->sh_registers, h->num_sh_registers, h->type, base);
    if (result != 0) {
        return result;
    }

    {
        static int shown = 0;
        if (std::getenv("APS5_TRACE_SHDR") != nullptr && shown++ < 80) {
            aps5::LogErr( "[shdr] type %u cx %u sh %u in_sem %u out_sem %u cx regs:", static_cast<unsigned>(h->type), static_cast<unsigned>(h->num_cx_registers), static_cast<unsigned>(h->num_sh_registers), h->num_input_semantics, static_cast<unsigned>(h->num_output_semantics));
            for (unsigned i = 0; h->cx_registers != nullptr && i < h->num_cx_registers && i < 40; ++i) aps5::LogErr( " %x=%x", h->cx_registers[i].offset, h->cx_registers[i].value);
            aps5::LogChar(aps5::LogStdErr, 10);
        }
    }
    AgcDriverRegisterShader_nid_postfix(h);
    *dst = h;

    return 0;
}

}
