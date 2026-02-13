#include <iostream>
#include <filesystem>
#include <vector>
#include <cstdlib>
#include <string>
#include <sstream>
#include <array>
#include <cctype>
#include "snatch/cli_parser.h"
#include "snatch/options.h"
#include "snatch/plugin.h"
#include "snatch/plugin_manager.h"
#include "snatch/ttf_extractor.h"

static void print_options(const snatch_options& opt) {
    std::cout << "snatch options:\n";
    std::cout << "  margins: " << opt.margins.left << "," << opt.margins.top
              << "," << opt.margins.right << "," << opt.margins.bottom << "\n";
    std::cout << "  padding: " << opt.padding.left << "," << opt.padding.top
              << "," << opt.padding.right << "," << opt.padding.bottom << "\n";
    std::cout << "  grid: " << opt.columns << " cols x " << opt.rows << " rows\n";
    std::cout << "  source format: "
              << (opt.src_fmt==source_format::image ? "image" :
                  opt.src_fmt==source_format::ttf   ? "ttf"   : "unknown") << "\n";
    std::cout << "  input:  " << opt.input_file << "\n";
    std::cout << "  output: " << (opt.output_file.empty() ? "(none)" : opt.output_file.string()) << "\n";
    std::cout << "  plugin dir: " << (opt.plugin_dir.empty() ? "(none)" : opt.plugin_dir.string()) << "\n";
    std::cout << "  exporter: " << (opt.exporter.empty() ? "(none)" : opt.exporter) << "\n";
    std::cout << "  exporter params: " << (opt.exporter_parameters.empty() ? "(none)" : opt.exporter_parameters) << "\n";
    std::cout << "  inverse: " << (opt.inverse ? "yes" : "no") << "\n";
    std::cout << "  fore color: #" << std::hex
              << (opt.fore_color.r<16?"0":"") << opt.fore_color.r
              << (opt.fore_color.g<16?"0":"") << opt.fore_color.g
              << (opt.fore_color.b<16?"0":"") << opt.fore_color.b << std::dec << "\n";
    std::cout << "  back color: #" << std::hex
              << (opt.back_color.r<16?"0":"") << opt.back_color.r
              << (opt.back_color.g<16?"0":"") << opt.back_color.g
              << (opt.back_color.b<16?"0":"") << opt.back_color.b << std::dec << "\n";
    if (opt.has_transparent) {
        std::cout << "  transparent color: #" << std::hex
                << (opt.transparent_color.r<16?"0":"") << opt.transparent_color.r
                << (opt.transparent_color.g<16?"0":"") << opt.transparent_color.g
                << (opt.transparent_color.b<16?"0":"") << opt.transparent_color.b
                << std::dec << "\n";
    }
    std::cout << "  ascii: first=" << opt.first_ascii << " last=" << opt.last_ascii << "\n";
    std::cout << "  font size: " << (opt.font_size > 0 ? std::to_string(opt.font_size) : "auto") << "\n";
}

static std::string trim_copy(std::string s) {
    const auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && is_space(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && is_space(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

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

static void append_exporter_params(
    const std::string& raw,
    std::vector<std::array<std::string, 2>>& storage,
    std::vector<snatch_kv>& out
) {
    if (raw.empty()) return;
    std::stringstream ss(raw);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token = trim_copy(token);
        if (token.empty()) continue;
        const auto eq = token.find('=');
        if (eq == std::string::npos) {
            add_kv(storage, out, token, "");
            continue;
        }
        add_kv(storage, out, trim_copy(token.substr(0, eq)), trim_copy(token.substr(eq + 1)));
    }
}

int main(int argc, const char** argv) {
    snatch_options opt;
    cli_parser parser;
    int rc = parser.parse(argc, argv, opt);
    if (rc) return rc;

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

    pm.load_from_dirs_in_order(plugin_dirs);
    print_options(opt);
    std::cout << "  plugins loaded: " << pm.plugins().size() << "\n";
    for (const auto& p : pm.plugins()) {
        const char* name = (p.info && p.info->name) ? p.info->name : "(unnamed)";
        std::cout << "    - " << name << " [" << p.path.string() << "]\n";
    }

    if (opt.src_fmt != source_format::ttf) {
        std::cerr << "error: only --source-format ttf pipeline is implemented right now\n";
        return 3;
    }
    if (opt.output_file.empty()) {
        std::cerr << "error: --output is required for export\n";
        return 3;
    }
    if (pm.plugins().empty()) {
        std::cerr << "error: no plugins found in search path\n";
        return 3;
    }

    const loaded_plugin* plugin = nullptr;
    if (!opt.exporter.empty()) {
        plugin = pm.find_by_name(opt.exporter);
        if (!plugin) {
            std::cerr << "error: exporter plugin not found: " << opt.exporter << "\n";
            return 3;
        }
    } else {
        plugin = &pm.plugins().front();
    }

    ttf_extractor extractor;
    extracted_font extracted;
    std::string extract_err;
    if (!extractor.extract(opt, extracted, extract_err)) {
        std::cerr << "error: ttf extraction failed: " << extract_err << "\n";
        return 4;
    }

    std::vector<std::array<std::string, 2>> kv_storage;
    std::vector<snatch_kv> export_options;
    kv_storage.reserve(16);
    export_options.reserve(16);

    if (opt.columns > 0) add_kv(kv_storage, export_options, "columns", std::to_string(opt.columns));
    if (opt.rows > 0) add_kv(kv_storage, export_options, "rows", std::to_string(opt.rows));
    add_kv(kv_storage, export_options, "first_ascii", std::to_string(extracted.first_codepoint));
    add_kv(kv_storage, export_options, "last_ascii", std::to_string(extracted.last_codepoint));
    add_kv(kv_storage, export_options, "pixel_size", std::to_string(extracted.pixel_size));
    append_exporter_params(opt.exporter_parameters, kv_storage, export_options);

    snatch_font plugin_font = extracted.as_plugin_font();
    char errbuf[512] = {0};
    const int export_rc = plugin->info->export_font(
        &plugin_font,
        opt.output_file.string().c_str(),
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
    std::cout << "  extracted glyphs: " << extracted.bitmap_view.glyph_count << " at " << extracted.pixel_size << "ppem\n";
    return 0;
}
