#pragma once

#include <filesystem>
#include <string>

#include "snatch/extracted_font.h"

struct edge4 {
    int left{0}, top{0}, right{0}, bottom{0};
};

struct color_rgb {
    int r{0}, g{0}, b{0}; // 0..255
};

struct image_extract_options {
    std::filesystem::path input_file;
    edge4 margins{};
    int columns{0};
    int rows{0};
    edge4 padding{};

    bool inverse{false};
    color_rgb fore_color{0, 0, 0};            // black
    color_rgb back_color{255, 255, 255};      // white
    color_rgb transparent_color{255, 0, 255}; // default magenta
    bool has_transparent{false};

    int first_ascii{-1};
    int last_ascii{-1};
    bool proportional{false};
};

class img_extractor {
public:
    bool extract(const image_extract_options& opt, extracted_font& out, std::string& err) const;
};
