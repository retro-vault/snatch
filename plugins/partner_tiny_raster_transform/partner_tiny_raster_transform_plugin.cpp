/// \file
/// \brief Partner Tiny raster reconstruction transformer plugin implementation.
///
/// This source file implements one part of the snatch pipeline architecture. It contributes to extracting, transforming, exporting, or orchestrating bitmap data in a plugin-driven workflow.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include "snatch/plugin.h"
#include "snatch/plugin_util.h"
#include "snatch_plugins/partner_tiny_bin.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

struct point_i {
    int x{0};
    int y{0};
};

struct glyph_owner {
    snatch_glyph_bitmap view{};
    std::vector<std::uint8_t> bytes;
};

struct raster_owner {
    std::vector<glyph_owner> glyphs;
    std::vector<snatch_glyph_bitmap> glyph_views;
    snatch_bitmap_font bitmap_view{};
};

static raster_owner g_owner;

/// \brief read_u16le.
inline std::uint16_t read_u16le(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8u));
}

/// \brief in_bounds.
inline bool in_bounds(int x, int y, int w, int h) {
    return x >= 0 && y >= 0 && x < w && y < h;
}

/// \brief write_pixel.
inline void write_pixel(std::vector<std::uint8_t>& bytes, int stride, int w, int h, int x, int y, std::uint8_t color) {
    if (!in_bounds(x, y, w, h)) return;
    const int byte_index = x / 8;
    const int bit_index = 7 - (x % 8);
    auto& b = bytes[static_cast<std::size_t>(y * stride + byte_index)];
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << bit_index);
    if (color == 1) b = static_cast<std::uint8_t>(b | mask);         // set
    else if (color == 2) b = static_cast<std::uint8_t>(b & ~mask);   // clear
    else if (color == 3) b = static_cast<std::uint8_t>(b ^ mask);    // xor/toggle
}

/// \brief draw_line.
void draw_line(std::vector<std::uint8_t>& bytes, int stride, int w, int h, point_i start, point_i end, std::uint8_t color) {
    bool steep = std::abs(end.y - start.y) > std::abs(end.x - start.x);
    if (steep) {
        std::swap(start.x, start.y);
        std::swap(end.x, end.y);
    }
    if (start.x > end.x) {
        std::swap(start.x, end.x);
        std::swap(start.y, end.y);
    }

    const int dx = end.x - start.x;
    const int dy = std::abs(end.y - start.y);
    int error = dx / 2;
    const int ystep = (start.y < end.y) ? 1 : -1;
    int y = start.y;

    for (int x = start.x; x <= end.x; ++x) {
        if (steep) write_pixel(bytes, stride, w, h, y, x, color);
        else write_pixel(bytes, stride, w, h, x, y, color);

        error -= dy;
        if (error < 0) {
            y += ystep;
            error += dx;
        }
    }
}

