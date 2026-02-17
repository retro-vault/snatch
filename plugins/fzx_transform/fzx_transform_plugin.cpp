/// \file
/// \brief FZX metadata transformer plugin implementation.
///
/// This source file implements one part of the snatch pipeline architecture. It contributes to extracting, transforming, exporting, or orchestrating bitmap data in a plugin-driven workflow.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include "snatch/plugin.h"
#include "snatch/plugin_util.h"
#include "snatch/glyph_algorithms.h"
#include "snatch_plugins/fzx_transform.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

struct fzx_transform_owner {
    snatch_fzx_transform_data view{};
    std::vector<snatch_fzx_glyph_info> glyphs;
};

static fzx_transform_owner g_owner;

/// \brief fzx_transform.
int fzx_transform(
    snatch_font* font,
    const snatch_kv* options,
    unsigned options_count,
    char* errbuf,
    unsigned errbuf_len
) {
    const plugin_kv_view kv{options, options_count};

    if (!font || !font->bitmap_font || !font->bitmap_font->glyphs) {
        plugin_set_err(errbuf, errbuf_len, "fzx-transform: bitmap font data missing");
        return 20;
    }

    const snatch_bitmap_font& bf = *font->bitmap_font;
    if (bf.glyph_count <= 0) {
        plugin_set_err(errbuf, errbuf_len, "fzx-transform: no glyphs in font");
        return 21;
    }

    int tracking = 1;
    if (const auto raw = kv.get("tracking"); raw && !raw->empty()) {
        const auto parsed = plugin_parse_int(*raw);
        if (!parsed || *parsed < -128 || *parsed > 127) {
            plugin_set_err(errbuf, errbuf_len, "fzx-transform: invalid tracking (expected -128..127)");
            return 22;
        }
        tracking = *parsed;
    }

    int explicit_height = -1;
    if (const auto raw = kv.get("height"); raw && !raw->empty()) {
        const auto parsed = plugin_parse_int(*raw);
        if (!parsed || *parsed < 1 || *parsed > 255) {
            plugin_set_err(errbuf, errbuf_len, "fzx-transform: invalid height (expected 1..255)");
            return 23;
        }
        explicit_height = *parsed;
    }

    const bool strict = plugin_parse_bool(kv.get("strict"), true);

    if (font->first_codepoint < 32 || font->last_codepoint > 255) {
        plugin_set_err(errbuf, errbuf_len, "fzx-transform: FZX supports codepoints 32..255");
        return 24;
    }

    const int first = 32;
    const int last = font->last_codepoint;
    const int table_count = last - first + 1;
    if (table_count <= 0 || table_count > 224) {
        plugin_set_err(errbuf, errbuf_len, "fzx-transform: invalid FZX table size");
        return 25;
    }

    g_owner = {};
    g_owner.glyphs.resize(static_cast<size_t>(table_count));
    for (int i = 0; i < table_count; ++i) {
        g_owner.glyphs[static_cast<size_t>(i)].codepoint = first + i;
    }

    int derived_height = std::max(1, font->glyph_height);
    for (int i = 0; i < bf.glyph_count; ++i) {
        const snatch_glyph_bitmap& g = bf.glyphs[i];
        const int cp = g.codepoint;
        if (cp < first || cp > last) continue;

        glyph_bounds b = glyph_bitmap_analyzer::bounds(g);
        snatch_fzx_glyph_info& out = g_owner.glyphs[static_cast<size_t>(cp - first)];
        out.codepoint = cp;

        if (b.empty) {
            out.empty = 1;
            out.kern = 0;
            out.shift = 0;
            out.width = 1;
            out.depth = 0;
            out.left = 0;
            out.right = 0;
            out.top = 0;
            out.bottom = 0;
            continue;
        }

        int width = b.right - b.left + 1;
        if (width > 16) {
            if (strict) {
                plugin_set_err(errbuf, errbuf_len, "fzx-transform: glyph width exceeds FZX max 16");
                return 26;
            }
            width = 16;
        }

        int shift = b.top;
        if (shift > 15) shift = 15;
        int depth = b.bottom - shift + 1;
        if (depth > 192) {
            if (strict) {
                plugin_set_err(errbuf, errbuf_len, "fzx-transform: glyph depth exceeds FZX max 192");
                return 27;
            }
            depth = 192;
        }

        int kern = std::clamp(b.left, 0, 3);

        out.empty = 0;
        out.kern = static_cast<std::uint8_t>(kern);
        out.shift = static_cast<std::uint8_t>(shift);
        out.width = static_cast<std::uint8_t>(std::max(1, width));
        out.depth = static_cast<std::uint8_t>(std::max(0, depth));
        out.left = static_cast<std::uint8_t>(std::clamp(b.left, 0, 255));
        out.right = static_cast<std::uint8_t>(std::clamp(b.right, 0, 255));
        out.top = static_cast<std::uint8_t>(std::clamp(b.top, 0, 255));
        out.bottom = static_cast<std::uint8_t>(std::clamp(b.bottom, 0, 255));
        out.offset_hint = 0;
    }

    const int header_height = (explicit_height > 0) ? explicit_height : std::clamp(derived_height, 1, 255);
    g_owner.view.height = static_cast<std::uint8_t>(header_height);
    g_owner.view.tracking = static_cast<std::int8_t>(tracking);
    g_owner.view.lastchar = static_cast<std::uint8_t>(last);
    g_owner.view.glyph_count = static_cast<std::uint16_t>(g_owner.glyphs.size());
    g_owner.view.glyphs = g_owner.glyphs.data();

    font->user_data = &g_owner.view;
    return 0;
}

const snatch_plugin_info k_info = {
    "fzx-transform",
    "Builds FZX-style glyph metadata (kern/shift/width/depth) into font->user_data",
    "snatch project",
    "bitmap",
    "zx-fzx",
    SNATCH_PLUGIN_ABI_VERSION,
    SNATCH_PLUGIN_KIND_TRANSFORMER,
    &fzx_transform,
    nullptr
};

} // namespace

extern "C" SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out) {
    if (!out) return 1;
    *out = &k_info;
    return 0;
}
