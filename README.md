![status.badge] [![language.badge]][language.url] [![standard.badge]][standard.url] [![license.badge]][license.url]

# snatch

snatch is a small utility that extracts bitmap fonts from images or TTF files — a font “snatcher” for developers, pixel artists, and retro game enthusiasts.

snatch takes a PNG image containing a full printed font sheet (or a TrueType font) and automatically slices it into individual character bitmaps. It’s perfect for converting fonts you find online into usable bitmap font assets for games, demos, or embedded systems.

# How it works

You provide a few simple parameters describing the layout of the source image:
 - margin – space around the font grid
 - padding – spacing between characters
 - columns / rows – how many characters per line and column
 - character width / height – size of each letter in pixels
 - first ASCII code – the starting character (e.g. 32 for space)
snatch then cuts the image grid character by character, assigning ASCII codes in sequence and producing a full bitmap font ready for use.

# Building and testing

snatch uses **CMake** and **GoogleTest** for building and unit testing. It builds cleanly on Linux (GCC or Clang).

## Build the project

```bash
# clone and enter the repo
git clone https://github.com/retro-vault/snatch.git
cd snatch

# create and enter the build folder
mkdir build && cd build

# configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Debug

# build everything (app, library, and tests)
make -j$(nproc)
```

The resulting binaries will be in:

```
bin/snatch           → main command-line tool  
test/snatch_tests    → GoogleTest-based unit tests
```

## Run the tests

To run all tests you can invoke the test binary directly:

```bash
./bin/snatch_tests
```

## Run snatch

### Command-line options

| Option | Alias | Arguments | Description |
|:--|:--|:--|:--|
| `--margins` | `-m` | *l,t,r,b* | Margins around the entire font grid (in pixels). Up to 4 comma-separated numbers: left, top, right, bottom. Missing values repeat the last one. Example: `-m 4,8` → left=4, top=8, right=8, bottom=8. |
| `--padding` | `-p` | *l,t,r,b* | Padding around each individual glyph (in pixels). Same format as margins. |
| `--columns` | `-c` | *n* | Number of glyphs per row in the image. |
| `--rows` | `-r` | *n* | Number of rows of glyphs in the image. |
| `--source-format` | `-s` | *ttf* \| *image* | Specifies the input type: either a TrueType font (`ttf`) or a bitmap image (`image`). |
| *(positional)* | — | *filename* | Input file to process (e.g. `font.png` or `font.ttf`). |
| `--output` | `-o` | *file* | Output filename for the exported bitmap font. |
| `--plugin-dir` | `-d` | *dir* | Override plugin lookup directory (highest priority). |
| `--exporter` | `-e` | *name* | Name of the exporter plugin to use (e.g. `bin`, `asm`, `json`). |
| `--exporter-parameters` | `-x` | *string* | Optional quoted string of extra parameters to pass to the exporter plugin. |
| `--inverse` | `-i` | — | Inverts image colors before processing (useful for white-on-black images). |
| `--fore-color` | `-f` | *hexcolor* | Foreground (glyph) color, e.g. `#FFFFFF` or `A0B1C2`. |
| `--back-color` | `-b` | *hexcolor* | Background color, same format as above. |
| `--transparent-color` | `-t` | *hexcolor* | Transparent color, same format as above. |
| `--first-ascii` | `-a` | *n* | ASCII code of the first glyph in the grid (usually 32 for space). |
| `--last-ascii` | `-z` | *n* | ASCII code of the last glyph in the grid (usually 126 for tilde). |
| `--font-size` | `-u` | *ppem* | Pixel size for TTF rasterization. If omitted, snatch auto-selects a size. |
| `--help` | `-h` | — | Displays help and usage information. |


### Examples

Extract a bitmap font from a PNG font sheet:

```bash
./bin/snatch \
  -sf image \
  -m 2,2,2,2 \
  -p 1 \
  -c 16 -r 6 \
  -a 32 -z 126 \
  -f "#000000" -b "#FFFFFF" \
  -e bin -o output/font.bin \
  assets/fontsheet.png
```

Convert a TTF file to a PNG glyph grid (1bpp, anti-aliasing off):

