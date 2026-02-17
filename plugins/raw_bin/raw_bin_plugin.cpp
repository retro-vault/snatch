/// \file
/// \brief Raw binary exporter plugin implementation.
///
/// This source file implements one part of the snatch pipeline architecture. It contributes to extracting, transforming, exporting, or orchestrating bitmap data in a plugin-driven workflow.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include "snatch_plugins/partner_bitmap_transform.h"
#include "snatch_plugins/partner_tiny_transform.h"
#include "snatch/plugin.h"
#include "snatch/plugin_util.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <vector>

namespace {

/// \brief partner_data_from_user_data.
const snatch_partner_bitmap_data* partner_data_from_user_data(const snatch_font* font) {
    if (!font || !font->user_data) return nullptr;
    const auto* data = static_cast<const snatch_partner_bitmap_data*>(font->user_data);
    if (data->magic != SNATCH_PARTNER_BITMAP_MAGIC || data->version != SNATCH_PARTNER_BITMAP_VERSION) return nullptr;
    if (!data->bytes || data->size == 0) return nullptr;
    return data;
}

/// \brief partner_tiny_data_from_user_data.
const snatch_partner_tiny_data* partner_tiny_data_from_user_data(const snatch_font* font) {
    if (!font || !font->user_data) return nullptr;
    const auto* data = static_cast<const snatch_partner_tiny_data*>(font->user_data);
    if (data->magic != SNATCH_PARTNER_TINY_MAGIC || data->version != SNATCH_PARTNER_TINY_VERSION) return nullptr;
    if (!data->glyphs || data->glyph_count == 0) return nullptr;
    return data;
}

/// \brief serialize_partner_tiny.
std::vector<std::uint8_t> serialize_partner_tiny(
    const snatch_font* font,
    const snatch_partner_tiny_data* tiny,
    const plugin_kv_view& kv,
    char* errbuf,
    unsigned errbuf_len
) {
    std::vector<std::uint8_t> out;
    if (!font || !tiny) return out;

    if (font->first_codepoint < 0 || font->last_codepoint < font->first_codepoint || font->last_codepoint > 255) {
        plugin_set_err(errbuf, errbuf_len, "raw_bin: invalid codepoint range for partner tiny stream");
        return {};
    }

    bool proportional = false;
    if (const auto mode = kv.get("font_mode"); mode && !mode->empty()) {
        if (*mode == "proportional") proportional = true;
        if (*mode == "fixed") proportional = false;
    }
    proportional = plugin_parse_bool(kv.get("proportional"), proportional);

    int letter_spacing = 0;
    if (const auto raw = kv.get("letter_spacing"); raw && !raw->empty()) {
        const auto parsed = plugin_parse_int(*raw);
        if (!parsed || *parsed < 0 || *parsed > 15) {
            plugin_set_err(errbuf, errbuf_len, "raw_bin: letter_spacing must be 0..15");
            return {};
        }
        letter_spacing = *parsed;
    }

    int space_width = 0;
    if (const auto raw = kv.get("space_width"); raw && !raw->empty()) {
        const auto parsed = plugin_parse_int(*raw);
        if (!parsed || *parsed < 0 || *parsed > 7) {
            plugin_set_err(errbuf, errbuf_len, "raw_bin: space_width must be 0..7");
            return {};
        }
        space_width = *parsed;
    }

    const std::uint8_t flags = static_cast<std::uint8_t>(
        (proportional ? 0x80 : 0x00) | ((space_width & 0x07) << 4u) | (letter_spacing & 0x0F)
    );

    const int first = font->first_codepoint;
    const int last = font->last_codepoint;
    const std::size_t glyph_count = static_cast<std::size_t>(last - first + 1);
    if (glyph_count != tiny->glyph_count) {
        plugin_set_err(errbuf, errbuf_len, "raw_bin: transformed tiny glyph count does not match codepoint range");
        return {};
    }

    out.reserve(5u + glyph_count * 2u);
    out.push_back(flags);
    out.push_back(static_cast<std::uint8_t>(tiny->max_width_minus_one));
    out.push_back(static_cast<std::uint8_t>(tiny->max_height_minus_one));
    out.push_back(static_cast<std::uint8_t>(first));
    out.push_back(static_cast<std::uint8_t>(last));

    std::vector<std::uint16_t> offsets;
    offsets.reserve(glyph_count);
    std::uint32_t offset = 5u + static_cast<std::uint32_t>(glyph_count * 2u);
    for (std::size_t i = 0; i < glyph_count; ++i) {
        const auto& g = tiny->glyphs[i];
        if (offset > 0xFFFFu) {
            plugin_set_err(errbuf, errbuf_len, "raw_bin: partner tiny stream too large (>64KiB)");
            return {};
        }
        offsets.push_back(static_cast<std::uint16_t>(offset));
        offset += 4u + static_cast<std::uint32_t>(g.data_size);
    }

    for (const auto off : offsets) {
        out.push_back(static_cast<std::uint8_t>(off & 0xFFu));
        out.push_back(static_cast<std::uint8_t>((off >> 8u) & 0xFFu));
    }

    constexpr std::uint8_t kGlyphClassTinyBits = 1u << 5u;
    for (std::size_t i = 0; i < glyph_count; ++i) {
        const auto& g = tiny->glyphs[i];
        if (g.data_size > 257u) {
            plugin_set_err(errbuf, errbuf_len, "raw_bin: partner tiny glyph has more than 255 moves");
            return {};
        }
        out.push_back(kGlyphClassTinyBits);
        out.push_back(g.width_minus_one);
        out.push_back(g.height_minus_one);
        const std::uint8_t move_count = static_cast<std::uint8_t>(g.data_size >= 2 ? (g.data_size - 2) : 0);
        out.push_back(move_count);
        if (g.data_size > 0 && g.data) {
            out.insert(out.end(), g.data, g.data + g.data_size);
        }
    }

    return out;
}

/// \brief find_glyph_by_codepoint.
const snatch_glyph_bitmap* find_glyph_by_codepoint(const snatch_bitmap_font& bf, int codepoint) {
    for (int i = 0; i < bf.glyph_count; ++i) {
        if (bf.glyphs[i].codepoint == codepoint) return &bf.glyphs[i];
    }
    return nullptr;
}

/// \brief export_raw_bin.
int export_raw_bin(
    const snatch_font* font,
    const char* output_path,
    const snatch_kv* options,
    unsigned options_count,
    char* errbuf,
    unsigned errbuf_len
) {
    if (!output_path || output_path[0] == '\0') {
        plugin_set_err(errbuf, errbuf_len, "raw_bin: output path is empty");
        return 11;
    }

    const plugin_kv_view kv{options, options_count};

    std::vector<std::uint8_t> packed;
    if (const auto* tiny = partner_tiny_data_from_user_data(font)) {
        packed = serialize_partner_tiny(font, tiny, kv, errbuf, errbuf_len);
        if (!packed.empty() || (errbuf && errbuf[0] != '\0')) {
            if (packed.empty()) return 15;
        }
    } else if (const auto* partner = partner_data_from_user_data(font)) {
        packed.assign(partner->bytes, partner->bytes + partner->size);
    } else {
        if (!font || !font->bitmap_font || !font->bitmap_font->glyphs) {
            plugin_set_err(errbuf, errbuf_len, "raw_bin: bitmap font data missing");
            return 10;
        }

        const int first = font->first_codepoint;
        const int last = font->last_codepoint;
        if (first < 0 || last < first || last > 0x10FFFF) {
            plugin_set_err(errbuf, errbuf_len, "raw_bin: invalid codepoint range");
            return 12;
        }

        const snatch_bitmap_font& bf = *font->bitmap_font;
        packed.reserve(static_cast<std::size_t>(bf.glyph_count) * static_cast<std::size_t>(std::max(font->glyph_height, 1)));

        for (int cp = first; cp <= last; ++cp) {
            const snatch_glyph_bitmap* glyph = find_glyph_by_codepoint(bf, cp);
            if (!glyph || !glyph->data || glyph->stride_bytes <= 0) continue;

            const int rows_to_copy = std::max(0, glyph->height);
            for (int y = 0; y < rows_to_copy; ++y) {
                const auto* src_row = glyph->data + static_cast<std::size_t>(y * glyph->stride_bytes);
                packed.insert(packed.end(), src_row, src_row + glyph->stride_bytes);
            }
        }
    }

    std::ofstream out{output_path, std::ios::binary | std::ios::trunc};
    if (!out.is_open()) {
        plugin_set_err(errbuf, errbuf_len, "raw_bin: cannot open output file");
        return 13;
    }

    out.write(reinterpret_cast<const char*>(packed.data()), static_cast<std::streamsize>(packed.size()));
    if (!out.good()) {
        plugin_set_err(errbuf, errbuf_len, "raw_bin: failed while writing output");
        return 14;
    }
    return 0;
}

const snatch_plugin_info k_info = {
    "raw_bin",
    "Exports continuous raw glyph bitmap bytes (.bin)",
    "snatch project",
    "bin",
    "raw-1bpp",
    SNATCH_PLUGIN_ABI_VERSION,
    SNATCH_PLUGIN_KIND_EXPORTER,
    nullptr,
    &export_raw_bin,
    nullptr
};

} // namespace

extern "C" SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out) {
    if (!out) return 1;
    *out = &k_info;
    return 0;
}
