/// \file
/// \brief Image extractor plugin adapter for core extraction.
///
/// This source file implements one part of the snatch pipeline architecture. It contributes to extracting, transforming, exporting, or orchestrating bitmap data in a plugin-driven workflow.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include "snatch/plugin.h"
#include "snatch/plugin_util.h"

#include "snatch/extracted_font.h"
#include "snatch/img_extractor.h"

#include <optional>
#include <string>

namespace {

struct image_extract_owner {
    extracted_font font{};
    snatch_font view{};
};

static image_extract_owner g_owner;

/// \brief parse_int_kv.
std::optional<int> parse_int_kv(const plugin_kv_view& kv, std::string_view key) {
    if (const auto raw = kv.get(key); raw && !raw->empty()) {
        return plugin_parse_int(*raw);
    }
    return std::nullopt;
}

/// \brief parse_color_kv.
bool parse_color_kv(const plugin_kv_view& kv, std::string_view key, color_rgb& out) {
    const auto raw = kv.get(key);
    if (!raw || raw->empty()) return true;
    const auto rgb = plugin_parse_hex_rgb(*raw);
    if (!rgb) return false;
    out.r = (*rgb)[0];
    out.g = (*rgb)[1];
    out.b = (*rgb)[2];
    return true;
}

/// \brief parse_proportional.
bool parse_proportional(const plugin_kv_view& kv, bool fallback, char* errbuf, unsigned errbuf_len) {
    if (const auto mode = kv.get("font_mode"); mode && !mode->empty()) {
        if (*mode == "fixed") return false;
        if (*mode == "proportional") return true;
        plugin_set_err(errbuf, errbuf_len, "image_extractor: font_mode must be fixed|proportional");
        return fallback;
    }
    return plugin_parse_bool(kv.get("proportional"), fallback);
}

/// \brief extract_image.
int extract_image(
    const char* input_path,
    const snatch_kv* options,
    unsigned options_count,
    snatch_font* out_font,
    char* errbuf,
    unsigned errbuf_len
) {
    if (!input_path || input_path[0] == '\0') {
        plugin_set_err(errbuf, errbuf_len, "image_extractor: input path is empty");
        return 10;
    }
    if (!out_font) {
        plugin_set_err(errbuf, errbuf_len, "image_extractor: out_font is null");
        return 11;
    }

    const plugin_kv_view kv{options, options_count};

    image_extract_options opt{};
    opt.input_file = input_path;

    if (const auto v = parse_int_kv(kv, "columns"); v.has_value()) opt.columns = *v;
    if (const auto v = parse_int_kv(kv, "rows"); v.has_value()) opt.rows = *v;
    if (const auto v = parse_int_kv(kv, "first_ascii"); v.has_value()) opt.first_ascii = *v;
    if (const auto v = parse_int_kv(kv, "last_ascii"); v.has_value()) opt.last_ascii = *v;

    if (const auto v = parse_int_kv(kv, "margins_left"); v.has_value()) opt.margins.left = *v;
    if (const auto v = parse_int_kv(kv, "margins_top"); v.has_value()) opt.margins.top = *v;
    if (const auto v = parse_int_kv(kv, "margins_right"); v.has_value()) opt.margins.right = *v;
    if (const auto v = parse_int_kv(kv, "margins_bottom"); v.has_value()) opt.margins.bottom = *v;

    if (const auto v = parse_int_kv(kv, "padding_left"); v.has_value()) opt.padding.left = *v;
    if (const auto v = parse_int_kv(kv, "padding_top"); v.has_value()) opt.padding.top = *v;
    if (const auto v = parse_int_kv(kv, "padding_right"); v.has_value()) opt.padding.right = *v;
    if (const auto v = parse_int_kv(kv, "padding_bottom"); v.has_value()) opt.padding.bottom = *v;

    opt.inverse = plugin_parse_bool(kv.get("inverse"), false);
    opt.proportional = parse_proportional(kv, false, errbuf, errbuf_len);
    if (errbuf && errbuf[0] != '\0') return 12;

    if (!parse_color_kv(kv, "fore_color", opt.fore_color)) {
        plugin_set_err(errbuf, errbuf_len, "image_extractor: invalid fore_color; expected #RRGGBB");
        return 13;
    }
    if (!parse_color_kv(kv, "back_color", opt.back_color)) {
        plugin_set_err(errbuf, errbuf_len, "image_extractor: invalid back_color; expected #RRGGBB");
        return 14;
    }
    if (const auto transparent = kv.get("transparent_color"); transparent && !transparent->empty()) {
        if (!parse_color_kv(kv, "transparent_color", opt.transparent_color)) {
            plugin_set_err(errbuf, errbuf_len, "image_extractor: invalid transparent_color; expected #RRGGBB");
            return 15;
        }
        opt.has_transparent = true;
    }

    img_extractor extractor;
    extracted_font extracted;
    std::string extract_err;
    if (!extractor.extract(opt, extracted, extract_err)) {
        plugin_set_err(errbuf, errbuf_len, std::string("image_extractor: ") + extract_err);
        return 16;
    }

    g_owner.font = std::move(extracted);
    g_owner.view = g_owner.font.as_plugin_font();
    *out_font = g_owner.view;
    return 0;
}

const snatch_plugin_info k_info = {
    "image_extractor",
    "Extracts bitmap glyphs from image sheets",
    "snatch project",
    "image",
    "extractor",
    SNATCH_PLUGIN_ABI_VERSION,
    SNATCH_PLUGIN_KIND_EXTRACTOR,
    nullptr,
    nullptr,
    &extract_image
};

} // namespace

extern "C" SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out) {
    if (!out) return 1;
    *out = &k_info;
    return 0;
}