```bash
./bin/snatch \
  --plugin-dir ./bin/plugins \
  --source-format ttf \
  --exporter png \
  --columns 16 --rows 6 \
  --first-ascii 32 --last-ascii 126 \
  --output out/font-grid.png \
  fonts/Retro.ttf
```

Convert a TTF to PNG with fixed size and padding:

```bash
./bin/snatch \
  --plugin-dir ./bin/plugins \
  --source-format ttf \
  --exporter png \
  --columns 16 --rows 6 \
  --first-ascii 32 \
  --last-ascii 126 \
  --font-size 16 \
  --exporter-parameters "padding=3" \
  --output out/font-grid.png \
  fonts/Retro.ttf
```

# Planned Features

 - Extracts bitmap fonts from PNG font sheets
 - Imports TTF fonts and converts them into bitmap grids
 - Creates printed font sheets from any supported font
 - Flexible layout control (margin, padding, grid size, etc.)
 - Plugin-based export system — output to multiple bitmap font formats
 - Lightweight and fast command-line tool
 - Ideal for retro, pixel-art, and embedded projects

# Export plugins

snatch supports modular exporters for different bitmap font formats (e.g. .fnt, .bdf, .bin, .json, or custom binary layouts). You can write your own plugin to match your engine’s needs.

## Plugin development

An example plugin is included at `plugins/dummy/dummy_plugin.cpp`.

- Plugin builds produce `.so` files in `bin/plugins`.
- The plugin ABI is defined in `include/snatch/plugin.h`.
- A plugin must export:

```c
int snatch_plugin_get(const snatch_plugin_info** out);
```

The `dummy` plugin is intentionally minimal and writes a debug text file from `export_font`, so you can use it to validate dynamic loading, ABI compatibility, and debugger setup.

### How snatch finds plugins

`snatch` scans directories in priority order and stops at the first directory that contains at least one valid plugin (`*.so` implementing `snatch_plugin_get` with matching ABI).

Search order:

1. `--plugin-dir <dir>` CLI override
2. `SNATCH_PLUGIN_DIR` environment variable
3. compiled default install path: `${CMAKE_INSTALL_FULL_LIBDIR}/snatch/plugins`
4. user-local fallback: `~/.local/lib/snatch/plugins`

Notes:

- The first directory that yields at least one loadable plugin wins.
- Plugins from later directories are not loaded once a match is found.
- Non-plugin `.so` files or ABI-mismatched plugins are skipped with an error message.

### TTF rasterization behavior

- TTF glyphs are rasterized as 1bpp bitmaps (anti-aliasing off).
- FreeType render flags used by snatch are monochrome (`FT_LOAD_MONOCHROME` + `FT_LOAD_TARGET_MONO`).
- If `--font-size` is omitted, snatch auto-selects a size:
  - prefers embedded fixed bitmap strikes (when present)
  - otherwise runs a heuristic scan over common sizes

## Third-party libraries and licenses

- FreeType (`freetype`): FreeType License (FTL) or GPLv2
- stb (`stb_image`, `stb_image_write`): public domain or MIT license
- argparse (`cofyc/argparse`): MIT license
- GoogleTest (`googletest`): BSD 3-Clause license

## Font fixture attribution

TTF fonts included under `test/data` are freeware and remain copyright (c) their respective authors.

Thank you to all font authors for making these fonts available.

Examples:

```bash
# 1) Force plugin path explicitly
./bin/snatch --plugin-dir ./bin/plugins -s image -o out.bin assets/font.png

# 2) Use environment variable
SNATCH_PLUGIN_DIR=./bin/plugins ./bin/snatch -s ttf -o out.bin fonts/font.ttf
```

[language.url]:   https://en.wikipedia.org/wiki/C%2B%2B
[language.badge]: https://img.shields.io/badge/language-C%2B%2B-blue.svg

[standard.url]:   https://en.wikipedia.org/wiki/C%2B%2B20
[standard.badge]: https://img.shields.io/badge/standard-C%2B%2B20-blue.svg

[license.url]:    https://github.com/retro-vault/snatch/blob/master/LICENSE
[license.badge]:  https://img.shields.io/badge/license-GPL2-blue.svg

[status.badge]:  https://img.shields.io/badge/status-unstable-red.svg
