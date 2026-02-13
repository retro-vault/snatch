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
    // scan directories in order and stop at the first directory
    // that yields at least one valid plugin.
    void load_from_dirs_in_order(const std::vector<std::filesystem::path>& dirs);

    const std::vector<loaded_plugin>& plugins() const { return plugins_; }
    const loaded_plugin* find_by_name(const std::string& name) const;

private:
    std::vector<loaded_plugin> plugins_;
};
