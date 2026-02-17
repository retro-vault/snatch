/// \file
/// \brief Main executable pipeline orchestration entry point.
///
/// This source file implements one part of the snatch pipeline architecture. It contributes to extracting, transforming, exporting, or orchestrating bitmap data in a plugin-driven workflow.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include <iostream>
#include <filesystem>
#include <vector>
#include <cstdlib>
#include <string>
#include <sstream>
#include <array>
#include <cctype>
#include <algorithm>
#include <optional>
#include <string_view>
#include "snatch/cli_parser.h"
#include "snatch/options.h"
#include "snatch/plugin.h"
#include "snatch/plugin_manager.h"

/// \brief parse_kv_pairs.
static std::vector<std::array<std::string, 2>> parse_kv_pairs(const std::string& raw) {
    std::vector<std::array<std::string, 2>> pairs;
    if (raw.empty()) return pairs;

    std::stringstream ss(raw);
    std::string token;
    while (std::getline(ss, token, ',')) {
        const auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
        while (!token.empty() && is_space(static_cast<unsigned char>(token.front()))) token.erase(token.begin());
        while (!token.empty() && is_space(static_cast<unsigned char>(token.back()))) token.pop_back();
        if (token.empty()) continue;
        const auto eq = token.find('=');
        if (eq == std::string::npos) {
            pairs.push_back({token, ""});
        } else {
            std::string key = token.substr(0, eq);
            std::string value = token.substr(eq + 1);
            while (!key.empty() && is_space(static_cast<unsigned char>(key.front()))) key.erase(key.begin());
            while (!key.empty() && is_space(static_cast<unsigned char>(key.back()))) key.pop_back();
            while (!value.empty() && is_space(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
            while (!value.empty() && is_space(static_cast<unsigned char>(value.back()))) value.pop_back();
            pairs.push_back({std::move(key), std::move(value)});
        }
    }
    return pairs;
}

/// \brief print_kv_pairs.
static void print_kv_pairs(const char* label, const std::string& raw) {
    const auto pairs = parse_kv_pairs(raw);
    if (pairs.empty()) return;
    std::cout << "  " << label << " parsed:\n";
    for (const auto& p : pairs) {
        std::cout << "    - " << p[0] << "=" << p[1] << "\n";
    }
}

/// \brief print_options.
static void print_options(const snatch_options& opt) {
    std::cout << "snatch options:\n";
    std::cout << "  plugin dir: " << (opt.plugin_dir.empty() ? "(none)" : opt.plugin_dir.string()) << "\n";
    std::cout << "  extractor: " << (opt.extractor.empty() ? "(auto)" : opt.extractor) << "\n";
    std::cout << "  extractor params: " << (opt.extractor_parameters.empty() ? "(none)" : opt.extractor_parameters) << "\n";
    print_kv_pairs("extractor params", opt.extractor_parameters);
    std::cout << "  transformer: " << (opt.transformer.empty() ? "(none)" : opt.transformer) << "\n";
    std::cout << "  transformer params: " << (opt.transformer_parameters.empty() ? "(none)" : opt.transformer_parameters) << "\n";
    print_kv_pairs("transformer params", opt.transformer_parameters);
    std::cout << "  exporter: " << (opt.exporter.empty() ? "(none)" : opt.exporter) << "\n";
    std::cout << "  exporter params: " << (opt.exporter_parameters.empty() ? "(none)" : opt.exporter_parameters) << "\n";
    print_kv_pairs("exporter params", opt.exporter_parameters);
}

/// \brief trim_copy.
static std::string trim_copy(std::string s) {
    const auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && is_space(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && is_space(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

/// \brief to_lower_copy.
static std::string to_lower_copy(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

/// \brief is_export_type_token.
static bool is_export_type_token(const std::string& exporter) {
    const std::string e = to_lower_copy(exporter);
    return e == "asm" || e == "c" || e == "bin";
}

struct exporter_resolution {
    std::string plugin_name;
    std::string error;
};

struct extractor_resolution {
    std::string plugin_name;
    std::string error;
};

/// \brief find_kv_value.
static std::optional<std::string> find_kv_value(const std::string& raw, std::string_view key) {
    for (const auto& pair : parse_kv_pairs(raw)) {
        if (pair[0] == key) return pair[1];
    }
    return std::nullopt;
}

/// \brief resolve_extractor_plugin.
static extractor_resolution resolve_extractor_plugin(const snatch_options& opt, const std::string& input_path) {
    extractor_resolution out{};
    if (!opt.extractor.empty()) {
        out.plugin_name = opt.extractor;
        return out;
    }
    std::filesystem::path p(input_path);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext == ".ttf" || ext == ".otf") {
        out.plugin_name = "ttf_extractor";
        return out;
    }
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif" ||
        ext == ".tga" || ext == ".webp") {
        out.plugin_name = "image_extractor";
        return out;
    }
    out.error =
        "cannot infer extractor from input extension '" + ext +
        "'; specify --extractor explicitly";
    return out;
}

/// \brief resolve_exporter_plugin.
static exporter_resolution resolve_exporter_plugin(const snatch_options& opt) {
    exporter_resolution out{};
    if (opt.exporter.empty() || !is_export_type_token(opt.exporter)) {
        const std::string exporter = to_lower_copy(opt.exporter);
        if (exporter == "partner_asm") {
            out.plugin_name = "partner_sdcc_asm_tiny";
        } else if (exporter == "partner_bitmap_asm") {
            out.plugin_name = "partner_sdcc_asm_bitmap";
        } else if (exporter == "partner-sdcc-asm-tiny" || exporter == "partner_tiny_asm") {
            out.plugin_name = "partner_sdcc_asm_tiny";
        } else if (exporter == "partner-sdcc-asm-bitmap" || exporter == "partner_bitmap_asm_sdcc") {
            out.plugin_name = "partner_sdcc_asm_bitmap";
        } else {
            out.plugin_name = opt.exporter; // already concrete plugin name (or empty)
        }
        return out;
    }

    const std::string type = to_lower_copy(opt.exporter);
    if (type == "asm") {
        out.error =
            "exporter 'asm' is ambiguous; use concrete exporter name: "
            "partner_sdcc_asm_tiny or partner_sdcc_asm_bitmap";
        return out;
    }

    if (type == "bin") {
        out.plugin_name = "raw_bin";
        return out;
    }

    if (type == "c") {
        out.plugin_name = "raw_c";
        return out;
    }

    out.error = "unsupported exporter type '" + type + "'";
    return out;
}

/// \brief add_kv.
static void add_kv(
    std::vector<std::array<std::string, 2>>& storage,
    std::vector<snatch_kv>& out,
    std::string key,
    std::string value
) {
    storage.push_back({std::move(key), std::move(value)});
    auto& pair = storage.back();
    out.push_back({pair[0].c_str(), pair[1].c_str()});
}

/// \brief append_kv_params.
static void append_kv_params(
    const std::string& raw,
    std::vector<std::array<std::string, 2>>& storage,
    std::vector<snatch_kv>& out,
    std::initializer_list<std::string_view> skip_keys = {}
) {
    if (raw.empty()) return;
    std::stringstream ss(raw);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token = trim_copy(token);
        if (token.empty()) continue;
        const auto eq = token.find('=');
        if (eq == std::string::npos) {
            bool skip = false;
            for (const auto& k : skip_keys) {
                if (token == k) { skip = true; break; }
            }
            if (skip) continue;
            add_kv(storage, out, token, "");
            continue;
        }
        auto key = trim_copy(token.substr(0, eq));
        bool skip = false;
        for (const auto& k : skip_keys) {
            if (key == k) { skip = true; break; }
        }
        if (skip) continue;
        add_kv(storage, out, std::move(key), trim_copy(token.substr(eq + 1)));
    }
}

/// \brief main.
int main(int argc, const char** argv) {
    snatch_options opt;
    cli_parser parser;
    int rc = parser.parse(argc, argv, opt);
    if (rc) return rc;

    const auto input_path_opt = find_kv_value(opt.extractor_parameters, "input");
    if (!input_path_opt || input_path_opt->empty()) {
        std::cerr << "error: extractor input path is required in --extractor-parameters (input=...)\n";
        return 3;
    }
    const std::string input_path = *input_path_opt;

    const auto output_path_opt = find_kv_value(opt.exporter_parameters, "output");
    if (!output_path_opt || output_path_opt->empty()) {
        std::cerr << "error: exporter output path is required in --exporter-parameters (output=...)\n";
        return 3;
    }
    const std::string output_path = *output_path_opt;

    const extractor_resolution extractor_resolved = resolve_extractor_plugin(opt, input_path);
    if (!extractor_resolved.error.empty()) {
        std::cerr << "error: " << extractor_resolved.error << "\n";
        return 3;
    }
    const std::string extractor_plugin_name = extractor_resolved.plugin_name;

    const exporter_resolution resolved = resolve_exporter_plugin(opt);
    if (!resolved.error.empty()) {
        std::cerr << "error: " << resolved.error << "\n";
        return 3;
    }
    const std::string exporter_plugin_name = resolved.plugin_name;

    plugin_manager pm;
    std::vector<std::filesystem::path> plugin_dirs;
    if (!opt.plugin_dir.empty()) {
        plugin_dirs.push_back(opt.plugin_dir);
    }

    if (const char* env_plugin_dir = std::getenv("SNATCH_PLUGIN_DIR");
        env_plugin_dir && env_plugin_dir[0] != '\0') {
        plugin_dirs.emplace_back(env_plugin_dir);
    }

#ifdef SNATCH_DEFAULT_PLUGIN_DIR
    plugin_dirs.emplace_back(SNATCH_DEFAULT_PLUGIN_DIR);
#else
    plugin_dirs.emplace_back("/usr/libexec/snatch/plugins");
#endif

    if (const char* home = std::getenv("HOME"); home && home[0] != '\0') {
        plugin_dirs.emplace_back(std::filesystem::path(home) / ".local/lib/snatch/plugins");
    }

    std::vector<std::string> requested_plugins;
    if (!extractor_plugin_name.empty()) {
        requested_plugins.push_back(extractor_plugin_name);
    }
    if (!opt.transformer.empty()) {
        requested_plugins.push_back(opt.transformer);
    }
    if (!exporter_plugin_name.empty() &&
        std::find(requested_plugins.begin(), requested_plugins.end(), exporter_plugin_name) == requested_plugins.end()) {
        requested_plugins.push_back(exporter_plugin_name);
    }

    if (!requested_plugins.empty()) {
        pm.load_named_from_dirs_in_order(plugin_dirs, requested_plugins);
    } else {
        pm.load_from_dirs_in_order(plugin_dirs);
    }
    print_options(opt);
    std::cout << "  input (extractor): " << input_path << "\n";
    std::cout << "  output (exporter): " << output_path << "\n";
    if (!opt.extractor.empty() && extractor_plugin_name != opt.extractor) {
        std::cout << "  extractor resolved plugin: " << extractor_plugin_name << "\n";
    }
    if (!opt.exporter.empty() && exporter_plugin_name != opt.exporter) {
        std::cout << "  exporter resolved plugin: " << exporter_plugin_name << "\n";
    }
    std::cout << "  plugins loaded: " << pm.plugins().size() << "\n";
    for (const auto& p : pm.plugins()) {
        const char* name = (p.info && p.info->name) ? p.info->name : "(unnamed)";
        const char* kind = "(unknown)";
        const char* format = "(n/a)";
        const char* standard = "(n/a)";
        if (p.info) {
            if (p.info->kind == SNATCH_PLUGIN_KIND_EXPORTER) kind = "exporter";
            else if (p.info->kind == SNATCH_PLUGIN_KIND_TRANSFORMER) kind = "transformer";
            else if (p.info->kind == SNATCH_PLUGIN_KIND_EXTRACTOR) kind = "extractor";
            if (p.info->kind == SNATCH_PLUGIN_KIND_EXPORTER) {
                format = (p.info->format && p.info->format[0] != '\0') ? p.info->format : "(unspecified)";
                standard = (p.info->standard && p.info->standard[0] != '\0') ? p.info->standard : "(unspecified)";
            }
        }
        std::cout << "    - " << name << " (" << kind << ", format=" << format << ", standard=" << standard << ") ["
                  << p.path.string() << "]\n";
    }

    if (pm.plugins().empty()) {
        std::cerr << "error: no plugins found in search path\n";
        return 3;
    }

    const loaded_plugin* extractor = nullptr;
    if (!extractor_plugin_name.empty()) {
        extractor = pm.find_by_name_and_kind(extractor_plugin_name, SNATCH_PLUGIN_KIND_EXTRACTOR);
        if (!extractor) {
            std::cerr << "error: extractor plugin not found: " << extractor_plugin_name << "\n";
            return 3;
        }
    } else {
        extractor = pm.find_first_by_kind(SNATCH_PLUGIN_KIND_EXTRACTOR);
        if (!extractor) {
            std::cerr << "error: no extractor plugins found in search path\n";
            return 3;
        }
    }

    const loaded_plugin* transformer = nullptr;
    if (!opt.transformer.empty()) {
        transformer = pm.find_by_name_and_kind(opt.transformer, SNATCH_PLUGIN_KIND_TRANSFORMER);
        if (!transformer) {
            std::cerr << "error: transformer plugin not found: " << opt.transformer << "\n";
            return 3;
        }
    }

    const loaded_plugin* plugin = nullptr;
    if (!exporter_plugin_name.empty()) {
        plugin = pm.find_by_name_and_kind(exporter_plugin_name, SNATCH_PLUGIN_KIND_EXPORTER);
        if (!plugin) {
            std::cerr << "error: exporter plugin not found: " << exporter_plugin_name << "\n";
            return 3;
        }
    } else {
        plugin = pm.find_first_by_kind(SNATCH_PLUGIN_KIND_EXPORTER);
        if (!plugin) {
            std::cerr << "error: no exporter plugins found in search path\n";
            return 3;
        }
    }

    std::vector<std::array<std::string, 2>> extract_kv_storage;
    std::vector<snatch_kv> extract_options;
    extract_kv_storage.reserve(16);
    extract_options.reserve(16);
    append_kv_params(opt.extractor_parameters, extract_kv_storage, extract_options, {"input"});

    snatch_font plugin_font{};
    char errbuf[512] = {0};
    const int extract_rc = extractor->info->extract_font(
        input_path.c_str(),
        extract_options.empty() ? nullptr : extract_options.data(),
        static_cast<unsigned>(extract_options.size()),
        &plugin_font,
        errbuf,
        static_cast<unsigned>(sizeof(errbuf))
    );
    if (extract_rc != 0) {
        std::cerr << "error: extractor failed (" << extract_rc << ")";
        if (errbuf[0] != '\0') std::cerr << ": " << errbuf;
        std::cerr << "\n";
        return 4;
    }
    std::cout << "  extracted with plugin: " << (extractor->info && extractor->info->name ? extractor->info->name : "(unknown)") << "\n";

    std::vector<std::array<std::string, 2>> export_kv_storage;
    std::vector<snatch_kv> export_options;
    export_kv_storage.reserve(16);
    export_options.reserve(16);

    append_kv_params(opt.exporter_parameters, export_kv_storage, export_options, {"output"});
    if (transformer) {
        std::vector<std::array<std::string, 2>> transform_kv_storage;
        std::vector<snatch_kv> transform_options;
        transform_kv_storage.reserve(16);
        transform_options.reserve(16);
        append_kv_params(opt.transformer_parameters, transform_kv_storage, transform_options);

        const int transform_rc = transformer->info->transform_font(
            &plugin_font,
            transform_options.empty() ? nullptr : transform_options.data(),
            static_cast<unsigned>(transform_options.size()),
            errbuf,
            static_cast<unsigned>(sizeof(errbuf))
        );
        if (transform_rc != 0) {
            std::cerr << "error: transformer failed (" << transform_rc << ")";
            if (errbuf[0] != '\0') std::cerr << ": " << errbuf;
            std::cerr << "\n";
            return 5;
        }
        std::cout << "  transformed with plugin: " << (transformer->info && transformer->info->name ? transformer->info->name : "(unknown)") << "\n";
    }

    errbuf[0] = '\0';
    const int export_rc = plugin->info->export_font(
        &plugin_font,
        output_path.c_str(),
        export_options.empty() ? nullptr : export_options.data(),
        static_cast<unsigned>(export_options.size()),
        errbuf,
        static_cast<unsigned>(sizeof(errbuf))
    );
    if (export_rc != 0) {
        std::cerr << "error: exporter failed (" << export_rc << ")";
        if (errbuf[0] != '\0') std::cerr << ": " << errbuf;
        std::cerr << "\n";
        return 5;
    }

    std::cout << "  exported with plugin: " << (plugin->info && plugin->info->name ? plugin->info->name : "(unknown)") << "\n";
    const int extracted_glyphs = (plugin_font.bitmap_font && plugin_font.bitmap_font->glyphs) ? plugin_font.bitmap_font->glyph_count : 0;
    std::cout << "  extracted glyphs: " << extracted_glyphs << " at " << plugin_font.pixel_size << "ppem\n";
    return 0;
}
