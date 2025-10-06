#pragma once

// Stable C ABI for plugins
#ifdef __cplusplus
extern "C" {
#endif

// ABI versioning
#define SNATCH_PLUGIN_ABI_VERSION 1

// symbol visibility (gcc/clang)
#if defined(__GNUC__) || defined(__clang__)
#define SNATCH_PLUGIN_API __attribute__((visibility("default")))
#else
#define SNATCH_PLUGIN_API
#endif

// minimal font description for now; you can extend later
typedef struct snatch_font {
    const char* name;         // e.g., "MyFont Regular"
    int glyph_width;          // px
    int glyph_height;         // px
    const void* user_data;    // optional: points to raw glyph data, etc.
} snatch_font;

// simple key=value option (plugins can accept arbitrary params)
typedef struct snatch_kv {
    const char* key;
    const char* value;
} snatch_kv;

// exporter returns 0 on success; nonzero on error (fill errbuf if provided)
typedef int (*snatch_export_fn)(
    const snatch_font* font,
    const char* output_path,
    const snatch_kv* options,      // array
    unsigned options_count,
    char* errbuf,                  // optional; plugin writes a human message
    unsigned errbuf_len
);

// constant plugin metadata (owned by the plugin; do not free)
typedef struct snatch_plugin_info {
    const char* name;              // short id, e.g., "txt"
    const char* description;       // human friendly
    const char* author;            // optional
    unsigned    abi_version;       // must be SNATCH_PLUGIN_ABI_VERSION
    snatch_export_fn export_font;  // required
} snatch_plugin_info;

// REQUIRED entry point symbol that snatch looks up with dlsym():
//   int snatch_plugin_get(const snatch_plugin_info** out);
// Returns 0 on success, nonzero on failure. *out must point to a static object.
SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out);

#ifdef __cplusplus
} // extern "C"
#endif