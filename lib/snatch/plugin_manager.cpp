/// \file
/// \brief Dynamic plugin loading, discovery, and lookup implementation.
///
/// This source file implements one part of the snatch pipeline architecture. It contributes to extracting, transforming, exporting, or orchestrating bitmap data in a plugin-driven workflow.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include "snatch/plugin_manager.h"

#include <dlfcn.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "snatch/plugin.h"

namespace fs = std::filesystem;

namespace {
/// \brief debug_plugins_enabled.
bool debug_plugins_enabled() {
    const char* v = std::getenv("SNATCH_DEBUG_PLUGINS");
    return v && v[0] != '\0' && v[0] != '0';
}
}

/// \brief plugin_manager::~plugin_manager.
plugin_manager::~plugin_manager() {
    for (auto& p : plugins_) {
        if (p.handle) dlclose(p.handle);
    }
}

/// \brief plugin_manager::load_plugin_file.
bool plugin_manager::load_plugin_file(const fs::path& path) {
    if (debug_plugins_enabled()) {
        std::cerr << "[plugin] try " << path << "\n";
    }
    std::error_code ec;
    if (!fs::exists(path, ec) || !fs::is_regular_file(path, ec)) {
        if (debug_plugins_enabled()) {
            std::cerr << "[plugin] skip (missing/not regular): " << path << "\n";
        }
        return false;
    }
    if (path.extension() != ".so") {
        if (debug_plugins_enabled()) {
            std::cerr << "[plugin] skip (not .so): " << path << "\n";
        }
        return false;
    }

    void* h = dlopen(path.c_str(), RTLD_NOW);
    if (!h) {
        std::cerr << "dlopen failed: " << dlerror() << " (" << path << ")\n";
        return false;
    }

    using get_fn_t = int (*)(const snatch_plugin_info**);
    dlerror();
    auto* sym = reinterpret_cast<get_fn_t>(dlsym(h, "snatch_plugin_get"));
    const char* dler = dlerror();
    if (dler || !sym) {
        std::cerr << "dlsym snatch_plugin_get failed: " << (dler ? dler : "null") << "\n";
        dlclose(h);
        return false;
    }

    const snatch_plugin_info* info = nullptr;
    if (sym(&info) != 0 || !info) {
        std::cerr << "plugin get() failed: " << path << "\n";
        dlclose(h);
        return false;
    }

    if (info->abi_version != SNATCH_PLUGIN_ABI_VERSION) {
        std::cerr << "ABI/version mismatch in " << path << "\n";
        dlclose(h);
        return false;
    }

    const bool valid_kind =
        info->kind == SNATCH_PLUGIN_KIND_EXPORTER ||
        info->kind == SNATCH_PLUGIN_KIND_TRANSFORMER ||
        info->kind == SNATCH_PLUGIN_KIND_EXTRACTOR;
    if (!valid_kind) {
        std::cerr << "invalid plugin kind in " << path << "\n";
        dlclose(h);
        return false;
    }

    if (info->kind == SNATCH_PLUGIN_KIND_EXPORTER && !info->export_font) {
        std::cerr << "missing exporter callback in " << path << "\n";
        dlclose(h);
        return false;
    }

    if (info->kind == SNATCH_PLUGIN_KIND_EXPORTER) {
        const bool has_format = info->format && info->format[0] != '\0';
        const bool has_standard = info->standard && info->standard[0] != '\0';
        if (!has_format || !has_standard) {
            std::cerr << "missing exporter format/standard metadata in " << path << "\n";
            dlclose(h);
            return false;
        }
    }

    if (info->kind == SNATCH_PLUGIN_KIND_TRANSFORMER && !info->transform_font) {
        std::cerr << "missing transformer callback in " << path << "\n";
        dlclose(h);
        return false;
    }

    if (info->kind == SNATCH_PLUGIN_KIND_EXTRACTOR && !info->extract_font) {
        std::cerr << "missing extractor callback in " << path << "\n";
        dlclose(h);
        return false;
    }

    loaded_plugin lp;
    lp.handle = h;
    lp.info = info;
    lp.path = path;
    plugins_.push_back(lp);
    return true;
}

/// \brief plugin_manager::load_from_dir.
void plugin_manager::load_from_dir(const fs::path& dir) {
    plugins_.clear();
    std::error_code ec;
    if (debug_plugins_enabled()) {
        std::cerr << "[plugin] scan dir " << dir << "\n";
    }
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        if (debug_plugins_enabled()) {
            std::cerr << "[plugin] dir not found/invalid: " << dir << "\n";
        }
        return;
    }

    for (auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        load_plugin_file(entry.path());
    }
}

/// \brief plugin_manager::load_named_from_dir.
void plugin_manager::load_named_from_dir(const fs::path& dir, const std::vector<std::string>& names) {
    plugins_.clear();
    std::error_code ec;
    if (debug_plugins_enabled()) {
        std::cerr << "[plugin] load named from dir " << dir << "\n";
    }
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        if (debug_plugins_enabled()) {
            std::cerr << "[plugin] dir not found/invalid: " << dir << "\n";
        }
        return;
    }

    for (const auto& name : names) {
        if (name.empty()) continue;
        load_plugin_file(dir / (name + ".so"));
    }
}

/// \brief plugin_manager::load_from_dirs_in_order.
void plugin_manager::load_from_dirs_in_order(const std::vector<fs::path>& dirs) {
    for (const auto& dir : dirs) {
        load_from_dir(dir);
        if (!plugins_.empty()) {
            return;
        }
    }
}

/// \brief plugin_manager::load_named_from_dirs_in_order.
void plugin_manager::load_named_from_dirs_in_order(
    const std::vector<fs::path>& dirs,
    const std::vector<std::string>& names
) {
    for (const auto& dir : dirs) {
        load_named_from_dir(dir, names);

        bool all_found = true;
        for (const auto& name : names) {
            if (name.empty()) continue;
            if (!find_by_name(name)) {
                all_found = false;
                break;
            }
        }

        if (all_found && !plugins_.empty()) {
            return;
        }
    }
}

/// \brief plugin_manager::find_by_name.
const loaded_plugin* plugin_manager::find_by_name(const std::string& name) const {
    for (const auto& p : plugins_) {
        if (!p.info || !p.info->name) continue;
        if (name == p.info->name) return &p;
    }
    return nullptr;
}

/// \brief plugin_manager::find_by_name_and_kind.
const loaded_plugin* plugin_manager::find_by_name_and_kind(const std::string& name, int kind) const {
    for (const auto& p : plugins_) {
        if (!p.info || !p.info->name) continue;
        if (p.info->kind != kind) continue;
        if (name == p.info->name) return &p;
    }
    return nullptr;
}

/// \brief plugin_manager::find_first_by_kind.
const loaded_plugin* plugin_manager::find_first_by_kind(int kind) const {
    for (const auto& p : plugins_) {
        if (!p.info) continue;
        if (p.info->kind == kind) return &p;
    }
    return nullptr;
}