/// \brief transform_partner_tiny_raster.
int transform_partner_tiny_raster(
    snatch_font* font,
    const snatch_kv* /*options*/,
    unsigned /*options_count*/,
    char* errbuf,
    unsigned errbuf_len
) {
    if (!font || !font->user_data) {
        plugin_set_err(errbuf, errbuf_len, "partner_tiny_raster_transform: missing input user_data");
        return 30;
    }

    const auto* bin = static_cast<const snatch_partner_tiny_bin_data*>(font->user_data);
    if (bin->magic != SNATCH_PARTNER_TINY_BIN_MAGIC || bin->version != SNATCH_PARTNER_TINY_BIN_VERSION ||
        !bin->bytes || bin->size < 5) {
        plugin_set_err(errbuf, errbuf_len, "partner_tiny_raster_transform: invalid partner tiny bin payload");
        return 31;
    }

    const auto* bytes = bin->bytes;
    const std::size_t size = bin->size;
    const int max_w = static_cast<int>(bytes[1]) + 1;
    const int max_h = static_cast<int>(bytes[2]) + 1;
    const int first = static_cast<int>(bytes[3]);
    const int last = static_cast<int>(bytes[4]);
    if (last < first) {
        plugin_set_err(errbuf, errbuf_len, "partner_tiny_raster_transform: invalid codepoint range in tiny bin");
        return 32;
    }
    const std::size_t glyph_count = static_cast<std::size_t>(last - first + 1);
    const std::size_t offsets_bytes = glyph_count * 2u;
    if (size < 5u + offsets_bytes) {
        plugin_set_err(errbuf, errbuf_len, "partner_tiny_raster_transform: truncated tiny bin offset table");
        return 33;
    }

    g_owner = {};
    g_owner.glyphs.reserve(glyph_count);

    for (std::size_t i = 0; i < glyph_count; ++i) {
        const std::size_t off_pos = 5u + i * 2u;
        const std::uint16_t off = read_u16le(bytes + off_pos);
        if (off + 4u > size) {
            plugin_set_err(errbuf, errbuf_len, "partner_tiny_raster_transform: invalid glyph offset");
            return 34;
        }

        const std::uint8_t width_minus_one = bytes[off + 1u];
        const std::uint8_t height_minus_one = bytes[off + 2u];
        const std::uint8_t moves_count = bytes[off + 3u];
        const int gw = static_cast<int>(width_minus_one) + 1;
        const int gh = static_cast<int>(height_minus_one) + 1;
        if (gw <= 0 || gh <= 0) {
            plugin_set_err(errbuf, errbuf_len, "partner_tiny_raster_transform: invalid glyph dimensions");
            return 35;
        }

        const int stride = (gw + 7) / 8;
        glyph_owner g{};
        g.bytes.assign(static_cast<std::size_t>(stride * gh), 0);

        std::size_t cursor_pos = off + 4u;
        point_i cursor{0, 0};
        if (moves_count > 0) {
            if (cursor_pos + 2u + moves_count > size) {
                plugin_set_err(errbuf, errbuf_len, "partner_tiny_raster_transform: truncated glyph move data");
                return 36;
            }
            cursor.x = static_cast<int>(bytes[cursor_pos]);
            cursor.y = static_cast<int>(bytes[cursor_pos + 1u]);
            cursor_pos += 2u;

            for (std::uint8_t m = 0; m < moves_count; ++m) {
                const std::uint8_t mv = bytes[cursor_pos + m];
                const int dx = static_cast<int>((mv >> 5u) & 0x03u);
                const int dy = static_cast<int>((mv >> 3u) & 0x03u);
                int sx = static_cast<int>((mv >> 1u) & 0x01u);
                int sy = static_cast<int>((mv >> 2u) & 0x01u);
                sx = (sx == 1) ? -1 : 1;
                sy = (sy == 1) ? -1 : 1;
                const std::uint8_t color = static_cast<std::uint8_t>(((mv >> 7u) & 0x01u) | ((mv << 1u) & 0x02u));

                point_i end{cursor.x + sx * dx, cursor.y + sy * dy};
                if (color == 1 || color == 2 || color == 3) {
                    draw_line(g.bytes, stride, gw, gh, cursor, end, color);
                }
                cursor = end;
            }
        }

        g.view.codepoint = first + static_cast<int>(i);
        g.view.width = gw;
        g.view.height = gh;
        g.view.bearing_x = 0;
        g.view.bearing_y = gh;
        g.view.advance_x = gw;
        g.view.stride_bytes = stride;
        g.view.data = g.bytes.data();

        g_owner.glyphs.push_back(std::move(g));
    }

    g_owner.glyph_views.clear();
    g_owner.glyph_views.reserve(g_owner.glyphs.size());
    for (auto& g : g_owner.glyphs) {
        g.view.data = g.bytes.data();
        g_owner.glyph_views.push_back(g.view);
    }

    g_owner.bitmap_view.glyph_count = static_cast<int>(g_owner.glyph_views.size());
    g_owner.bitmap_view.glyphs = g_owner.glyph_views.data();

    font->glyph_width = std::max(1, max_w);
    font->glyph_height = std::max(1, max_h);
    font->first_codepoint = first;
    font->last_codepoint = last;
    font->pixel_size = 0;
    font->bitmap_font = &g_owner.bitmap_view;
    return 0;
}

const snatch_plugin_info k_info = {
    "partner_tiny_raster_transform",
    "Interprets Partner Tiny binary stream and rebuilds bitmap glyphs",
    "snatch project",
    "bin",
    "partner-tiny-raster",
    SNATCH_PLUGIN_ABI_VERSION,
    SNATCH_PLUGIN_KIND_TRANSFORMER,
    &transform_partner_tiny_raster,
    nullptr,
    nullptr
};

} // namespace

extern "C" SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out) {
    if (!out) return 1;
    *out = &k_info;
    return 0;
}
