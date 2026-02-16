#pragma once

#include <cstdint>

// Data contract between partner_bitmap_transform (transformer) and
// presentation exporters (asm/c/bin). Stored in snatch_font::user_data.

constexpr std::uint32_t SNATCH_PARTNER_BITMAP_MAGIC = 0x5042544Du; // "PBTM"
constexpr std::uint16_t SNATCH_PARTNER_BITMAP_VERSION = 1u;

struct snatch_partner_bitmap_data {
    std::uint32_t magic{SNATCH_PARTNER_BITMAP_MAGIC};
    std::uint16_t version{SNATCH_PARTNER_BITMAP_VERSION};
    const std::uint8_t* bytes{nullptr}; // full serialized Partner stream
    std::uint32_t size{0};              // bytes length
};

