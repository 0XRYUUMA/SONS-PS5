#pragma once
#include <cstddef>
#include <cstdint>

struct GuestDirectoryEntry {
    std::uint32_t fileNumber;
    std::uint16_t recordLength;
    std::uint8_t type;
    std::uint8_t nameLength;
    char name[256];
};
static_assert(offsetof(GuestDirectoryEntry, name) == 8);
static_assert(sizeof(GuestDirectoryEntry) == 264);
