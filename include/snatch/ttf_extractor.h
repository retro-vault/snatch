#pragma once

#include <string>
#include <vector>
#include "snatch/options.h"
#include "snatch/plugin.h"

struct extracted_glyph {
    snatch_glyph_bitmap view{};
    std::vector<unsigned char> bitmap;
};

struct extracted_font {
    std::string name;
    int glyph_width{0};
    int glyph_height{0};
    int first_codepoint{0};
    int last_codepoint{0};
    int pixel_size{0};

    std::vector<extracted_glyph> glyphs;
    std::vector<snatch_glyph_bitmap> glyph_views;
    snatch_bitmap_font bitmap_view{};

    snatch_font as_plugin_font() const;
};

class ttf_extractor {
public:
    bool extract(const snatch_options& opt, extracted_font& out, std::string& err) const;

private:
    static int choose_natural_size(void* ft_face);
};
