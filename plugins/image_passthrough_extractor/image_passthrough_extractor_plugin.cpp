/// \file
/// \brief Full-image passthrough extractor plugin implementation.
///
/// This source file implements one part of the snatch pipeline architecture. It contributes to extracting, transforming, exporting, or orchestrating bitmap data in a plugin-driven workflow.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include "snatch/plugin.h"
#include "snatch/plugin_util.h"
#include "snatch_plugins/image_passthrough_data.h"

#include <cstdint>
#include <string>
#include <vector>

extern "C" {
#include <stb_image.h>
}

namespace {

struct image_passthrough_owner {
    std::vector<std::uint8_t> pixels{};
    snatch_image_passthrough_data view{};
    snatch_font font_view{};
};

static image_passthrough_owner g_owner;

/// \brief extract_image_passthrough.
int extract_image_passthrough(
    const char* input_path,
    const snatch_kv* /*options*/,
    unsigned /*options_count*/,
    snatch_font* out_font,
    char* errbuf,
    unsigned errbuf_len
) {
    if (!input_path || input_path[0] == '\0') {
        plugin_set_err(errbuf, errbuf_len, "image_passthrough_extractor: input path is empty");
        return 10;
    }
    if (!out_font) {
        plugin_set_err(errbuf, errbuf_len, "image_passthrough_extractor: out_font is null");
        return 11;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* gray = stbi_load(input_path, &width, &height, &channels, 1);
    if (!gray || width <= 0 || height <= 0) {
        if (gray) stbi_image_free(gray);
        plugin_set_err(errbuf, errbuf_len, "image_passthrough_extractor: failed to load image");
        return 12;
    }

    g_owner.pixels.assign(gray, gray + static_cast<std::size_t>(width * height));
    stbi_image_free(gray);

    g_owner.view = {};
    g_owner.view.magic = SNATCH_IMAGE_PASSTHROUGH_MAGIC;
    g_owner.view.version = SNATCH_IMAGE_PASSTHROUGH_VERSION;
    g_owner.view.width = static_cast<std::uint16_t>(width);
    g_owner.view.height = static_cast<std::uint16_t>(height);
    g_owner.view.stride = static_cast<std::uint16_t>(width);
    g_owner.view.pixels = g_owner.pixels.data();

    g_owner.font_view = {};
    g_owner.font_view.name = "image-passthrough";
    g_owner.font_view.glyph_width = width;
    g_owner.font_view.glyph_height = height;
    g_owner.font_view.first_codepoint = 0;
    g_owner.font_view.last_codepoint = 0;
    g_owner.font_view.pixel_size = 0;
    g_owner.font_view.bitmap_font = nullptr;
    g_owner.font_view.user_data = &g_owner.view;

    *out_font = g_owner.font_view;
    return 0;
}

const snatch_plugin_info k_info = {
    "image_passthrough_extractor",
    "Loads image as grayscale passthrough data in user_data",
    "snatch project",
    "image",
    "passthrough-gray8",
    SNATCH_PLUGIN_ABI_VERSION,
    SNATCH_PLUGIN_KIND_EXTRACTOR,
    nullptr,
    nullptr,
    &extract_image_passthrough
};

} // namespace

extern "C" SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out) {
    if (!out) return 1;
    *out = &k_info;
    return 0;
}
