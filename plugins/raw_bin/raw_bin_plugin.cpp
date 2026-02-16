#include "snatch_plugins/partner_bitmap_transform.h"
#include "snatch/plugin.h"
#include "snatch/plugin_util.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <vector>

namespace {

const snatch_partner_bitmap_data* partner_data_from_user_data(const snatch_font* font) {
    if (!font || !font->user_data) return nullptr;
    const auto* data = static_cast<const snatch_partner_bitmap_data*>(font->user_data);
    if (data->magic != SNATCH_PARTNER_BITMAP_MAGIC || data->version != SNATCH_PARTNER_BITMAP_VERSION) return nullptr;
    if (!data->bytes || data->size == 0) return nullptr;
    return data;
}

const snatch_glyph_bitmap* find_glyph_by_codepoint(const snatch_bitmap_font& bf, int codepoint) {
    for (int i = 0; i < bf.glyph_count; ++i) {
        if (bf.glyphs[i].codepoint == codepoint) return &bf.glyphs[i];
    }
    return nullptr;
}

int export_raw_bin(
    const snatch_font* font,
    const char* output_path,
    const snatch_kv* options,
    unsigned options_count,
    char* errbuf,
    unsigned errbuf_len
) {
    (void)options;
    (void)options_count;
    if (!output_path || output_path[0] == '\0') {
        plugin_set_err(errbuf, errbuf_len, "raw_bin: output path is empty");
        return 11;
    }

    std::vector<std::uint8_t> packed;
    if (const auto* partner = partner_data_from_user_data(font)) {
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
    &export_raw_bin
};

} // namespace

extern "C" SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out) {
    if (!out) return 1;
    *out = &k_info;
    return 0;
}
