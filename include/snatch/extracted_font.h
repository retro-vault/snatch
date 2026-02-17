/// \file
/// \brief Internal extracted font and glyph container types.
///
/// This header declares types and contracts used by snatch core and plugin stages. It is part of the extractor-transformer-exporter architecture and is consumed by build-time and runtime plugin integration.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <string>
#include <vector>

#include "snatch/plugin.h"

struct extracted_glyph {
    snatch_glyph_bitmap view{};
    std::vector<unsigned char> bitmap;
};

struct extracted_font {
    std::string name;
    int glyph_width{0};
    int glyph_height{0};
    int first_codepoint{0};
    int last_codepoint{0};
    int pixel_size{0};

    std::vector<extracted_glyph> glyphs;
    std::vector<snatch_glyph_bitmap> glyph_views;
    snatch_bitmap_font bitmap_view{};

    snatch_font as_plugin_font() const;
};
