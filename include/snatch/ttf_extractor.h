/// \file
/// \brief TTF extraction interface and option definitions.
///
/// This header declares types and contracts used by snatch core and plugin stages. It is part of the extractor-transformer-exporter architecture and is consumed by build-time and runtime plugin integration.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

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
