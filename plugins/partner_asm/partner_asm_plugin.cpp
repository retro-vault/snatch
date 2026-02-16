#include "snatch_plugins/partner_tiny_transform.h"
#include "snatch/plugin.h"
#include "snatch/plugin_util.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint8_t kGlyphClassTiny = 1;
constexpr std::string_view kIndent = "        ";

struct export_state {
    int code{0};
    std::string message;

    [[nodiscard]] bool ok() const { return code == 0; }
};

std::string sanitize_symbol(std::string value) {
    if (value.empty()) return "snatch_font";

    for (char& ch : value) {
        const auto u = static_cast<unsigned char>(ch);
        if (!std::isalnum(u) && ch != '_') ch = '_';
    }

    const auto first = static_cast<unsigned char>(value.front());
    if (!std::isalpha(first) && value.front() != '_') {
        value.insert(value.begin(), '_');
    }
    return value;
}

std::string default_symbol_from_output(std::string_view output_path) {
    std::filesystem::path p{std::string(output_path)};
    std::string stem = p.stem().string();
    if (stem.empty()) stem = "snatch_font";
    return sanitize_symbol(std::move(stem));
}

void write_dw_line(std::ostream& os, std::span<const std::uint16_t> values) {
    os << kIndent << ".dw ";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) os << ", ";
        std::ostringstream hex;
        hex << std::uppercase << std::hex;
        hex.width(4);
        hex.fill('0');
        hex << static_cast<unsigned>(values[i]);
        os << "0x" << hex.str();
    }
    os << '\n';
}

void write_db_value(std::ostream& os, std::uint8_t value, std::string_view comment) {
    os << kIndent << ".db " << std::left << std::setw(20) << static_cast<unsigned>(value) << "; " << comment << '\n';
}

std::string decode_move_comment(std::uint8_t byte) {
    const int adx = static_cast<int>((byte >> 5u) & 0x3u);
    const int ady = static_cast<int>((byte >> 3u) & 0x3u);
    const int sx = static_cast<int>((byte >> 1u) & 0x1u);
    const int sy = static_cast<int>((byte >> 2u) & 0x1u);
    const int co0 = static_cast<int>(byte & 0x1u);
    const int co1 = static_cast<int>((byte >> 7u) & 0x1u);

    const int dx = sx ? -adx : adx;
    const int dy = sy ? -ady : ady;
    const int color = (co1 << 1) | co0;

    std::ostringstream out;
    out << "move dx=" << dx << ", dy=" << dy << ", color=";
    if (color == 0) {
        out << "none (move only!)";
    } else if (color == 2) {
        out << "fore (set)";
    } else if (color == 1) {
        out << "back (clear)";
    } else {
        out << "xor (toggle)";
    }
    return out.str();
}

std::string glyph_label_for_comment(int codepoint) {
    if (codepoint == 127) return "<non standard>";
    if (codepoint == 39) return "'''";
    if (codepoint >= 32 && codepoint <= 126) {
        return std::string("'") + static_cast<char>(codepoint) + "'";
    }
    return "'?'";
}

