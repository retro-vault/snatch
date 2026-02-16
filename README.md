![status.badge] [![language.badge]][language.url] [![standard.badge]][standard.url] [![license.badge]][license.url]

# snatch

`snatch` is a plugin-driven bitmap processing pipeline for retro/pixel workflows.

It is still great for bitmap fonts, but the architecture is now more generic:
- extract bitmap data from a source
- transform that data (optional)
- export in a target format

## Architecture

`snatch` runs in three stages:

1. Extractor plugin
- Reads the input source (`ttf`, `image`, etc.)
- Produces a `snatch_font` bitmap representation

2. Transformer plugin (optional)
- Reads `snatch_font`
- Can annotate or replace data through `font->user_data`

3. Exporter plugin
- Reads `snatch_font` and optional transformed data
- Writes the final artifact (`.png`, `.s`, `.bin`, `.c`, ...)

Pipeline examples:

```text
TTF -> ttf_extractor -> partner_tiny_transform -> partner_sdcc_asm_tiny -> .s
image -> image_extractor -> (no transform) -> raw_bin -> .bin
```

This is why the new model is more flexible: with appropriate plugins, you can run non-font flows too (for example: extract one glyph/region from color image -> dither transform -> 1bpp image export).

## Current Capabilities

- Extract bitmap font glyphs from image sheets (`image_extractor`)
- Rasterize TTF fonts to 1bpp glyph bitmaps (`ttf_extractor`)
- Run optional transformers before export
- Export to PNG grid, Partner SDCC ASM (tiny or bitmap), raw binary, and raw C array
- Control ASCII range, colors, margins/padding, font size, fixed/proportional mode

## Quick Start

### Build

```bash
git clone https://github.com/retro-vault/snatch.git
cd snatch
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

### Run tests

```bash
ctest --test-dir build --output-on-failure
```

## Common Workflows

### 1) TTF -> PNG grid

```bash
./bin/snatch \
  --plugin-dir ./bin/plugins \
  --extractor-parameters "input=fonts/Retro.ttf,first_ascii=32,last_ascii=126,font_size=16" \
  --exporter png \
  --exporter-parameters "output=out/font-grid.png,columns=16,rows=6"
```

### 2) Image sheet -> continuous raw binary

```bash
./bin/snatch \
  --plugin-dir ./bin/plugins \
  --extractor-parameters "input=assets/fontsheet.png,margins_left=2,margins_top=2,margins_right=2,margins_bottom=2,padding_left=1,padding_top=1,padding_right=1,padding_bottom=1,columns=16,rows=6,first_ascii=32,last_ascii=126,fore_color=#000000,back_color=#FFFFFF" \
  --exporter raw_bin \
  --exporter-parameters "output=out/font.bin"
```

### 3) TTF -> Partner Tiny SDCC ASM

```bash
./bin/snatch \
  --plugin-dir ./bin/plugins \
  --extractor-parameters "input=fonts/Retro.ttf,first_ascii=32,last_ascii=127,font_size=16" \
  --transformer partner_tiny_transform \
  --exporter partner_sdcc_asm_tiny \
  --exporter-parameters "output=out/my_font.s,module=my_font,symbol=my_font,proportional=true,space_width=3,letter_spacing=2"
```

### 4) TTF -> raw C bytes

```bash
./bin/snatch \
  --plugin-dir ./bin/plugins \
  --extractor-parameters "input=fonts/Retro.ttf,first_ascii=32,last_ascii=127,font_size=16" \
  --transformer partner_bitmap_transform \
  --transformer-parameters "font_mode=proportional,space_width=3,letter_spacing=2" \
  --exporter raw_c \
  --exporter-parameters "output=out/font_raw.c,bytes_per_line=8,symbol=font_data"
