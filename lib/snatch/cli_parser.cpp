/// \file
/// \brief Implementation of CLI parsing and option normalization.
///
/// This source file implements one part of the snatch pipeline architecture. It contributes to extracting, transforming, exporting, or orchestrating bitmap data in a plugin-driven workflow.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include "snatch/cli_parser.h"
#include <iostream>
#include <string>
#include <vector>



extern "C" {
#include <argparse.h>
}

// ---- parse ---------------------------------------------------------------

/// \brief cli_parser::parse.
int cli_parser::parse(int argc, const char** argv, snatch_options& out) const {
    std::vector<std::string> normalized_storage;
    std::vector<const char*> normalized_argv;
    normalized_storage.reserve(static_cast<size_t>(argc));
    normalized_argv.reserve(static_cast<size_t>(argc));

    for (int i = 0; i < argc; ++i) {
        std::string arg = argv[i] ? argv[i] : "";
        normalized_storage.push_back(std::move(arg));
    }
    for (auto& s : normalized_storage) {
        normalized_argv.push_back(s.c_str());
    }

    // argparse target variables
    const char* extractor_str = nullptr;
    const char* extractor_params_str = nullptr;
    const char* exporter_str = nullptr;
    const char* exporter_params_str = nullptr;
    const char* transformer_str = nullptr;
    const char* transformer_params_str = nullptr;
    const char* plugin_dir_str = nullptr;
    const char* const usage[] = {
        "snatch [options]",
        nullptr
    };

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    
    // Use macros to avoid missing-field warnings in C++
    struct argparse_option options[] = {
        OPT_STRING('q', "extractor",            &extractor_str,       "extractor plugin name override"),
        OPT_STRING('v', "extractor-parameters", &extractor_params_str,"parameters for extractor (quoted ok)"),
        OPT_STRING('d', "plugin-dir", &plugin_dir_str, "plugin directory override"),

        // exporter and parameters
        OPT_STRING('e', "exporter",             &exporter_str,        "exporter name (plugin/tool)"),
        OPT_STRING('x', "exporter-parameters",  &exporter_params_str, "parameters for exporter (quoted ok)"),
        OPT_STRING('w', "transformer",          &transformer_str,        "transformer name (plugin/tool)"),
        OPT_STRING('y', "transformer-parameters", &transformer_params_str, "parameters for transformer (quoted ok)"),

        OPT_HELP(),
        OPT_END()
    };

#pragma GCC diagnostic pop

    struct argparse ap{};
    argparse_init(&ap, options, usage, 0);
    argparse_describe(&ap, "snatch font processor",
                           "example: snatch --extractor ttf_extractor --extractor-parameters \"input=MyFont.ttf\" --exporter raw_bin --exporter-parameters \"output=out.bin\"");
    int nargs = argparse_parse(&ap, argc, normalized_argv.data());

    if (nargs != 0) {
        std::cerr << "error: unexpected positional arguments; pass input via --extractor-parameters input=...\n";
        return 1;
    }

    // fill output struct
    if (plugin_dir_str) out.plugin_dir = plugin_dir_str;
    if (extractor_str) out.extractor = extractor_str;
    if (extractor_params_str) out.extractor_parameters = extractor_params_str;
    if (exporter_str) out.exporter = exporter_str;
    if (exporter_params_str) out.exporter_parameters = exporter_params_str;
    if (transformer_str) out.transformer = transformer_str;
    if (transformer_params_str) out.transformer_parameters = transformer_params_str;
    return 0;
}
