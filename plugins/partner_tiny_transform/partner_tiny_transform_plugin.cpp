/// \file
/// \brief Partner Tiny vectorization transformer plugin implementation.
///
/// This source file implements one part of the snatch pipeline architecture. It contributes to extracting, transforming, exporting, or orchestrating bitmap data in a plugin-driven workflow.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include "snatch/glyph_algorithms.h"
#include "snatch_plugins/partner_tiny_transform.h"
#include "snatch/plugin.h"
#include "snatch/plugin_util.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

constexpr std::uint8_t kColorNone = 0;
constexpr std::uint8_t kColorFore = 1;

struct tiny_move {
    int dx{0};
    int dy{0};
    std::uint8_t color{kColorNone};
};

struct glyph_owner {
    snatch_partner_tiny_glyph view{};
    std::vector<std::uint8_t> bytes;
};

struct partner_tiny_owner {
    snatch_partner_tiny_data view{};
    std::vector<glyph_owner> glyphs;
    std::vector<snatch_partner_tiny_glyph> glyph_views;
};

static partner_tiny_owner g_owner;

/// \brief u8_clamp.
constexpr std::uint8_t u8_clamp(int v) {
    return static_cast<std::uint8_t>(std::clamp(v, 0, 255));
}

/// \brief encode_tiny_move.
std::uint8_t encode_tiny_move(const tiny_move& move) {
    const int dx = std::clamp(move.dx, -3, 3);
    const int dy = std::clamp(move.dy, -3, 3);
    const int adx = std::abs(dx);
    const int ady = std::abs(dy);

    const std::uint8_t sx = static_cast<std::uint8_t>(dx < 0 ? 1 : 0);
    const std::uint8_t sy = static_cast<std::uint8_t>(dy < 0 ? 1 : 0);
    // Partner tiny bit layout is: c0 dx dx dy dy sy sx c1
    // so color bit0 goes to bit7 (c0), color bit1 goes to bit0 (c1).
    const std::uint8_t c0 = static_cast<std::uint8_t>(move.color & 1u);
    const std::uint8_t c1 = static_cast<std::uint8_t>((move.color >> 1u) & 1u);

    std::uint8_t out = 0;
    out |= static_cast<std::uint8_t>(c0 << 7u);
    out |= static_cast<std::uint8_t>(adx << 5u);
    out |= static_cast<std::uint8_t>(ady << 3u);
    out |= static_cast<std::uint8_t>(sy << 2u);
    out |= static_cast<std::uint8_t>(sx << 1u);
    out |= c1;
    return out;
}

/// \brief append_none_steps.
void append_none_steps(std::vector<tiny_move>& out, int dx, int dy) {
    int rem_x = dx;
    int rem_y = dy;

    while (rem_x != 0 || rem_y != 0) {
        const int sx = rem_x == 0 ? 0 : (rem_x > 0 ? std::min(rem_x, 3) : std::max(rem_x, -3));
        const int sy = rem_y == 0 ? 0 : (rem_y > 0 ? std::min(rem_y, 3) : std::max(rem_y, -3));
        out.push_back({sx, sy, kColorNone});
        rem_x -= sx;
        rem_y -= sy;
    }
}

/// \brief vectorize_glyph.
std::vector<tiny_move> vectorize_glyph(
    const snatch_glyph_bitmap& glyph,
    bool optimize_route,
    int& origin_x,
    int& origin_y
) {
    std::vector<tiny_move> moves;
    std::vector<glyph_pixel> points = glyph_bitmap_analyzer::foreground_pixels(glyph, 1);
    if (points.empty()) return moves;

    if (optimize_route && points.size() >= 4) {
        glyph_route_optimizer optimizer;
        points = optimizer.tsp_2opt(points);
    }

    origin_x = points.front().x;
    origin_y = points.front().y;
    int cx = origin_x;
    int cy = origin_y;
    moves.push_back({0, 0, kColorFore}); // initial dot at origin

    for (std::size_t i = 1; i < points.size(); ++i) {
        const int tx = points[i].x;
        const int ty = points[i].y;
        const int dx = tx - cx;
        const int dy = ty - cy;
        // Keep reconstruction faithful: travel with color=none, then set exactly one pixel.
        append_none_steps(moves, dx, dy);
        moves.push_back({0, 0, kColorFore});
        cx = tx;
        cy = ty;
    }

    return moves;
}

/// \brief find_glyph_by_codepoint.
const snatch_glyph_bitmap* find_glyph_by_codepoint(const snatch_bitmap_font& bf, int codepoint) {
    for (int i = 0; i < bf.glyph_count; ++i) {
        const auto& glyph = bf.glyphs[i];
        if (glyph.codepoint == codepoint) return &glyph;
    }
    return nullptr;
}

