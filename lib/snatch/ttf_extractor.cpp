/// \file
/// \brief FreeType-based rasterization of TTF glyphs to 1bpp bitmaps.
///
/// This source file implements one part of the snatch pipeline architecture. It contributes to extracting, transforming, exporting, or orchestrating bitmap data in a plugin-driven workflow.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include "snatch/ttf_extractor.h"
#include "snatch/glyph_algorithms.h"

#include <algorithm>
#include <cmath>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace {

/// \brief rasterize_glyph.
bool rasterize_glyph(FT_Face face, int codepoint, bool proportional, extracted_glyph& out, std::string& err) {
    const int flags = FT_LOAD_RENDER | FT_LOAD_MONOCHROME | FT_LOAD_TARGET_MONO;
    if (FT_Load_Char(face, static_cast<FT_ULong>(codepoint), flags) != 0) {
        err = "failed to load glyph for codepoint " + std::to_string(codepoint);
        return false;
    }

    if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
        if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_MONO) != 0) {
            err = "failed to render glyph for codepoint " + std::to_string(codepoint);
            return false;
        }
    }

    const FT_Bitmap& bmp = face->glyph->bitmap;
    const int stride = std::abs(static_cast<int>(bmp.pitch));
    const int rows = static_cast<int>(bmp.rows);

    out.view.codepoint = codepoint;
    out.view.width = static_cast<int>(bmp.width);
    out.view.height = rows;
    out.view.bearing_x = face->glyph->bitmap_left;
    out.view.bearing_y = face->glyph->bitmap_top;
    out.view.advance_x = static_cast<int>(face->glyph->advance.x >> 6);
    out.view.stride_bytes = stride;

    out.bitmap.assign(static_cast<size_t>(stride * rows), 0);
    if (bmp.buffer && stride > 0 && rows > 0) {
        for (int y = 0; y < rows; ++y) {
            const unsigned char* src = bmp.buffer + static_cast<size_t>(y * stride);
            unsigned char* dst = out.bitmap.data() + static_cast<size_t>(y * stride);
            std::copy_n(src, static_cast<size_t>(stride), dst);
        }
    }

    if (proportional) {
        out.view.data = out.bitmap.empty() ? nullptr : out.bitmap.data();
        const int rightmost = glyph_bitmap_analyzer::rightmost_set_bit(out.view);
        out.view.width = (rightmost >= 0) ? (rightmost + 1) : 0;
    }
    out.view.data = out.bitmap.data();
    return true;
}

} // namespace

/// \brief ttf_extractor::choose_natural_size.
int ttf_extractor::choose_natural_size(void* ft_face) {
    FT_Face face = static_cast<FT_Face>(ft_face);
    if (face->num_fixed_sizes > 0 && face->available_sizes) {
        // Prefer fixed pixel strikes in a typical readable range.
        int best = face->available_sizes[0].y_ppem >> 6;
        for (int i = 0; i < face->num_fixed_sizes; ++i) {
            const int s = face->available_sizes[i].y_ppem >> 6;
            if (s >= 12 && s <= 18) return s;
            if (s > best) best = s;
        }
        return best > 0 ? best : 16;
    }

    // Heuristic for scalable fonts: sample sizes and prefer a clean readable zone.
    const int sample_chars[] = {'H', 'n', 'm', '0', '8', 'A', 'a'};
    int best_size = 16;
    double best_score = -1e18;

    for (int size = 8; size <= 32; ++size) {
        if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(size)) != 0) continue;

        int non_empty = 0;
        int total_h = 0;
        int total_w = 0;
        for (int c : sample_chars) {
            extracted_glyph g;
            std::string err;
            if (!rasterize_glyph(face, c, false, g, err)) continue;
            if (g.view.width > 0 && g.view.height > 0) {
                ++non_empty;
                total_h += g.view.height;
                total_w += g.view.width;
            }
        }
        if (non_empty == 0) continue;

        const double avg_h = static_cast<double>(total_h) / non_empty;
        const double avg_w = static_cast<double>(total_w) / non_empty;
        const double target_h = 14.0;
        const double target_w = 8.0;
        const double score = non_empty * 100.0 - std::abs(avg_h - target_h) * 12.0 - std::abs(avg_w - target_w) * 6.0;
        if (score > best_score) {
            best_score = score;
            best_size = size;
        }
    }
    return best_size;
}

/// \brief ttf_extractor::extract.
bool ttf_extractor::extract(const ttf_extract_options& opt, extracted_font& out, std::string& err) const {
    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library) != 0) {
        err = "failed to initialize FreeType";
        return false;
    }

    FT_Face face = nullptr;
    const std::string font_path = opt.input_file.string();
    if (FT_New_Face(library, font_path.c_str(), 0, &face) != 0) {
        FT_Done_FreeType(library);
        err = "failed to open TTF file: " + font_path;
        return false;
    }

    const int first = (opt.first_ascii >= 0) ? opt.first_ascii : 32;
    const int last = (opt.last_ascii >= 0) ? opt.last_ascii : 126;
    if (first > last) {
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        err = "invalid codepoint range";
        return false;
    }

    const int size = (opt.font_size > 0) ? opt.font_size : choose_natural_size(face);
    if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(size)) != 0) {
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        err = "failed to set pixel size";
        return false;
    }

    out = {};
    out.name = (face->family_name ? face->family_name : "unknown");
    if (face->style_name && face->style_name[0] != '\0') {
        out.name += " ";
        out.name += face->style_name;
    }
    out.first_codepoint = first;
    out.last_codepoint = last;
    out.pixel_size = size;

    const int glyph_count = last - first + 1;
    out.glyphs.reserve(static_cast<size_t>(glyph_count));
    out.glyph_views.reserve(static_cast<size_t>(glyph_count));

    for (int cp = first; cp <= last; ++cp) {
        extracted_glyph g;
        if (!rasterize_glyph(face, cp, opt.proportional, g, err)) {
            FT_Done_Face(face);
            FT_Done_FreeType(library);
            return false;
        }
        out.glyph_width = std::max(out.glyph_width, g.view.width);
        out.glyph_height = std::max(out.glyph_height, g.view.height);
        out.glyphs.push_back(std::move(g));
    }

    for (auto& g : out.glyphs) {
        g.view.data = g.bitmap.empty() ? nullptr : g.bitmap.data();
        out.glyph_views.push_back(g.view);
    }

    out.bitmap_view.glyph_count = static_cast<int>(out.glyph_views.size());
    out.bitmap_view.glyphs = out.glyph_views.empty() ? nullptr : out.glyph_views.data();

    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return true;
}
