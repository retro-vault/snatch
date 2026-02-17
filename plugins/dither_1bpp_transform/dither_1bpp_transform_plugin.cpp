/// \file
/// \brief 1bpp dithering transformer plugin implementation.
///
/// This source file implements one part of the snatch pipeline architecture. It contributes to extracting, transforming, exporting, or orchestrating bitmap data in a plugin-driven workflow.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include "snatch/plugin.h"
#include "snatch/plugin_util.h"
#include "snatch_plugins/image_passthrough_data.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

struct dither_owner {
    std::vector<std::uint8_t> bitmap{};
    snatch_glyph_bitmap glyph{};
    snatch_bitmap_font font{};
};

static dither_owner g_owner;

/// \brief parse_threshold.
int parse_threshold(const plugin_kv_view& kv, char* errbuf, unsigned errbuf_len) {
    if (const auto raw = kv.get("threshold"); raw && !raw->empty()) {
        const auto parsed = plugin_parse_int(*raw);
        if (!parsed || *parsed < 0 || *parsed > 255) {
            plugin_set_err(errbuf, errbuf_len, "dither_1bpp_transform: threshold must be 0..255");
            return -1;
        }
        return *parsed;
    }
    return 128;
}

/// \brief add_error.
void add_error(std::vector<float>& buf, int w, int h, int x, int y, float value) {
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    buf[static_cast<std::size_t>(y * w + x)] += value;
}

/// \brief dither_1bpp_transform.
int dither_1bpp_transform(
    snatch_font* font,
    const snatch_kv* options,
    unsigned options_count,
    char* errbuf,
    unsigned errbuf_len
) {
    if (!font || !font->user_data) {
        plugin_set_err(errbuf, errbuf_len, "dither_1bpp_transform: user_data missing");
        return 10;
    }

    const auto* src = static_cast<const snatch_image_passthrough_data*>(font->user_data);
    if (src->magic != SNATCH_IMAGE_PASSTHROUGH_MAGIC || src->version != SNATCH_IMAGE_PASSTHROUGH_VERSION) {
        plugin_set_err(errbuf, errbuf_len, "dither_1bpp_transform: incompatible user_data payload");
        return 11;
    }
    if (!src->pixels || src->width == 0 || src->height == 0 || src->stride == 0) {
        plugin_set_err(errbuf, errbuf_len, "dither_1bpp_transform: invalid source image buffer");
        return 12;
    }

    const plugin_kv_view kv{options, options_count};
    const int threshold = parse_threshold(kv, errbuf, errbuf_len);
    if (threshold < 0) return 13;

    const int w = src->width;
    const int h = src->height;
    const int stride = (w + 7) / 8;

    std::vector<float> work(static_cast<std::size_t>(w * h), 0.0f);
    for (int y = 0; y < h; ++y) {
        const auto* row = src->pixels + static_cast<std::size_t>(y * src->stride);
        for (int x = 0; x < w; ++x) {
            work[static_cast<std::size_t>(y * w + x)] = static_cast<float>(row[x]);
        }
    }

    g_owner.bitmap.assign(static_cast<std::size_t>(stride * h), 0);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t idx = static_cast<std::size_t>(y * w + x);
            const float old_px = work[idx];
            const float new_px = (old_px >= static_cast<float>(threshold)) ? 255.0f : 0.0f;
            const float err = old_px - new_px;

            if (new_px < 128.0f) {
                const int byte_index = x / 8;
                const int bit_index = 7 - (x % 8);
                auto& out = g_owner.bitmap[static_cast<std::size_t>(y * stride + byte_index)];
                out = static_cast<std::uint8_t>(out | (1u << bit_index));
            }

            // Floyd-Steinberg error diffusion
            add_error(work, w, h, x + 1, y,     err * (7.0f / 16.0f));
            add_error(work, w, h, x - 1, y + 1, err * (3.0f / 16.0f));
            add_error(work, w, h, x,     y + 1, err * (5.0f / 16.0f));
            add_error(work, w, h, x + 1, y + 1, err * (1.0f / 16.0f));
        }
    }

    g_owner.glyph = {};
    g_owner.glyph.codepoint = 0;
    g_owner.glyph.width = w;
    g_owner.glyph.height = h;
    g_owner.glyph.bearing_x = 0;
    g_owner.glyph.bearing_y = h;
    g_owner.glyph.advance_x = w;
    g_owner.glyph.stride_bytes = stride;
    g_owner.glyph.data = g_owner.bitmap.data();

    g_owner.font = {};
    g_owner.font.glyph_count = 1;
    g_owner.font.glyphs = &g_owner.glyph;

    font->glyph_width = w;
    font->glyph_height = h;
    font->first_codepoint = 0;
    font->last_codepoint = 0;
    font->bitmap_font = &g_owner.font;
    font->user_data = nullptr;

    return 0;
}

const snatch_plugin_info k_info = {
    "dither_1bpp_transform",
    "Converts grayscale passthrough image to 1bpp bitmap with Floyd-Steinberg dithering",
    "snatch project",
    "bitmap",
    "dither-1bpp",
    SNATCH_PLUGIN_ABI_VERSION,
    SNATCH_PLUGIN_KIND_TRANSFORMER,
    &dither_1bpp_transform,
    nullptr,
    nullptr
};

} // namespace

extern "C" SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out) {
    if (!out) return 1;
    *out = &k_info;
    return 0;
}
