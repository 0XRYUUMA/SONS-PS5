#pragma once

#ifdef _WIN32
#include <windows.h>
#include <cstddef>
#include <cstdint>

namespace sse4a {

enum class Op : std::uint8_t { Extrq, Insertq };

struct Instruction {
    Op op = Op::Extrq;
    bool registerForm = false;
    unsigned destination = 0;
    unsigned source = 0;
    std::uint8_t length = 0;
    std::uint8_t index = 0;
    std::size_t size = 0;
};

struct Field {
    unsigned length = 0;
    unsigned index = 0;
};

constexpr std::size_t kMaxInstructionSize = 7;

inline bool Decode(const std::uint8_t* bytes, std::size_t available, Instruction& out) {
    std::size_t cursor = 0;
    if (available < 4) return false;
    const std::uint8_t prefix = bytes[cursor++];
    if (prefix != 0x66 && prefix != 0xf2) return false;
    std::uint8_t rex = 0;
    if ((bytes[cursor] & 0xf0) == 0x40) rex = bytes[cursor++];
    if (cursor + 3 > available) return false;
    if (bytes[cursor++] != 0x0f) return false;
    const std::uint8_t opcode = bytes[cursor++];
    if (opcode != 0x78 && opcode != 0x79) return false;
    const std::uint8_t modrm = bytes[cursor++];
    if ((modrm >> 6) != 3) return false;
    const unsigned regField = (modrm >> 3) & 7;
    const unsigned reg = regField | ((rex & 0x4) ? 8u : 0u);
    const unsigned rm = (modrm & 7) | ((rex & 0x1) ? 8u : 0u);
    Instruction result;
    result.op = prefix == 0x66 ? Op::Extrq : Op::Insertq;
    result.registerForm = opcode == 0x79;
    if (opcode == 0x78) {
        if (cursor + 2 > available) return false;
        result.length = bytes[cursor++];
        result.index = bytes[cursor++];
        if (result.op == Op::Extrq) {
            if (regField != 0) return false;
            result.destination = result.source = rm;
        } else {
            result.destination = reg;
            result.source = rm;
        }
    } else {
        result.destination = reg;
        result.source = rm;
    }
    result.size = cursor;
    out = result;
    return true;
}

inline M128A& Register(CONTEXT& context, unsigned number) {
    return context.FltSave.XmmRegisters[number & 15];
}

inline Field Resolve(const Instruction& instruction, const CONTEXT& context) {
    Field field;
    if (instruction.registerForm) {
        const M128A& source = context.FltSave.XmmRegisters[instruction.source & 15];
        const auto control = instruction.op == Op::Extrq ? static_cast<std::uint64_t>(source.Low) : static_cast<std::uint64_t>(source.High);
        field.length = static_cast<unsigned>(control & 0x3f);
        field.index = static_cast<unsigned>((control >> 8) & 0x3f);
    } else {
        field.length = instruction.length & 0x3f;
        field.index = instruction.index & 0x3f;
    }
    if (field.length == 0) field.length = 64;
    return field;
}

inline std::uint64_t FieldMask(unsigned length) {
    return length >= 64 ? ~std::uint64_t{0} : ((std::uint64_t{1} << length) - 1);
}

inline Field Execute(const Instruction& instruction, CONTEXT& context) {
    const Field field = Resolve(instruction, context);
    const std::uint64_t mask = FieldMask(field.length);
    M128A& destination = Register(context, instruction.destination);
    const M128A& source = Register(context, instruction.source);
    const auto low = static_cast<std::uint64_t>(destination.Low);
    if (instruction.op == Op::Extrq) {
        destination.Low = (low >> field.index) & mask;
    } else {
        const std::uint64_t hole = mask << field.index;
        const std::uint64_t bits = (static_cast<std::uint64_t>(source.Low) & mask) << field.index;
        destination.Low = (low & ~hole) | bits;
    }
    return field;
}

inline bool Emulate(const std::uint8_t* bytes, std::size_t available, CONTEXT& context, Instruction* decoded = nullptr, Field* field = nullptr) {
    Instruction instruction;
    if (!Decode(bytes, available, instruction)) return false;
    const Field resolved = Execute(instruction, context);
    context.Rip += instruction.size;
    if (decoded) *decoded = instruction;
    if (field) *field = resolved;
    return true;
}

}
#endif
