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