/// \brief partner_tiny_transform.
int partner_tiny_transform(
    snatch_font* font,
    const snatch_kv* options,
    unsigned options_count,
    char* errbuf,
    unsigned errbuf_len
) {
    if (!font || !font->bitmap_font || !font->bitmap_font->glyphs) {
        plugin_set_err(errbuf, errbuf_len, "partner_tiny_transform: bitmap font data missing");
        return 30;
    }

    if (font->first_codepoint < 0 || font->last_codepoint < font->first_codepoint || font->last_codepoint > 255) {
        plugin_set_err(errbuf, errbuf_len, "partner_tiny_transform: invalid codepoint range");
        return 31;
    }

    const plugin_kv_view kv{options, options_count};
    const bool optimize_route = plugin_parse_bool(kv.get("optimize"), true);

    const snatch_bitmap_font& bf = *font->bitmap_font;
    const int first = font->first_codepoint;
    const int last = font->last_codepoint;

    g_owner = {};
    g_owner.glyphs.reserve(static_cast<std::size_t>(last - first + 1));

    int max_width = std::max(1, font->glyph_width);
    int max_height = std::max(1, font->glyph_height);

    for (int cp = first; cp <= last; ++cp) {
        glyph_owner owner{};
        owner.view.codepoint = static_cast<std::uint16_t>(cp);

        const snatch_glyph_bitmap* glyph = find_glyph_by_codepoint(bf, cp);
        const int gw = glyph ? std::max(1, glyph->width) : std::max(1, font->glyph_width);
        const int gh = glyph ? std::max(1, glyph->height) : std::max(1, font->glyph_height);

        max_width = std::max(max_width, gw);
        max_height = std::max(max_height, gh);

        owner.view.width_minus_one = u8_clamp(gw - 1);
        owner.view.height_minus_one = u8_clamp(gh - 1);

        if (glyph && glyph->data && glyph->width > 0 && glyph->height > 0) {
            int origin_x = 0;
            int origin_y = 0;
            const std::vector<tiny_move> tiny = vectorize_glyph(*glyph, optimize_route, origin_x, origin_y);
            if (!tiny.empty()) {
                if (tiny.size() > 255) {
                    plugin_set_err(errbuf, errbuf_len, "partner_tiny_transform: glyph has more than 255 moves");
                    return 32;
                }
                if (tiny.size() + 2 > 65535) {
                    plugin_set_err(errbuf, errbuf_len, "partner_tiny_transform: glyph payload too large");
                    return 33;
                }

                owner.bytes.reserve(tiny.size() + 2);
                owner.bytes.push_back(u8_clamp(origin_x));
                owner.bytes.push_back(u8_clamp(origin_y));
                for (const auto& move : tiny) {
                    owner.bytes.push_back(encode_tiny_move(move));
                }
            } 
        }

        owner.view.data_size = static_cast<std::uint16_t>(owner.bytes.size());
        owner.view.data = owner.bytes.empty() ? nullptr : owner.bytes.data();
        g_owner.glyphs.push_back(std::move(owner));
    }

    g_owner.glyph_views.clear();
    g_owner.glyph_views.reserve(g_owner.glyphs.size());
    for (auto& glyph : g_owner.glyphs) {
        glyph.view.data = glyph.bytes.empty() ? nullptr : glyph.bytes.data();
        g_owner.glyph_views.push_back(glyph.view);
    }

    g_owner.view.magic = SNATCH_PARTNER_TINY_MAGIC;
    g_owner.view.version = SNATCH_PARTNER_TINY_VERSION;
    g_owner.view.glyph_count = static_cast<std::uint16_t>(g_owner.glyph_views.size());
    g_owner.view.max_width_minus_one = u8_clamp(max_width - 1);
    g_owner.view.max_height_minus_one = u8_clamp(max_height - 1);
    g_owner.view.glyphs = g_owner.glyph_views.empty() ? nullptr : g_owner.glyph_views.data();

    font->user_data = &g_owner.view;
    return 0;
}

const snatch_plugin_info k_info = {
    "partner_tiny_transform",
    "Vectorizes bitmap glyphs into Partner Tiny move streams (font->user_data)",
    "snatch project",
    "bitmap",
    "partner-tiny",
    SNATCH_PLUGIN_ABI_VERSION,
    SNATCH_PLUGIN_KIND_TRANSFORMER,
    &partner_tiny_transform,
    nullptr,
    nullptr
};

} // namespace

extern "C" SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out) {
    if (!out) return 1;
    *out = &k_info;
    return 0;
}
