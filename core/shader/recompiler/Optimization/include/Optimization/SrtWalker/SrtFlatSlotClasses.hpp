#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTFLATSLOTCLASSES_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTFLATSLOTCLASSES_HPP

#include "IntermediateRepresentation/IrProgram.hpp"

#include <cstdint>
#include <vector>

namespace ShaderRecompiler::Detail {

std::vector<std::uint8_t> ComputePureFlatSlots(const IrResourcePlan& plan);

}

#endif
