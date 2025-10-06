#pragma once
#include "options.h"

class cli_parser {
public:
    // returns 0 on success, non-zero on error. On success, fills out.
    // Note: -h/--help is handled by argparse and will print usage & exit(0).
    int parse(int argc, const char** argv, snatch_options& out) const;

private:
    static bool parse_edge4_csv(const char* s, edge4& out);
    static bool parse_hex_color(const char* s, color_rgb& out);
    static source_format parse_source_format(const char* s);
};
