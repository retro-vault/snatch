/// \file
/// \brief Core plugin ABI contracts and function signatures.
///
/// This header declares types and contracts used by snatch core and plugin stages. It is part of the extractor-transformer-exporter architecture and is consumed by build-time and runtime plugin integration.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#pragma once

// Stable C ABI for plugins
#ifdef __cplusplus
extern "C" {
#endif

// ABI versioning
#define SNATCH_PLUGIN_ABI_VERSION 5

// symbol visibility (gcc/clang)
#if defined(__GNUC__) || defined(__clang__)
#define SNATCH_PLUGIN_API __attribute__((visibility("default")))
#else
#define SNATCH_PLUGIN_API
#endif

// minimal font description for now; you can extend later
typedef struct snatch_glyph_bitmap {
    int codepoint;              // Unicode codepoint / ASCII value
    int width;                  // glyph bitmap width in pixels
    int height;                 // glyph bitmap height in pixels
    int bearing_x;              // horizontal bearing from pen position
    int bearing_y;              // vertical bearing from baseline
    int advance_x;              // horizontal advance in pixels
    int stride_bytes;           // bytes per row in bitmap (1bpp packed)
    const unsigned char* data;  // packed bits (MSB first per byte)
} snatch_glyph_bitmap;

typedef struct snatch_bitmap_font {
    int glyph_count;
    const snatch_glyph_bitmap* glyphs;
} snatch_bitmap_font;

typedef struct snatch_font {
    const char* name;         // e.g., "MyFont Regular"
    int glyph_width;          // px
    int glyph_height;         // px
    int first_codepoint;      // usually first ASCII codepoint
    int last_codepoint;       // usually last ASCII codepoint
    int pixel_size;           // ppem used during rasterization
    const snatch_bitmap_font* bitmap_font; // optional rasterized glyph data
    const void* user_data;    // optional: points to raw glyph data, etc.
} snatch_font;

// simple key=value option (plugins can accept arbitrary params)
typedef struct snatch_kv {
    const char* key;
    const char* value;
} snatch_kv;

typedef enum snatch_plugin_kind {
    SNATCH_PLUGIN_KIND_EXPORTER = 1,
    SNATCH_PLUGIN_KIND_TRANSFORMER = 2,
    SNATCH_PLUGIN_KIND_EXTRACTOR = 3
} snatch_plugin_kind;

// extractor reads source input and produces a bitmap font for downstream stages.
typedef int (*snatch_extract_fn)(
    const char* input_path,
    const snatch_kv* options,      // array
    unsigned options_count,
    snatch_font* out_font,         // required output
    char* errbuf,                  // optional; plugin writes a human message
    unsigned errbuf_len
);

// exporter returns 0 on success; nonzero on error (fill errbuf if provided)
typedef int (*snatch_export_fn)(
    const snatch_font* font,
    const char* output_path,
    const snatch_kv* options,      // array
    unsigned options_count,
    char* errbuf,                  // optional; plugin writes a human message
    unsigned errbuf_len
);

// transformer mutates font metadata/user_data before export.
// it must not outlive the font object supplied by snatch.
typedef int (*snatch_transform_fn)(
    snatch_font* font,
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
    const char* format;            // exporter output format, e.g. "asm","bin","c"
    const char* standard;          // exporter standard/profile, e.g. "partner-f","zx-fzx"
    unsigned    abi_version;       // must be SNATCH_PLUGIN_ABI_VERSION
    snatch_plugin_kind kind;       // exporter, transformer or extractor
    snatch_transform_fn transform_font; // required for transformers
    snatch_export_fn export_font;  // required for exporters
    snatch_extract_fn extract_font; // required for extractors
} snatch_plugin_info;

// REQUIRED entry point symbol that snatch looks up with dlsym():
//   int snatch_plugin_get(const snatch_plugin_info** out);
// Returns 0 on success, nonzero on failure. *out must point to a static object.
SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out);

#ifdef __cplusplus
} // extern "C"
#endif
