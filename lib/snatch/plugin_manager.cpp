#include "snatch/plugin_manager.h"
#include <dlfcn.h>
#include <iostream>
#include <filesystem>
#include "snatch/plugin.h"

namespace fs = std::filesystem;

plugin_manager::~plugin_manager() {
    for (auto& p : plugins_) {
        if (p.handle) dlclose(p.handle);
    }
}

void plugin_manager::load_from_dir(const fs::path& dir) {
    plugins_.clear();
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        return; // not fatal; just no plugins
    }
    for (auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext != ".so") continue;

        void* h = dlopen(entry.path().c_str(), RTLD_NOW);
        if (!h) {
            std::cerr << "dlopen failed: " << dlerror() << " (" << entry.path() << ")\n";
            continue;
        }
        using get_fn_t = int (*)(const snatch_plugin_info**);
        dlerror(); // clear
        auto* sym = reinterpret_cast<get_fn_t>(dlsym(h, "snatch_plugin_get"));
        const char* dler = dlerror();
        if (dler || !sym) {
            std::cerr << "dlsym snatch_plugin_get failed: " << (dler ? dler : "null") << "\n";
            dlclose(h);
            continue;
        }
        const snatch_plugin_info* info = nullptr;
        if (sym(&info) != 0 || !info) {
            std::cerr << "plugin get() failed: " << entry.path() << "\n";
            dlclose(h);
            continue;
        }
        if (info->abi_version != SNATCH_PLUGIN_ABI_VERSION || !info->export_font) {
            std::cerr << "ABI/version mismatch or missing export in " << entry.path() << "\n";
            dlclose(h);
            continue;
        }
        loaded_plugin lp;
        lp.handle = h;
        lp.info = info;
        lp.path = entry.path();
        plugins_.push_back(lp);
    }
}
