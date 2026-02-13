#include "snatch/plugin.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include <stb_image_write.h>
}

namespace {

const char* find_option(const snatch_kv* options, unsigned count, const char* key) {
    if (!options || !key) return nullptr;
    for (unsigned i = 0; i < count; ++i) {
        if (!options[i].key || !options[i].value) continue;
        if (std::strcmp(options[i].key, key) == 0) return options[i].value;
    }
    return nullptr;
}

int parse_positive(const char* s) {
    if (!s || *s == '\0') return 0;
    char* end = nullptr;
    const long v = std::strtol(s, &end, 10);
    if (end == s || *end != '\0' || v <= 0 || v > 1000000) return 0;
    return static_cast<int>(v);
}

inline bool bit_is_set(const unsigned char* row, int x) {
    const int byte_index = x / 8;
    const int bit_index = 7 - (x % 8);
    return (row[byte_index] & (1u << bit_index)) != 0;
}

void draw_glyph(
    std::vector<unsigned char>& img,
    int image_w,
    int image_h,
    int dst_x,
    int dst_y,
    const snatch_glyph_bitmap& g
) {
    if (!g.data || g.width <= 0 || g.height <= 0 || g.stride_bytes <= 0) return;
    for (int y = 0; y < g.height; ++y) {
        const unsigned char* row = g.data + static_cast<size_t>(y * g.stride_bytes);
        const int yy = dst_y + y;
        if (yy < 0 || yy >= image_h) continue;
        for (int x = 0; x < g.width; ++x) {
            const int xx = dst_x + x;
            if (xx < 0 || xx >= image_w) continue;
            if (bit_is_set(row, x)) {
                img[static_cast<size_t>(yy * image_w + xx)] = 0; // black pixel
            }
        }
    }
}

} // namespace

extern "C" int snatch_plugin_get(const snatch_plugin_info** out);

static int export_png_grid(
    const snatch_font* font,
    const char* output_path,
    const snatch_kv* options,
    unsigned options_count,
    char* errbuf,
    unsigned errbuf_len
) {
    if (!font || !font->bitmap_font || !font->bitmap_font->glyphs) {
        if (errbuf && errbuf_len > 0) std::snprintf(errbuf, errbuf_len, "png: bitmap font data missing");
        return 10;
    }
    if (!output_path || output_path[0] == '\0') {
        if (errbuf && errbuf_len > 0) std::snprintf(errbuf, errbuf_len, "png: output path is empty");
        return 11;
    }

    const snatch_bitmap_font& bf = *font->bitmap_font;
    const int glyph_count = bf.glyph_count;
    if (glyph_count <= 0) {
        if (errbuf && errbuf_len > 0) std::snprintf(errbuf, errbuf_len, "png: no glyphs to export");
        return 12;
    }

    int cols = parse_positive(find_option(options, options_count, "columns"));
    int rows = parse_positive(find_option(options, options_count, "rows"));
    int padding = parse_positive(find_option(options, options_count, "padding"));
    if (padding <= 0) padding = 0;
    if (cols <= 0 && rows <= 0) {
        cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(glyph_count))));
        rows = static_cast<int>(std::ceil(static_cast<double>(glyph_count) / cols));
    } else if (cols <= 0) {
        cols = static_cast<int>(std::ceil(static_cast<double>(glyph_count) / rows));
    } else if (rows <= 0) {
        rows = static_cast<int>(std::ceil(static_cast<double>(glyph_count) / cols));
    }

    int max_bearing_y = 0;
    int min_descender = 0;
    int cell_w = std::max(font->glyph_width, 1);
    for (int i = 0; i < glyph_count; ++i) {
        cell_w = std::max(cell_w, bf.glyphs[i].width);
        max_bearing_y = std::max(max_bearing_y, bf.glyphs[i].bearing_y);
        min_descender = std::min(min_descender, bf.glyphs[i].bearing_y - bf.glyphs[i].height);
    }
    const int cell_h = std::max(1, max_bearing_y - min_descender);

    const int draw_w = cell_w + padding * 2;
    const int draw_h = cell_h + padding * 2;

    const int image_w = cols * draw_w;
    const int image_h = rows * draw_h;
    if (image_w <= 0 || image_h <= 0) {
        if (errbuf && errbuf_len > 0) std::snprintf(errbuf, errbuf_len, "png: invalid image dimensions");
        return 13;
    }

    std::vector<unsigned char> image(static_cast<size_t>(image_w * image_h), 255); // white background

    for (int i = 0; i < glyph_count; ++i) {
        const int gx = (i % cols) * draw_w + padding;
        const int gy = (i / cols) * draw_h + padding;
        const int baseline_y = gy + max_bearing_y;
        const int draw_x = gx;
        const int draw_y = baseline_y - bf.glyphs[i].bearing_y;
        draw_glyph(image, image_w, image_h, draw_x, draw_y, bf.glyphs[i]);
    }

    if (stbi_write_png(output_path, image_w, image_h, 1, image.data(), image_w) == 0) {
        if (errbuf && errbuf_len > 0) std::snprintf(errbuf, errbuf_len, "png: failed to write png");
        return 14;
    }
    return 0;
}

static const snatch_plugin_info k_info = {
    "png",
    "Exports bitmap glyphs into a PNG grid",
    "snatch project",
    SNATCH_PLUGIN_ABI_VERSION,
    &export_png_grid
};

extern "C" SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out) {
    if (!out) return 1;
    *out = &k_info;
    return 0;
}
