/// \file
/// \brief Core pipeline option structure definitions.
///
/// This header declares types and contracts used by snatch core and plugin stages. It is part of the extractor-transformer-exporter architecture and is consumed by build-time and runtime plugin integration.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#pragma once
#include <filesystem>
#include <string>

struct snatch_options {
    std::filesystem::path plugin_dir;

    std::string extractor;
    std::string extractor_parameters;
    std::string exporter;
    std::string exporter_parameters;
    std::string transformer;
    std::string transformer_parameters;
};
