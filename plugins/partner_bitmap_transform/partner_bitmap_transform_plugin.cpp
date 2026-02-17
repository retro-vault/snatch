/// \file
/// \brief Partner bitmap transformer plugin implementation.
///
/// This source file implements one part of the snatch pipeline architecture. It contributes to extracting, transforming, exporting, or orchestrating bitmap data in a plugin-driven workflow.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include "snatch_plugins/partner_bitmap_transform.h"
#include "snatch/plugin.h"
#include "snatch/plugin_util.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

struct glyph_blob {
    std::uint8_t width{0};
    std::uint8_t height{0};
    std::vector<std::uint8_t> payload;
};

struct partner_bitmap_owner {
    snatch_partner_bitmap_data view{};
    std::vector<std::uint8_t> bytes;
};

partner_bitmap_owner g_owner;

/// \brief bit_is_set.
bool bit_is_set(const unsigned char* row, int x) {
    const int byte_index = x / 8;
    const int bit_index = 7 - (x % 8);
    return (row[byte_index] & (1u << bit_index)) != 0;
}

/// \brief find_glyph_by_codepoint.
const snatch_glyph_bitmap* find_glyph_by_codepoint(const snatch_bitmap_font& bf, int codepoint) {
    for (int i = 0; i < bf.glyph_count; ++i) {
        if (bf.glyphs[i].codepoint == codepoint) return &bf.glyphs[i];
    }
    return nullptr;
}

/// \brief append_u16_le.
void append_u16_le(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xFFu));
}

/// \brief pack_glyph_rows.
glyph_blob pack_glyph_rows(
    const snatch_glyph_bitmap* glyph,
    int cell_width,
    int cell_height,
    int max_bearing_y
) {
    glyph_blob out{};
    out.width = static_cast<std::uint8_t>(std::clamp(cell_width, 0, 255));
    out.height = static_cast<std::uint8_t>(std::clamp(cell_height, 0, 255));

    const int bytes_per_row = (cell_width + 7) / 8;
    if (bytes_per_row <= 0 || cell_height <= 0) return out;

    out.payload.assign(
        static_cast<std::size_t>(bytes_per_row) * static_cast<std::size_t>(cell_height),
        0
    );

    if (!glyph || !glyph->data || glyph->width <= 0 || glyph->height <= 0 || glyph->stride_bytes <= 0) {
        return out;
    }

    const int y_offset = max_bearing_y - glyph->bearing_y;
    for (int y = 0; y < glyph->height; ++y) {
        const int dst_y = y + y_offset;
        if (dst_y < 0 || dst_y >= cell_height) continue;

        const auto* src_row = glyph->data + static_cast<std::size_t>(y * glyph->stride_bytes);
        for (int x = 0; x < glyph->width && x < cell_width; ++x) {
            if (!bit_is_set(src_row, x)) continue;
            const int byte_index = x / 8;
            const int bit_index = 7 - (x % 8);
            auto& dst = out.payload[static_cast<std::size_t>(dst_y * bytes_per_row + byte_index)];
            dst = static_cast<std::uint8_t>(dst | (1u << bit_index));
        }
    }

    return out;
}

