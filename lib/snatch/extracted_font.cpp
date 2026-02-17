/// \file
/// \brief Conversions from extracted font types to plugin-facing structs.
///
/// This source file implements one part of the snatch pipeline architecture. It contributes to extracting, transforming, exporting, or orchestrating bitmap data in a plugin-driven workflow.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include "snatch/extracted_font.h"

/// \brief extracted_font::as_plugin_font.
snatch_font extracted_font::as_plugin_font() const {
    snatch_font out{};
    out.name = name.c_str();
    out.glyph_width = glyph_width;
    out.glyph_height = glyph_height;
    out.first_codepoint = first_codepoint;
    out.last_codepoint = last_codepoint;
    out.pixel_size = pixel_size;
    out.bitmap_font = &bitmap_view;
    out.user_data = nullptr;
    return out;
}
