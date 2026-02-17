/// \file
/// \brief Shared Partner Tiny payload definitions for plugins.
///
/// This header declares types and contracts used by snatch core and plugin stages. It is part of the extractor-transformer-exporter architecture and is consumed by build-time and runtime plugin integration.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <cstdint>

// Data contract between partner_tiny_transform (transformer) and
// partner_asm (exporter). Stored in snatch_font::user_data.

constexpr std::uint32_t SNATCH_PARTNER_TINY_MAGIC = 0x50544E59u; // "PTNY"
constexpr std::uint16_t SNATCH_PARTNER_TINY_VERSION = 1u;

struct snatch_partner_tiny_glyph {
    std::uint16_t codepoint{0};
    std::uint8_t width_minus_one{0};
    std::uint8_t height_minus_one{0};
    std::uint16_t data_size{0};              // includes x_origin,y_origin and tiny move bytes
    const std::uint8_t* data{nullptr};       // pointer to encoded tiny glyph payload
};

struct snatch_partner_tiny_data {
    std::uint32_t magic{SNATCH_PARTNER_TINY_MAGIC};
    std::uint16_t version{SNATCH_PARTNER_TINY_VERSION};
    std::uint16_t glyph_count{0};
    std::uint8_t max_width_minus_one{0};
    std::uint8_t max_height_minus_one{0};
    const snatch_partner_tiny_glyph* glyphs{nullptr};
};

