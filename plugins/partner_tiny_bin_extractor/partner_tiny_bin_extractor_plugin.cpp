/// \file
/// \brief Partner Tiny binary extractor plugin implementation.
///
/// This source file implements one part of the snatch pipeline architecture. It contributes to extracting, transforming, exporting, or orchestrating bitmap data in a plugin-driven workflow.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include "snatch/plugin.h"
#include "snatch/plugin_util.h"
#include "snatch_plugins/partner_tiny_bin.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct tiny_bin_owner {
    std::vector<std::uint8_t> bytes{};
    snatch_partner_tiny_bin_data view{};
    snatch_font font_view{};
    std::string name{};
};

static tiny_bin_owner g_owner;

/// \brief extract_tiny_bin.
int extract_tiny_bin(
    const char* input_path,
    const snatch_kv* /*options*/,
    unsigned /*options_count*/,
    snatch_font* out_font,
    char* errbuf,
    unsigned errbuf_len
) {
    if (!input_path || input_path[0] == '\0') {
        plugin_set_err(errbuf, errbuf_len, "partner_tiny_bin_extractor: input path is empty");
        return 10;
    }
    if (!out_font) {
        plugin_set_err(errbuf, errbuf_len, "partner_tiny_bin_extractor: out_font is null");
        return 11;
    }

    std::ifstream in{input_path, std::ios::binary};
    if (!in.is_open()) {
        plugin_set_err(errbuf, errbuf_len, "partner_tiny_bin_extractor: cannot open input file");
        return 12;
    }

    g_owner.bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (g_owner.bytes.empty()) {
        plugin_set_err(errbuf, errbuf_len, "partner_tiny_bin_extractor: input file is empty");
        return 13;
    }

    g_owner.name = std::filesystem::path(input_path).stem().string();
    if (g_owner.name.empty()) g_owner.name = "partner_tiny_bin";

    g_owner.view = {};
    g_owner.view.magic = SNATCH_PARTNER_TINY_BIN_MAGIC;
    g_owner.view.version = SNATCH_PARTNER_TINY_BIN_VERSION;
    g_owner.view.bytes = g_owner.bytes.data();
    g_owner.view.size = static_cast<std::uint32_t>(g_owner.bytes.size());

    g_owner.font_view = {};
    g_owner.font_view.name = g_owner.name.c_str();
    g_owner.font_view.glyph_width = 0;
    g_owner.font_view.glyph_height = 0;
    g_owner.font_view.first_codepoint = 0;
    g_owner.font_view.last_codepoint = 0;
    g_owner.font_view.pixel_size = 0;
    g_owner.font_view.bitmap_font = nullptr;
    g_owner.font_view.user_data = &g_owner.view;

    *out_font = g_owner.font_view;
    return 0;
}

const snatch_plugin_info k_info = {
    "partner_tiny_bin_extractor",
    "Loads Partner Tiny binary stream into user_data for raster transform",
    "snatch project",
    "bin",
    "partner-tiny",
    SNATCH_PLUGIN_ABI_VERSION,
    SNATCH_PLUGIN_KIND_EXTRACTOR,
    nullptr,
    nullptr,
    &extract_tiny_bin
};

} // namespace

extern "C" SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out) {
    if (!out) return 1;
    *out = &k_info;
    return 0;
}

