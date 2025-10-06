#include "snatch/cli_parser.h"
#include <iostream>
#include <cstdlib>
#include <cstring>



extern "C" {
#include <argparse.h>
}

// ---- helpers -------------------------------------------------------------

bool cli_parser::parse_edge4_csv(const char* s, edge4& out) {
    if (!s) return false;
    int vals[4] = {0,0,0,0};
    int count = 0;
    const char* p = s;
    while (*p && count < 4) {
        char* end = nullptr;
        long v = std::strtol(p, &end, 10);
        if (end == p || v < 0 || v > 1000000) return false;
        vals[count++] = static_cast<int>(v);
        if (*end == ',') { p = end + 1; }
        else { p = end; break; }
    }
    while (*p == ' ') ++p;
    if (*p != '\0') return false;
    out.left = vals[0];
    if (count > 1) out.top = vals[1];
    if (count > 2) out.right = vals[2];
    if (count > 3) out.bottom = vals[3];
    return true;
}

static bool parse_hex_component(const char* p, int& out) {
    char* end = nullptr;
    long v = std::strtol(p, &end, 16);
    if (end == p || v < 0 || v > 255) return false;
    out = static_cast<int>(v);
    return true;
}

bool cli_parser::parse_hex_color(const char* s, color_rgb& out) {
    if (!s) return false;
    if (s[0] == '#') ++s;
    size_t len = std::strlen(s);
    if (len != 6) return false;
    int r=0,g=0,b=0;
    char rs[3] = { s[0], s[1], 0 };
    char gs[3] = { s[2], s[3], 0 };
    char bs[3] = { s[4], s[5], 0 };
    if (!parse_hex_component(rs, r)) return false;
    if (!parse_hex_component(gs, g)) return false;
    if (!parse_hex_component(bs, b)) return false;
    out.r = r; out.g = g; out.b = b;
    return true;
}

source_format cli_parser::parse_source_format(const char* s) {
    if (!s) return source_format::unknown;
    if (std::strcmp(s, "image") == 0) return source_format::image;
    if (std::strcmp(s, "ttf")   == 0) return source_format::ttf;
    return source_format::unknown;
}

// ---- parse ---------------------------------------------------------------

int cli_parser::parse(int argc, const char** argv, snatch_options& out) const {
    // argparse target variables
    const char* margins_str = nullptr;
    const char* padding_str = nullptr;
    const char* src_fmt_str = nullptr;
    const char* exporter_str = nullptr;
    const char* exporter_params_str = nullptr;
    const char* out_file_str = nullptr;
    const char* fore_str = nullptr;
    const char* back_str = nullptr;

    int columns = 0;
    int rows = 0;
    int inverse_flag = 0;
    int first_ascii = -1;
    int last_ascii = -1;

    const char* const usage[] = {
        "snatch [options] <input_file>",
        nullptr
    };

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    
    // Use macros to avoid missing-field warnings in C++
    struct argparse_option options[] = {
        // margins & padding
        OPT_STRING('m', "margins", &margins_str,
                "image margins: left,top,right,bottom (up to 4)"),
        OPT_STRING(  0, "padding", &padding_str,
                "glyph padding: left,top,right,bottom (up to 4)"),

        // grid
        OPT_INTEGER('c', "columns", &columns, "number of columns"),
        OPT_INTEGER('r', "rows",    &rows,    "number of rows"),

        // source format
        OPT_STRING(  0, "source-format", &src_fmt_str, "source format: image|ttf"),
        OPT_STRING('s', "sf",            &src_fmt_str, "alias for --source-format"),

        // output file
        OPT_STRING('o', "output",  &out_file_str, "output filename"),

        // exporter and parameters
        OPT_STRING('e', "exporter",            &exporter_str,         "exporter name (plugin/tool)"),
        OPT_STRING(  0, "exporter-parameters", &exporter_params_str,  "parameters for exporter (quoted ok)"),
        OPT_STRING('p', "ep",                  &exporter_params_str,  "alias for --exporter-parameters"),

        // inverse
        OPT_BOOLEAN('i', "inverse", &inverse_flag, "invert image"),

        // colors
        OPT_STRING(  0, "fore-color", &fore_str, "foreground color #RRGGBB"),
        OPT_STRING(  0, "fc",         &fore_str, "alias for --fore-color"),
        OPT_STRING(  0, "back-color", &back_str, "background color #RRGGBB"),
        OPT_STRING(  0, "bc",         &back_str, "alias for --back-color"),

        // ascii range
        OPT_INTEGER( 0, "first-ascii", &first_ascii, "first ascii code"),
        OPT_INTEGER( 0, "fa",          &first_ascii, "alias for --first-ascii"),
        OPT_INTEGER( 0, "last-ascii",  &last_ascii,  "last ascii code"),
        OPT_INTEGER( 0, "la",          &last_ascii,  "alias for --last-ascii"),

        OPT_HELP(),
        OPT_END()
    };

#pragma GCC diagnostic pop

    struct argparse ap{};
    argparse_init(&ap, options, usage, 0);
    argparse_describe(&ap, "snatch font processor",
                           "example: snatch --source-format ttf -o out.bin MyFont.ttf");
    int nargs = argparse_parse(&ap, argc, argv);

    // positional input file required
    if (nargs < 1) {
        std::cerr << "error: input file is required\n";
        return 1;
    }

    // fill output struct
    out.input_file = std::filesystem::path(argv[argc - nargs]);
    out.columns = columns;
    out.rows = rows;
    out.inverse = (inverse_flag != 0);
    if (out_file_str) out.output_file = out_file_str;
    if (exporter_str) out.exporter = exporter_str;
    if (exporter_params_str) out.exporter_parameters = exporter_params_str;
    out.src_fmt = parse_source_format(src_fmt_str);

    if (margins_str && !parse_edge4_csv(margins_str, out.margins)) {
        std::cerr << "error: invalid --margins; expected up to 4 positive integers (left,top,right,bottom)\n";
        return 2;
    }
    if (padding_str && !parse_edge4_csv(padding_str, out.padding)) {
        std::cerr << "error: invalid --padding; expected up to 4 positive integers (left,top,right,bottom)\n";
        return 2;
    }

    if (fore_str && !parse_hex_color(fore_str, out.fore_color)) {
        std::cerr << "error: invalid --fore-color; expected #RRGGBB\n";
        return 2;
    }
    if (back_str && !parse_hex_color(back_str, out.back_color)) {
        std::cerr << "error: invalid --back-color; expected #RRGGBB\n";
        return 2;
    }

    out.first_ascii = first_ascii;
    out.last_ascii  = last_ascii;
    if ((out.first_ascii >= 0 && out.last_ascii >= 0) && out.first_ascii > out.last_ascii) {
        std::cerr << "error: --first-ascii must be <= --last-ascii\n";
        return 2;
    }

    if (out.src_fmt == source_format::unknown) {
        std::cerr << "warning: --source-format not set (use image|ttf)\n";
    }
    if (out.output_file.empty()) {
        std::cerr << "warning: no --output specified\n";
    }
    return 0;
}
