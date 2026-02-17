/// \file
/// \brief Implementation of bitmap glyph extraction from raster images.
///
/// This source file implements one part of the snatch pipeline architecture. It contributes to extracting, transforming, exporting, or orchestrating bitmap data in a plugin-driven workflow.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include "snatch/img_extractor.h"
#include "snatch/glyph_algorithms.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

extern "C" {
#include <stb_image.h>
}

namespace {

constexpr int k_color_threshold = 48;

struct rgba {
    int r{0};
    int g{0};
    int b{0};
    int a{255};
};

/// \brief stride_for_bits.
inline int stride_for_bits(int width) {
    return (width + 7) / 8;
}

/// \brief set_bit.
inline void set_bit(unsigned char* row, int x) {
    const int byte_index = x / 8;
    const int bit_index = 7 - (x % 8);
    row[byte_index] |= static_cast<unsigned char>(1u << bit_index);
}

/// \brief color_distance_sq.
double color_distance_sq(const rgba& c, const color_rgb& ref) {
    const int dr = c.r - ref.r;
    const int dg = c.g - ref.g;
    const int db = c.b - ref.b;
    return static_cast<double>(dr * dr + dg * dg + db * db);
}

/// \brief is_near_color.
bool is_near_color(const rgba& c, const color_rgb& ref, int threshold) {
    return color_distance_sq(c, ref) <= static_cast<double>(threshold * threshold);
}

/// \brief pixel_is_foreground.
bool pixel_is_foreground(const rgba& px, const image_extract_options& opt) {
    if (px.a == 0) return false;
    if (opt.has_transparent && is_near_color(px, opt.transparent_color, k_color_threshold)) return false;

    const double d_fore = color_distance_sq(px, opt.fore_color);
    const double d_back = color_distance_sq(px, opt.back_color);
    bool on = d_fore <= d_back;
    if (opt.inverse) on = !on;
    return on;
}

} // namespace

/// \brief img_extractor::extract.
bool img_extractor::extract(const image_extract_options& opt, extracted_font& out, std::string& err) const {
    int img_w = 0;
    int img_h = 0;
    unsigned char* image = stbi_load(opt.input_file.string().c_str(), &img_w, &img_h, nullptr, 4);
    if (!image) {
        err = "failed to open image file: " + opt.input_file.string();
        return false;
    }

    const int first = (opt.first_ascii >= 0) ? opt.first_ascii : 32;
    const int last = (opt.last_ascii >= 0) ? opt.last_ascii : 126;
    if (first > last) {
        stbi_image_free(image);
        err = "invalid codepoint range";
        return false;
    }

    if (opt.columns <= 0) {
        stbi_image_free(image);
        err = "image extraction requires --columns (>0)";
        return false;
    }

    const int glyph_count = last - first + 1;
    int rows = opt.rows;
    if (rows <= 0) {
        rows = (glyph_count + opt.columns - 1) / opt.columns;
    }
    if (rows <= 0) {
        stbi_image_free(image);
        err = "invalid row count for image extraction";
        return false;
    }

    if (opt.columns * rows < glyph_count) {
        stbi_image_free(image);
        err = "grid too small for requested ASCII range (columns*rows < glyph count)";
        return false;
    }

    const int usable_w = img_w - opt.margins.left - opt.margins.right;
    const int usable_h = img_h - opt.margins.top - opt.margins.bottom;
    if (usable_w <= 0 || usable_h <= 0) {
        stbi_image_free(image);
        err = "invalid margins: no drawable area remains";
        return false;
    }

    const int cell_w = usable_w / opt.columns;
    const int cell_h = usable_h / rows;
    if (cell_w <= 0 || cell_h <= 0) {
        stbi_image_free(image);
        err = "grid cell size became zero; check margins/rows/columns";
        return false;
    }

    const int draw_w = cell_w - opt.padding.left - opt.padding.right;
    const int draw_h = cell_h - opt.padding.top - opt.padding.bottom;
    if (draw_w <= 0 || draw_h <= 0) {
        stbi_image_free(image);
        err = "invalid padding: no drawable area remains inside glyph cell";
        return false;
    }

    out = {};
    out.name = opt.input_file.stem().string();
    out.first_codepoint = first;
    out.last_codepoint = last;
    out.pixel_size = 0;
    out.glyphs.reserve(static_cast<size_t>(glyph_count));
    out.glyph_views.reserve(static_cast<size_t>(glyph_count));

    const int full_stride = stride_for_bits(draw_w);

    for (int i = 0; i < glyph_count; ++i) {
        const int codepoint = first + i;
        const int row = i / opt.columns;
        const int col = i % opt.columns;
        const int cell_x = opt.margins.left + col * cell_w;
        const int cell_y = opt.margins.top + row * cell_h;
        const int start_x = cell_x + opt.padding.left;
        const int start_y = cell_y + opt.padding.top;

        extracted_glyph g;
        g.view.codepoint = codepoint;
        g.view.height = draw_h;
        g.view.width = draw_w;
        g.view.bearing_x = 0;
        g.view.bearing_y = draw_h;
        g.view.advance_x = draw_w;
        g.view.stride_bytes = full_stride;
        g.bitmap.assign(static_cast<size_t>(full_stride * draw_h), 0);

        for (int y = 0; y < draw_h; ++y) {
            unsigned char* bits_row = g.bitmap.data() + static_cast<size_t>(y * full_stride);
            const int sy = start_y + y;
            for (int x = 0; x < draw_w; ++x) {
                const int sx = start_x + x;
                if (sx < 0 || sx >= img_w || sy < 0 || sy >= img_h) continue;
                const size_t pix_index = static_cast<size_t>((sy * img_w + sx) * 4);
                const rgba px{
                    static_cast<int>(image[pix_index + 0]),
                    static_cast<int>(image[pix_index + 1]),
                    static_cast<int>(image[pix_index + 2]),
                    static_cast<int>(image[pix_index + 3])
                };
                if (pixel_is_foreground(px, opt)) set_bit(bits_row, x);
            }
        }

        if (opt.proportional) {
            g.view.data = g.bitmap.empty() ? nullptr : g.bitmap.data();
            const int rightmost = glyph_bitmap_analyzer::rightmost_set_bit(g.view);
            g.view.width = (rightmost >= 0) ? (rightmost + 1) : 0;
            g.view.advance_x = g.view.width;
            g.view.stride_bytes = stride_for_bits(g.view.width);
            if (g.view.stride_bytes != full_stride) {
                std::vector<unsigned char> packed(static_cast<size_t>(g.view.stride_bytes * draw_h), 0);
                for (int y = 0; y < draw_h; ++y) {
                    const unsigned char* src = g.bitmap.data() + static_cast<size_t>(y * full_stride);
                    unsigned char* dst = packed.data() + static_cast<size_t>(y * g.view.stride_bytes);
                    for (int x = 0; x < g.view.width; ++x) {
                        const int src_byte = x / 8;
                        const int src_bit = 7 - (x % 8);
                        if ((src[src_byte] & (1u << src_bit)) != 0) set_bit(dst, x);
                    }
                }
                g.bitmap.swap(packed);
            }
        }

        g.view.data = g.bitmap.empty() ? nullptr : g.bitmap.data();
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

    stbi_image_free(image);
    return true;
}
