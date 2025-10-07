#pragma once
#include <string>
#include <filesystem>

struct edge4 {
    int left{0}, top{0}, right{0}, bottom{0};
};

struct color_rgb {
    int r{0}, g{0}, b{0}; // 0..255
};

enum class source_format {
    unknown,
    image,
    ttf
};

struct snatch_options {
    edge4 margins{};
    int columns{0};
    int rows{0};
    edge4 padding{};

    source_format src_fmt{source_format::unknown};
    std::filesystem::path input_file;
    std::filesystem::path output_file;

    std::string exporter;
    std::string exporter_parameters;

    bool inverse{false};

    color_rgb fore_color{0,0,0};            // black
    color_rgb back_color{255,255,255};      // white
    color_rgb transparent_color{255,0,255}; // default magenta (conventional)
    bool has_transparent{false};

    int first_ascii{-1};
    int last_ascii{-1};
};