export_state export_partner_asm_impl(
    const snatch_font* font,
    std::string_view output_path,
    plugin_kv_view opts
) {
    if (!font) return {10, "partner_asm: font is null"};
    if (output_path.empty()) return {11, "partner_asm: output path is empty"};

    const int first_ascii = font->first_codepoint;
    const int last_ascii = font->last_codepoint;
    if (first_ascii < 0 || last_ascii < first_ascii || last_ascii > 255) {
        return {12, "partner_asm: invalid codepoint range"};
    }

    if (!font->user_data) {
        return {13, "partner_asm: missing transformed data; use --transformer partner_tiny_transform"};
    }

    const auto* transformed = static_cast<const snatch_partner_tiny_data*>(font->user_data);
    if (transformed->magic != SNATCH_PARTNER_TINY_MAGIC || transformed->version != SNATCH_PARTNER_TINY_VERSION) {
        return {14, "partner_asm: incompatible user_data; expected partner_tiny_transform output"};
    }

    const int expected_glyph_count = last_ascii - first_ascii + 1;
    if (transformed->glyph_count != expected_glyph_count || !transformed->glyphs) {
        return {15, "partner_asm: transformed glyph table does not match ascii range"};
    }

    int letter_spacing = 0;
    if (const auto raw = opts.get("letter_spacing"); raw && !raw->empty()) {
        const auto parsed = plugin_parse_int(*raw);
        if (!parsed || *parsed < 0 || *parsed > 15) {
            return {16, "partner_asm: letter_spacing must be 0..15"};
        }
        letter_spacing = *parsed;
    } else if (const auto raw = opts.get("spacing_hint"); raw && !raw->empty()) {
        // Backward-compatible alias.
        const auto parsed = plugin_parse_int(*raw);
        if (!parsed || *parsed < 0 || *parsed > 15) {
            return {16, "partner_asm: spacing_hint must be 0..15"};
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
            return {21, "partner_asm: space_width must be 0..7"};
        }
        has_space_width = true;
        space_width = *parsed;
    }
    if (proportional && !has_space_width) {
        return {22, "partner_asm: space_width is required when proportional=true"};
    }

    std::string module = default_symbol_from_output(output_path);
    if (const auto v = opts.get("module"); v && !v->empty()) {
        module = sanitize_symbol(std::string(*v));
    }

    std::string symbol = module;
    if (const auto v = opts.get("symbol"); v && !v->empty()) {
        symbol = sanitize_symbol(std::string(*v));
    }

    const std::uint8_t flags = static_cast<std::uint8_t>(
        (proportional ? 0x80 : 0x00) |
        ((space_width & 0x07) << 4u) |
        (letter_spacing & 0x0F)
    );

    std::vector<std::uint16_t> offsets;
    offsets.reserve(static_cast<std::size_t>(transformed->glyph_count));

    std::uint32_t offset = 5u + static_cast<std::uint32_t>(transformed->glyph_count * 2u);
    for (std::size_t i = 0; i < transformed->glyph_count; ++i) {
        const auto& glyph = transformed->glyphs[i];
        if (offset > 0xFFFFu) return {17, "partner_asm: font too large (>64KiB)"};
        offsets.push_back(static_cast<std::uint16_t>(offset));
        offset += 4u + static_cast<std::uint32_t>(glyph.data_size);
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
    write_db_value(out, transformed->max_width_minus_one, "width (max width for proportional)");
    write_db_value(out, transformed->max_height_minus_one, "height");
    write_db_value(out, static_cast<std::uint8_t>(first_ascii), "first ascii");
    write_db_value(out, static_cast<std::uint8_t>(last_ascii), "last ascii");
    out << '\n';

    out << kIndent << ";; glpyh offsets\n";
    for (std::size_t i = 0; i < offsets.size(); i += 8) {
        const std::size_t n = std::min<std::size_t>(8, offsets.size() - i);
        write_dw_line(out, std::span<const std::uint16_t>{offsets}.subspan(i, n));
    }
    out << '\n';

    for (std::size_t i = 0; i < transformed->glyph_count; ++i) {
        const auto& glyph = transformed->glyphs[i];
        const int codepoint = first_ascii + static_cast<int>(i);

        out << kIndent << ";; ascii " << codepoint << ": " << glyph_label_for_comment(codepoint) << '\n';
        write_db_value(out, static_cast<std::uint8_t>(kGlyphClassTiny << 5u), "class(bits 5-7)");
        write_db_value(out, glyph.width_minus_one, "width");
        write_db_value(out, glyph.height_minus_one, "height");

        if (glyph.data_size == 0 || !glyph.data) {
            write_db_value(out, 0u, "# moves");
            continue;
        }

        if (glyph.data_size < 2) {
            return {20, "partner_asm: malformed glyph data (origin missing)"};
        }

        const auto bytes = std::span<const std::uint8_t>{glyph.data, glyph.data_size};
        const auto move_count = static_cast<std::uint8_t>(bytes.size() - 2u);
        write_db_value(out, move_count, "# moves");
        write_db_value(out, bytes[0], "x origin");
        write_db_value(out, bytes[1], "y origin");
        for (std::size_t b = 2; b < bytes.size(); ++b) {
            write_db_value(out, bytes[b], decode_move_comment(bytes[b]));
        }
    }

    std::ofstream file{std::string(output_path), std::ios::out | std::ios::trunc};
    if (!file.is_open()) {
        return {18, "partner_asm: cannot open output file"};
    }

    file << out.str();
    if (!file.good()) {
        return {19, "partner_asm: failed while writing output"};
    }

    return {};
}

int export_partner_asm(
    const snatch_font* font,
    const char* output_path,
    const snatch_kv* options,
    unsigned options_count,
    char* errbuf,
    unsigned errbuf_len
) {
    const export_state result = export_partner_asm_impl(
        font,
        output_path ? std::string_view{output_path} : std::string_view{},
        plugin_kv_view{options, options_count}
    );

    if (!result.ok()) plugin_set_err(errbuf, errbuf_len, result.message);
    return result.code;
}

const snatch_plugin_info k_info = {
    "partner_sdcc_asm_tiny",
    "Exports Partner Tiny-vector font assembly (.db/.dw); requires partner_tiny_transform",
    "snatch project",
    "asm",
    "partner-sdcc-asm-tiny",
    SNATCH_PLUGIN_ABI_VERSION,
    SNATCH_PLUGIN_KIND_EXPORTER,
    nullptr,
    &export_partner_asm
};

} // namespace

extern "C" SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out) {
    if (!out) return 1;
    *out = &k_info;
    return 0;
}
