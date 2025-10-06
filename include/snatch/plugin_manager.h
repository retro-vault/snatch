#pragma once
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

struct snatch_plugin_info; // from the C header

struct loaded_plugin {
    void* handle = nullptr;                 // dlopen handle
    const snatch_plugin_info* info = nullptr;
    std::filesystem::path path;
};

class plugin_manager {
public:
    ~plugin_manager();

    // scan a directory for *.so and load plugins
    void load_from_dir(const std::filesystem::path& dir);

    const std::vector<loaded_plugin>& plugins() const { return plugins_; }

private:
    std::vector<loaded_plugin> plugins_;
};
