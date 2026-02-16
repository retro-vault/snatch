#include "snatch/plugin.h"
#include "snatch/plugin_util.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kIndent = "        ";
constexpr std::uint8_t kGlyphClassBitmap = 0;

struct glyph_blob {
    int codepoint{0};
    std::uint8_t width{0};
    std::uint8_t height{0};
    std::uint16_t payload_size{0};
    int bytes_per_row{0};
    std::vector<std::uint8_t> payload;
};

struct export_state {
    int code{0};
    std::string message;

    [[nodiscard]] bool ok() const { return code == 0; }
};

bool bit_is_set(const unsigned char* row, int x) {
    const int byte_index = x / 8;
    const int bit_index = 7 - (x % 8);
    return (row[byte_index] & (1u << bit_index)) != 0;
}

const snatch_glyph_bitmap* find_glyph_by_codepoint(const snatch_bitmap_font& bf, int codepoint) {
    for (int i = 0; i < bf.glyph_count; ++i) {
        if (bf.glyphs[i].codepoint == codepoint) return &bf.glyphs[i];
    }
    return nullptr;
}

std::string sanitize_symbol(std::string value) {
    if (value.empty()) return "snatch_font";
    for (char& ch : value) {
        const auto u = static_cast<unsigned char>(ch);
        if (!std::isalnum(u) && ch != '_') ch = '_';
    }
    const auto first = static_cast<unsigned char>(value.front());
    if (!std::isalpha(first) && value.front() != '_') value.insert(value.begin(), '_');
    return value;
}

std::string default_symbol_from_output(std::string_view output_path) {
    std::filesystem::path p{std::string(output_path)};
    std::string stem = p.stem().string();
    if (stem.empty()) stem = "snatch_font";
    return sanitize_symbol(std::move(stem));
}

std::string glyph_label_for_comment(int codepoint) {
    if (codepoint == 127) return "<non standard>";
    if (codepoint == 39) return "'''";
    if (codepoint >= 32 && codepoint <= 126) return std::string("'") + static_cast<char>(codepoint) + "'";
    return "'?'";
}

std::string to_bin8(std::uint8_t byte) {
    std::string out(8, '0');
    for (int i = 0; i < 8; ++i) {
        if (byte & static_cast<std::uint8_t>(1u << (7 - i))) out[static_cast<std::size_t>(i)] = '1';
    }
    return out;
}

void write_db_value(std::ostream& os, std::uint8_t value, std::string_view comment) {
    os << kIndent << ".db " << std::left << std::setw(20) << static_cast<unsigned>(value) << "; " << comment << '\n';
}

void write_dw_line(std::ostream& os, const std::vector<std::uint16_t>& values, std::size_t off, std::size_t n) {
    os << kIndent << ".dw ";
    for (std::size_t i = 0; i < n; ++i) {
        if (i != 0) os << ", ";
        std::ostringstream hex;
        hex << std::uppercase << std::hex;
        hex.width(4);
        hex.fill('0');
        hex << static_cast<unsigned>(values[off + i]);
        os << "0x" << hex.str();
    }
    os << '\n';
}

glyph_blob pack_glyph_rows(
    const snatch_glyph_bitmap* glyph,
    int codepoint,
    int cell_width,
    int cell_height,
    int max_bearing_y
) {
    glyph_blob out{};
    out.codepoint = codepoint;
    const int glyph_width = std::max(0, cell_width);
    out.width = static_cast<std::uint8_t>(std::clamp(glyph_width, 0, 255));
    out.height = static_cast<std::uint8_t>(std::clamp(cell_height, 0, 255));
    out.bytes_per_row = (glyph_width + 7) / 8;
    if (out.bytes_per_row <= 0 || cell_height <= 0) return out;

    const std::size_t total = static_cast<std::size_t>(out.bytes_per_row) * static_cast<std::size_t>(cell_height);
    out.payload.assign(total, 0);

    if (!glyph || !glyph->data || glyph->width <= 0 || glyph->height <= 0 || glyph->stride_bytes <= 0) {
        out.payload_size = static_cast<std::uint16_t>(out.payload.size());
        return out;
    }

    // Align all glyphs to a common baseline across the exported font cell.
    const int y_offset = max_bearing_y - glyph->bearing_y;

    for (int y = 0; y < glyph->height; ++y) {
        const int dst_y = y + y_offset;
        if (dst_y < 0 || dst_y >= cell_height) continue;
        const auto* src_row = glyph->data + static_cast<std::size_t>(y * glyph->stride_bytes);
        for (int x = 0; x < glyph->width && x < glyph_width; ++x) {
            if (!bit_is_set(src_row, x)) continue;
            const int byte_index = x / 8;
            const int bit_index = 7 - (x % 8);
            auto& dst = out.payload[static_cast<std::size_t>(dst_y * out.bytes_per_row + byte_index)];
            dst = static_cast<std::uint8_t>(dst | (1u << bit_index));
        }
    }

    out.payload_size = static_cast<std::uint16_t>(out.payload.size());
    return out;
}

