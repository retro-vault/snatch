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