/// \brief partner_bitmap_transform.
int partner_bitmap_transform(
    snatch_font* font,
    const snatch_kv* options,
    unsigned options_count,
    char* errbuf,
    unsigned errbuf_len
) {
    if (!font || !font->bitmap_font || !font->bitmap_font->glyphs) {
        plugin_set_err(errbuf, errbuf_len, "partner_bitmap_transform: bitmap font data missing");
        return 30;
    }

    const int first_ascii = font->first_codepoint;
    const int last_ascii = font->last_codepoint;
    if (first_ascii < 0 || last_ascii < first_ascii || last_ascii > 255) {
        plugin_set_err(errbuf, errbuf_len, "partner_bitmap_transform: invalid codepoint range");
        return 31;
    }

    const plugin_kv_view kv{options, options_count};

    int letter_spacing = 0;
    if (const auto raw = kv.get("letter_spacing"); raw && !raw->empty()) {
        const auto parsed = plugin_parse_int(*raw);
        if (!parsed || *parsed < 0 || *parsed > 15) {
            plugin_set_err(errbuf, errbuf_len, "partner_bitmap_transform: letter_spacing must be 0..15");
            return 32;
        }
        letter_spacing = *parsed;
    } else if (const auto raw = kv.get("spacing_hint"); raw && !raw->empty()) {
        const auto parsed = plugin_parse_int(*raw);
        if (!parsed || *parsed < 0 || *parsed > 15) {
            plugin_set_err(errbuf, errbuf_len, "partner_bitmap_transform: spacing_hint must be 0..15");
            return 32;
        }
        letter_spacing = *parsed;
    }

    bool proportional = false;
    if (const auto mode = kv.get("font_mode"); mode && !mode->empty()) {
        if (*mode == "proportional") proportional = true;
        if (*mode == "fixed") proportional = false;
    }
    proportional = plugin_parse_bool(kv.get("proportional"), proportional);

    int space_width = 0;
    bool has_space_width = false;
    if (const auto raw = kv.get("space_width"); raw && !raw->empty()) {
        const auto parsed = plugin_parse_int(*raw);
        if (!parsed || *parsed < 0 || *parsed > 7) {
            plugin_set_err(errbuf, errbuf_len, "partner_bitmap_transform: space_width must be 0..7");
            return 33;
        }
        space_width = *parsed;
        has_space_width = true;
    }
    if (proportional && !has_space_width) {
        plugin_set_err(errbuf, errbuf_len, "partner_bitmap_transform: space_width is required when proportional=true");
        return 34;
    }

    const std::uint8_t flags = static_cast<std::uint8_t>(
        (proportional ? 0x80 : 0x00) |
        ((space_width & 0x07) << 4u) |
        (letter_spacing & 0x0F)
    );

    const snatch_bitmap_font& bf = *font->bitmap_font;
    std::vector<const snatch_glyph_bitmap*> glyph_ptrs;
    glyph_ptrs.reserve(static_cast<std::size_t>(last_ascii - first_ascii + 1));

    int max_w = 0;
    int max_bearing_y = 0;
    int min_descender = 0;
    for (int cp = first_ascii; cp <= last_ascii; ++cp) {
        const auto* g = find_glyph_by_codepoint(bf, cp);
        glyph_ptrs.push_back(g);
        if (!g) continue;
        max_w = std::max(max_w, g->width);
        max_bearing_y = std::max(max_bearing_y, g->bearing_y);
        min_descender = std::min(min_descender, g->bearing_y - g->height);
    }
    const int max_h = std::max(1, max_bearing_y - min_descender);
    const int fixed_cell_width = std::max(1, max_w);

    std::vector<glyph_blob> glyphs;
    glyphs.reserve(glyph_ptrs.size());
    for (const auto* g : glyph_ptrs) {
        const int cell_width = proportional ? std::max(0, g ? g->width : 0) : fixed_cell_width;
        glyphs.push_back(pack_glyph_rows(g, cell_width, max_h, max_bearing_y));
        if (glyphs.back().payload.size() > 255) {
            plugin_set_err(errbuf, errbuf_len, "partner_bitmap_transform: glyph payload too large for Partner format");
            return 35;
        }
    }

    std::vector<std::uint16_t> offsets;
    offsets.reserve(glyphs.size());
    std::uint32_t offset = 5u + static_cast<std::uint32_t>(glyphs.size() * 2u);
    for (const auto& glyph : glyphs) {
        if (offset > 0xFFFFu) {
            plugin_set_err(errbuf, errbuf_len, "partner_bitmap_transform: serialized font too large (>64KiB)");
            return 36;
        }
        offsets.push_back(static_cast<std::uint16_t>(offset));
        offset += 4u + static_cast<std::uint32_t>(glyph.payload.size());
    }

    g_owner = {};
    g_owner.bytes.reserve(static_cast<std::size_t>(offset));

    g_owner.bytes.push_back(flags);
    g_owner.bytes.push_back(static_cast<std::uint8_t>(std::clamp(max_w, 0, 255)));
    g_owner.bytes.push_back(static_cast<std::uint8_t>(std::clamp(max_h, 0, 255)));
    g_owner.bytes.push_back(static_cast<std::uint8_t>(first_ascii));
    g_owner.bytes.push_back(static_cast<std::uint8_t>(last_ascii));

    for (const std::uint16_t off : offsets) {
        append_u16_le(g_owner.bytes, off);
    }

    for (const auto& glyph : glyphs) {
        g_owner.bytes.push_back(0); // class(bits 5-7) for bitmap
        g_owner.bytes.push_back(glyph.width);
        g_owner.bytes.push_back(glyph.height);
        g_owner.bytes.push_back(static_cast<std::uint8_t>(glyph.payload.size()));
        g_owner.bytes.insert(g_owner.bytes.end(), glyph.payload.begin(), glyph.payload.end());
    }

    g_owner.view.magic = SNATCH_PARTNER_BITMAP_MAGIC;
    g_owner.view.version = SNATCH_PARTNER_BITMAP_VERSION;
    g_owner.view.bytes = g_owner.bytes.empty() ? nullptr : g_owner.bytes.data();
    g_owner.view.size = static_cast<std::uint32_t>(g_owner.bytes.size());
    font->user_data = &g_owner.view;
    return 0;
}

const snatch_plugin_info k_info = {
    "partner_bitmap_transform",
    "Serializes bitmap glyphs to Partner binary stream in font->user_data",
    "snatch project",
    "bitmap",
    "partner-b",
    SNATCH_PLUGIN_ABI_VERSION,
    SNATCH_PLUGIN_KIND_TRANSFORMER,
    &partner_bitmap_transform,
    nullptr
};

} // namespace

extern "C" SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out) {
    if (!out) return 1;
    *out = &k_info;
    return 0;
}
