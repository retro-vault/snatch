#pragma once

#include <filesystem>
#include <string>

#include "snatch/extracted_font.h"

struct ttf_extract_options {
    std::filesystem::path input_file;
    int first_ascii{-1};
    int last_ascii{-1};
    int font_size{0}; // ppem. <=0 means auto-select.
    bool proportional{false}; // false=fixed, true=proportional
};

class ttf_extractor {
public:
    bool extract(const ttf_extract_options& opt, extracted_font& out, std::string& err) const;

private:
    static int choose_natural_size(void* ft_face);
};