```

## Plugin Catalog

### Extractors

| Name | Input | Purpose |
|:--|:--|:--|
| `ttf_extractor` | `ttf` | Rasterize TTF glyphs into 1bpp bitmap glyphs |
| `image_extractor` | `image` | Extract glyph bitmaps from grid image sheets |
| `image_passthrough_extractor` | `image` | Load full image as grayscale passthrough payload in `user_data` |

### Transformers

| Name | Purpose | Notes |
|:--|:--|:--|
| `partner_tiny_transform` | Vectorize bitmap glyphs into Partner Tiny move streams | Intended for `partner_sdcc_asm_tiny` |
| `partner_bitmap_transform` | Serialize bitmap font to Partner bitmap byte stream | Intended for `partner_sdcc_asm_bitmap`, `raw_bin`, `raw_c` |
| `fzx-transform` | Compute ZX Spectrum FZX-style glyph metadata | Stores metadata in `font->user_data` |
| `dither_1bpp_transform` | Dither grayscale passthrough image to 1bpp bitmap glyph | Intended for `image_passthrough_extractor` + `png` |

### Exporters

| Name | Format | Standard | Purpose |
|:--|:--|:--|:--|
| `png` | `png` | `snatch-grid` | Render bitmap font as PNG grid |
| `partner_sdcc_asm_tiny` | `asm` | `partner-sdcc-asm-tiny` | SDCC assembly export for Partner tiny format |
| `partner_sdcc_asm_bitmap` | `asm` | `partner-sdcc-asm-bitmap` | SDCC assembly export for Partner bitmap format |
| `raw_bin` | `bin` | `raw-1bpp` | Raw continuous byte stream |
| `raw_c` | `c` | `raw-1bpp` | Raw byte stream as `const uint8_t[]` |
| `dummy` | `txt` | `debug-dump` | Diagnostic exporter |

## Important CLI Options

| Option | Alias | Description |
|:--|:--|:--|
| `--extractor` | `-q` | Optional extractor plugin override |
| `--extractor-parameters` | `-v` | Extractor params (`k=v,...`) |
| `--plugin-dir` | `-d` | Plugin search directory override |
| `--transformer` | `-w` | Optional transformer plugin name |
| `--transformer-parameters` | `-y` | Transformer params (`k=v,...`) |
| `--exporter` | `-e` | Exporter plugin name |
| `--exporter-parameters` | `-x` | Exporter params (`k=v,...`) |

Stage-specific tuning should be passed to the owning plugin:
- extractor options via `--extractor-parameters`
- transformer options via `--transformer-parameters`
- exporter options via `--exporter-parameters`

If `--extractor` is omitted, `snatch` infers it from input extension:
- `.ttf`, `.otf` -> `ttf_extractor`
- common image extensions (`.png`, `.jpg`, `.jpeg`, ...) -> `image_extractor`

Required ownership split:
- extractor owns input path: set `input=...` in `--extractor-parameters`
- exporter owns output path: set `output=...` in `--exporter-parameters`
- there is no positional input argument and no root `--output` option anymore

Use concrete exporter names directly (no separate format parameter):
- `partner_sdcc_asm_tiny`
- `partner_sdcc_asm_bitmap`
- `raw_c`
- `raw_bin`
- `png`

Concept example (full image passthrough -> dither -> PNG):

```bash
./bin/snatch \
  --plugin-dir ./bin/plugins \
  --extractor image_passthrough_extractor \
  --extractor-parameters "input=test/data/tut.png" \
  --transformer dither_1bpp_transform \
  --transformer-parameters "threshold=128" \
  --exporter png \
  --exporter-parameters "output=out/tut_dither_1bpp.png,columns=1,rows=1,padding=0,grid_thickness=0"
```

## Plugin Discovery

`snatch` searches plugin directories in this order and stops at the first one that provides the requested plugins:

1. `--plugin-dir <dir>`
2. `SNATCH_PLUGIN_DIR`
3. `${CMAKE_INSTALL_FULL_LIBDIR}/snatch/plugins`
4. `~/.local/lib/snatch/plugins`

## Plugin Development

Plugins export:

```c
int snatch_plugin_get(const snatch_plugin_info** out);
```

Plugin ABI: `include/snatch/plugin.h`

`snatch_plugin_info.kind`:
- `SNATCH_PLUGIN_KIND_EXTRACTOR` -> `extract_font`
- `SNATCH_PLUGIN_KIND_TRANSFORMER` -> `transform_font`
- `SNATCH_PLUGIN_KIND_EXPORTER` -> `export_font`

### Add a Plugin (Mini Template)

1. Create a plugin folder and CMake file under `plugins/<name>/`.
2. Add it to `plugins/CMakeLists.txt` with `add_subdirectory(<name>)`.
3. Implement `snatch_plugin_get(...)` and a static `snatch_plugin_info`.
4. Build and run with `--plugin-dir ./bin/plugins`.

Extractor skeleton:

```cpp
int my_extract(
    const char* input_path,
    const snatch_kv* options,
    unsigned options_count,
    snatch_font* out_font,
    char* errbuf,
    unsigned errbuf_len
);
```

Transformer skeleton:

```cpp
int my_transform(
    snatch_font* font,
    const snatch_kv* options,
    unsigned options_count,
    char* errbuf,
    unsigned errbuf_len
);
```

Exporter skeleton:

```cpp
int my_export(
    const snatch_font* font,
    const char* output_path,
    const snatch_kv* options,
    unsigned options_count,
    char* errbuf,
    unsigned errbuf_len
);
```

Minimal plugin descriptor shape:

```cpp
const snatch_plugin_info k_info = {
    "my_plugin",
    "Short description",
    "author",
    "format-or-input",
    "standard-or-profile",
    SNATCH_PLUGIN_ABI_VERSION,
    SNATCH_PLUGIN_KIND_EXPORTER, // or EXTRACTOR/TRANSFORMER
    nullptr,                     // transform callback if transformer
    nullptr,                     // export callback if exporter
    nullptr                      // extract callback if extractor
};
```

Notes:
- For transformers, use `font->user_data` for stage-to-stage contracts.
- For exporters, `format`/`standard` should be non-empty.
- Keep plugin-owned buffers alive for as long as `snatch` may read them.

## Third-party Libraries

- FreeType (`freetype`): FreeType License (FTL) or GPLv2
- stb (`stb_image`, `stb_image_write`): public domain or MIT
- argparse (`cofyc/argparse`): MIT
- GoogleTest (`googletest`): BSD 3-Clause

## Font Fixture Attribution

TTF fonts under `test/data` are freeware and remain copyright of their authors.

[language.url]:   https://en.wikipedia.org/wiki/C%2B%2B
[language.badge]: https://img.shields.io/badge/language-C%2B%2B-blue.svg

[standard.url]:   https://en.wikipedia.org/wiki/C%2B%2B20
[standard.badge]: https://img.shields.io/badge/standard-C%2B%2B20-blue.svg

[license.url]:    https://github.com/retro-vault/snatch/blob/master/LICENSE
[license.badge]:  https://img.shields.io/badge/license-GPL2-blue.svg

[status.badge]:  https://img.shields.io/badge/status-beta-orange.svg
