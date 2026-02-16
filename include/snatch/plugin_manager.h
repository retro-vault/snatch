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
    // load specific plugin file names from a directory (name -> name.so)
    void load_named_from_dir(const std::filesystem::path& dir, const std::vector<std::string>& names);
    // scan directories in order and stop at the first directory
    // that yields at least one valid plugin.
    void load_from_dirs_in_order(const std::vector<std::filesystem::path>& dirs);
    // scan directories in order and stop at the first directory
    // that resolves all requested plugin names.
    void load_named_from_dirs_in_order(
        const std::vector<std::filesystem::path>& dirs,
        const std::vector<std::string>& names
    );

    const std::vector<loaded_plugin>& plugins() const { return plugins_; }
    const loaded_plugin* find_by_name(const std::string& name) const;
    const loaded_plugin* find_by_name_and_kind(const std::string& name, int kind) const;
    const loaded_plugin* find_first_by_kind(int kind) const;

private:
    bool load_plugin_file(const std::filesystem::path& path);
    std::vector<loaded_plugin> plugins_;
};