export_state export_partner_bitmap_asm_impl(
    const snatch_font* font,
    std::string_view output_path,
    plugin_kv_view opts
) {
    if (!font || !font->bitmap_font || !font->bitmap_font->glyphs) {
        return {10, "partner_bitmap_asm: bitmap font data missing"};
    }
    if (output_path.empty()) return {11, "partner_bitmap_asm: output path is empty"};

    const int first_ascii = font->first_codepoint;
    const int last_ascii = font->last_codepoint;
    if (first_ascii < 0 || last_ascii < first_ascii || last_ascii > 255) {
        return {12, "partner_bitmap_asm: invalid codepoint range"};
    }

    int letter_spacing = 0;
    if (const auto raw = opts.get("letter_spacing"); raw && !raw->empty()) {
        const auto parsed = plugin_parse_int(*raw);
        if (!parsed || *parsed < 0 || *parsed > 15) {
            return {13, "partner_bitmap_asm: letter_spacing must be 0..15"};
        }
        letter_spacing = *parsed;
    } else if (const auto raw = opts.get("spacing_hint"); raw && !raw->empty()) {
        // Backward-compatible alias.
        const auto parsed = plugin_parse_int(*raw);
        if (!parsed || *parsed < 0 || *parsed > 15) {
            return {13, "partner_bitmap_asm: spacing_hint must be 0..15"};
        }
        letter_spacing = *parsed;
    }

    bool proportional = false;
    if (const auto mode = opts.get("font_mode"); mode && !mode->empty()) {
        if (*mode == "proportional") proportional = true;
        if (*mode == "fixed") proportional = false;
    }
    proportional = plugin_parse_bool(opts.get("proportional"), proportional);

    bool has_space_width = false;
    int space_width = 0;
    if (const auto raw = opts.get("space_width"); raw && !raw->empty()) {
        const auto parsed = plugin_parse_int(*raw);
        if (!parsed || *parsed < 0 || *parsed > 7) {
            return {18, "partner_bitmap_asm: space_width must be 0..7"};
        }
        has_space_width = true;
        space_width = *parsed;
    }
    if (proportional && !has_space_width) {
        return {19, "partner_bitmap_asm: space_width is required when proportional=true"};
    }

    std::string module = default_symbol_from_output(output_path);
    if (const auto v = opts.get("module"); v && !v->empty()) module = sanitize_symbol(std::string(*v));

    std::string symbol = module;
    if (const auto v = opts.get("symbol"); v && !v->empty()) symbol = sanitize_symbol(std::string(*v));

    const std::uint8_t flags = static_cast<std::uint8_t>(
        (proportional ? 0x80 : 0x00) |
        ((space_width & 0x07) << 4u) |
        (letter_spacing & 0x0F)
    );

    const snatch_bitmap_font& bf = *font->bitmap_font;
    std::vector<glyph_blob> glyphs;
    glyphs.reserve(static_cast<std::size_t>(last_ascii - first_ascii + 1));

    int max_w = 0;
    int max_bearing_y = 0;
    int min_descender = 0;
    std::vector<const snatch_glyph_bitmap*> glyph_ptrs;
    glyph_ptrs.reserve(static_cast<std::size_t>(last_ascii - first_ascii + 1));

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

    for (std::size_t i = 0; i < glyph_ptrs.size(); ++i) {
        const int cp = first_ascii + static_cast<int>(i);
        const auto* g = glyph_ptrs[i];
        const int cell_width = proportional ? std::max(0, g ? g->width : 0) : fixed_cell_width;
        glyphs.push_back(pack_glyph_rows(g, cp, cell_width, max_h, max_bearing_y));
        if (glyphs.back().payload_size > 255) {
            return {17, "partner_bitmap_asm: glyph payload too large for 1-byte length"};
        }
    }

    std::vector<std::uint16_t> offsets;
    offsets.reserve(glyphs.size());
    std::uint32_t offset = 5u + static_cast<std::uint32_t>(glyphs.size() * 2u);
    for (const auto& g : glyphs) {
        if (offset > 0xFFFFu) return {14, "partner_bitmap_asm: font too large (>64KiB)"};
        offsets.push_back(static_cast<std::uint16_t>(offset));
        offset += 4u + static_cast<std::uint32_t>(g.payload_size);
    }

    std::ostringstream out;
    out << kIndent << ";;  " << module << ".s\n";
    out << kIndent << ";;  \n";
    out << kIndent << ";;  " << module << "\n";
    out << kIndent << ";; \n";
    out << kIndent << ";;  notes: see font.h for format details\n";
    out << kIndent << ";;  \n";
    out << kIndent << ";;  generated by snatch\n";
    out << kIndent << ".module " << module << "\n\n";
    out << kIndent << ".globl _" << symbol << "\n\n";
    out << kIndent << ".area _CODE\n" << "_" << symbol << "::\n";

    out << kIndent << ";; font header\n";
    write_db_value(out, flags, "font flags (bit7 prop, bits4-6 space width, bits0-3 letter spacing)");
    write_db_value(out, static_cast<std::uint8_t>(std::clamp(max_w, 0, 255)), "width (max width for proportional)");
    write_db_value(out, static_cast<std::uint8_t>(std::clamp(max_h, 0, 255)), "height");
    write_db_value(out, static_cast<std::uint8_t>(first_ascii), "first ascii");
    write_db_value(out, static_cast<std::uint8_t>(last_ascii), "last ascii");
    out << '\n';

    out << kIndent << ";; glpyh offsets\n";
    for (std::size_t i = 0; i < offsets.size(); i += 8) {
        const auto n = std::min<std::size_t>(8, offsets.size() - i);
        write_dw_line(out, offsets, i, n);
    }
    out << '\n';

    for (const auto& g : glyphs) {
        out << kIndent << ";; ascii " << g.codepoint << ": " << glyph_label_for_comment(g.codepoint) << '\n';
        write_db_value(out, static_cast<std::uint8_t>(kGlyphClassBitmap << 5u), "class(bits 5-7)");
        write_db_value(out, g.width, "width");
        write_db_value(out, g.height, "height");
        write_db_value(out, static_cast<std::uint8_t>(std::min<std::uint16_t>(255, g.payload_size)), "# bytes");

        if (g.payload.empty() || g.bytes_per_row <= 0 || g.height == 0) continue;

        for (int y = 0; y < g.height; ++y) {
            out << kIndent << ".db ";
            for (int b = 0; b < g.bytes_per_row; ++b) {
                if (b != 0) out << ", ";
                const auto byte = g.payload[static_cast<std::size_t>(y * g.bytes_per_row + b)];
                out << "0b" << to_bin8(byte);
            }
            out << " ; row " << y << '\n';
        }
    }

    std::ofstream file{std::string(output_path), std::ios::out | std::ios::trunc};
    if (!file.is_open()) return {15, "partner_bitmap_asm: cannot open output file"};

    file << out.str();
    if (!file.good()) return {16, "partner_bitmap_asm: failed while writing output"};

    return {};
}

int export_partner_bitmap_asm(
    const snatch_font* font,
    const char* output_path,
    const snatch_kv* options,
    unsigned options_count,
    char* errbuf,
    unsigned errbuf_len
) {
    const export_state result = export_partner_bitmap_asm_impl(
        font,
        output_path ? std::string_view{output_path} : std::string_view{},
        plugin_kv_view{options, options_count}
    );
    if (!result.ok()) plugin_set_err(errbuf, errbuf_len, result.message);
    return result.code;
}

const snatch_plugin_info k_info = {
    "partner_sdcc_asm_bitmap",
    "Exports Partner-style bitmap assembly with per-row binary bytes (.db 0bxxxxxxxx)",
    "snatch project",
    "asm",
    "partner-sdcc-asm-bitmap",
    SNATCH_PLUGIN_ABI_VERSION,
    SNATCH_PLUGIN_KIND_EXPORTER,
    nullptr,
    &export_partner_bitmap_asm
};

} // namespace

extern "C" SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out) {
    if (!out) return 1;
    *out = &k_info;
    return 0;
}
