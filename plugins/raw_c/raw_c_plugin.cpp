/// \file
/// \brief Raw C array exporter plugin implementation.
///
/// This source file implements one part of the snatch pipeline architecture. It contributes to extracting, transforming, exporting, or orchestrating bitmap data in a plugin-driven workflow.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include "snatch_plugins/partner_bitmap_transform.h"
#include "snatch/plugin.h"
#include "snatch/plugin_util.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
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

/// \brief sanitize_c_ident.
std::string sanitize_c_ident(std::string value) {
    if (value.empty()) return "font";
    for (char& c : value) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (!(std::isalnum(u) || c == '_')) c = '_';
    }
    const unsigned char first = static_cast<unsigned char>(value.front());
    if (!(std::isalpha(first) || value.front() == '_')) value.insert(value.begin(), '_');
    return value;
}

/// \brief parse_positive_int.
std::optional<int> parse_positive_int(std::optional<std::string_view> raw, int fallback) {
    if (!raw || raw->empty()) return fallback;
    const auto parsed = plugin_parse_int(*raw);
    if (!parsed || *parsed <= 0) return std::nullopt;
    return *parsed;
}

/// \brief export_raw_c.
int export_raw_c(
    const snatch_font* font,
    const char* output_path,
    const snatch_kv* options,
    unsigned options_count,
    char* errbuf,
    unsigned errbuf_len
) {
    if (!output_path || output_path[0] == '\0') {
        plugin_set_err(errbuf, errbuf_len, "raw_c: output path is empty");
        return 11;
    }

    const plugin_kv_view kv{options, options_count};

    const auto bytes_per_line = parse_positive_int(kv.get("bytes_per_line"), 8);
    if (!bytes_per_line || *bytes_per_line > 1024) {
        plugin_set_err(errbuf, errbuf_len, "raw_c: bytes_per_line must be in range 1..1024");
        return 14;
    }

    std::vector<std::uint8_t> packed;
    if (const auto* partner = partner_data_from_user_data(font)) {
        packed.assign(partner->bytes, partner->bytes + partner->size);
    } else {
        if (!font || !font->bitmap_font || !font->bitmap_font->glyphs) {
            plugin_set_err(errbuf, errbuf_len, "raw_c: bitmap font data missing");
            return 10;
        }

        const auto bytes_per_row = parse_positive_int(kv.get("bytes_per_row"), (std::max(font->glyph_width, 1) + 7) / 8);
        if (!bytes_per_row || *bytes_per_row > 1024) {
            plugin_set_err(errbuf, errbuf_len, "raw_c: bytes_per_row must be in range 1..1024");
            return 12;
        }

        const auto rows = parse_positive_int(kv.get("rows"), std::max(font->glyph_height, 1));
        if (!rows || *rows > 1024) {
            plugin_set_err(errbuf, errbuf_len, "raw_c: rows must be in range 1..1024");
            return 13;
        }

        const int first = font->first_codepoint;
        const int last = font->last_codepoint;
        if (first < 0 || last < first || last > 0x10FFFF) {
            plugin_set_err(errbuf, errbuf_len, "raw_c: invalid codepoint range");
            return 15;
        }

        const int glyph_count = last - first + 1;
        const std::size_t glyph_bytes = static_cast<std::size_t>(*bytes_per_row) * static_cast<std::size_t>(*rows);
        const std::size_t total_bytes = static_cast<std::size_t>(glyph_count) * glyph_bytes;
        packed.assign(total_bytes, 0);

        const snatch_bitmap_font& bf = *font->bitmap_font;
        const int max_width_bits = *bytes_per_row * 8;

        for (int cp = first; cp <= last; ++cp) {
            const std::size_t glyph_index = static_cast<std::size_t>(cp - first);
            const std::size_t glyph_base = glyph_index * glyph_bytes;
            const snatch_glyph_bitmap* glyph = find_glyph_by_codepoint(bf, cp);
            if (!glyph || !glyph->data || glyph->stride_bytes <= 0) continue;

            const int rows_to_copy = std::min(*rows, glyph->height);
            const int cols_to_copy = std::min(max_width_bits, glyph->width);
            for (int y = 0; y < rows_to_copy; ++y) {
                const auto* src_row = glyph->data + static_cast<std::size_t>(y * glyph->stride_bytes);
                auto* dst_row = packed.data() + glyph_base + static_cast<std::size_t>(y * (*bytes_per_row));
                for (int x = 0; x < cols_to_copy; ++x) {
                    if (!bit_is_set(src_row, x)) continue;
                    const int byte_index = x / 8;
                    const int bit_index = 7 - (x % 8);
                    dst_row[byte_index] = static_cast<std::uint8_t>(dst_row[byte_index] | (1u << bit_index));
                }
            }
        }
    }

    std::filesystem::path out_path{output_path};
    std::string symbol = sanitize_c_ident(out_path.stem().string());
    if (const auto v = kv.get("symbol"); v && !v->empty()) {
        symbol = sanitize_c_ident(std::string(*v));
    }

    const bool include_stdint = plugin_parse_bool(kv.get("include_stdint"), true);
    const bool use_hex_prefix = plugin_parse_bool(kv.get("hex_prefix"), true);
    const bool uppercase_hex = plugin_parse_bool(kv.get("uppercase_hex"), false);

    std::ostringstream text;
    text << "// " << out_path.filename().string() << "\n";
    text << "// .bin raw binary rendered as C array.\n";
    text << "//\n";
    text << "// Format is .bin, size (in bytes) is " << packed.size() << ".\n";
    if (include_stdint) text << "#include <stdint.h>\n\n";
    text << "const uint8_t " << symbol << "[] = {\n";

    for (std::size_t i = 0; i < packed.size(); ++i) {
        if (i % static_cast<std::size_t>(*bytes_per_line) == 0) {
            text << "    ";
        }
        text << (use_hex_prefix ? "0x" : "");
        std::ostringstream h;
        if (uppercase_hex) h.setf(std::ios::uppercase);
        h << std::hex;
        h.width(2);
        h.fill('0');
        h << static_cast<unsigned>(packed[i]);
        text << h.str();
        if (i + 1 < packed.size()) text << ", ";
        if ((i + 1) % static_cast<std::size_t>(*bytes_per_line) == 0) text << '\n';
    }
    if (packed.size() % static_cast<std::size_t>(*bytes_per_line) != 0) text << '\n';
    text << "};\n";

    std::ofstream out{output_path, std::ios::out | std::ios::trunc};
    if (!out.is_open()) {
        plugin_set_err(errbuf, errbuf_len, "raw_c: cannot open output file");
        return 16;
    }
    out << text.str();
    if (!out.good()) {
        plugin_set_err(errbuf, errbuf_len, "raw_c: failed while writing output");
        return 17;
    }
    return 0;
}

const snatch_plugin_info k_info = {
    "raw_c",
    "Exports raw bytes as a C uint8_t array (raw bitmap or transformer-provided stream)",
    "snatch project",
    "c",
    "raw-1bpp",
    SNATCH_PLUGIN_ABI_VERSION,
    SNATCH_PLUGIN_KIND_EXPORTER,
    nullptr,
    &export_raw_c
};

} // namespace

extern "C" SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out) {
    if (!out) return 1;
    *out = &k_info;
    return 0;
}
