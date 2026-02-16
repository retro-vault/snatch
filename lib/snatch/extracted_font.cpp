#include "snatch/extracted_font.h"

snatch_font extracted_font::as_plugin_font() const {
    snatch_font out{};
    out.name = name.c_str();
    out.glyph_width = glyph_width;
    out.glyph_height = glyph_height;
    out.first_codepoint = first_codepoint;
    out.last_codepoint = last_codepoint;
    out.pixel_size = pixel_size;
    out.bitmap_font = &bitmap_view;
    out.user_data = nullptr;
    return out;
}
