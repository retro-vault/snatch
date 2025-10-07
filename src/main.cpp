#include <iostream>
#include "snatch/cli_parser.h"
#include "snatch/options.h"

static void print_options(const snatch_options& opt) {
    std::cout << "snatch options:\n";
    std::cout << "  margins: " << opt.margins.left << "," << opt.margins.top
              << "," << opt.margins.right << "," << opt.margins.bottom << "\n";
    std::cout << "  padding: " << opt.padding.left << "," << opt.padding.top
              << "," << opt.padding.right << "," << opt.padding.bottom << "\n";
    std::cout << "  grid: " << opt.columns << " cols x " << opt.rows << " rows\n";
    std::cout << "  source format: "
              << (opt.src_fmt==source_format::image ? "image" :
                  opt.src_fmt==source_format::ttf   ? "ttf"   : "unknown") << "\n";
    std::cout << "  input:  " << opt.input_file << "\n";
    std::cout << "  output: " << (opt.output_file.empty() ? "(none)" : opt.output_file.string()) << "\n";
    std::cout << "  exporter: " << (opt.exporter.empty() ? "(none)" : opt.exporter) << "\n";
    std::cout << "  exporter params: " << (opt.exporter_parameters.empty() ? "(none)" : opt.exporter_parameters) << "\n";
    std::cout << "  inverse: " << (opt.inverse ? "yes" : "no") << "\n";
    std::cout << "  fore color: #" << std::hex
              << (opt.fore_color.r<16?"0":"") << opt.fore_color.r
              << (opt.fore_color.g<16?"0":"") << opt.fore_color.g
              << (opt.fore_color.b<16?"0":"") << opt.fore_color.b << std::dec << "\n";
    std::cout << "  back color: #" << std::hex
              << (opt.back_color.r<16?"0":"") << opt.back_color.r
              << (opt.back_color.g<16?"0":"") << opt.back_color.g
              << (opt.back_color.b<16?"0":"") << opt.back_color.b << std::dec << "\n";
    if (opt.has_transparent) {
        std::cout << "  transparent color: #" << std::hex
                << (opt.transparent_color.r<16?"0":"") << opt.transparent_color.r
                << (opt.transparent_color.g<16?"0":"") << opt.transparent_color.g
                << (opt.transparent_color.b<16?"0":"") << opt.transparent_color.b
                << std::dec << "\n";
    }
    std::cout << "  ascii: first=" << opt.first_ascii << " last=" << opt.last_ascii << "\n";
}

int main(int argc, const char** argv) {
    snatch_options opt;
    cli_parser parser;
    int rc = parser.parse(argc, argv, opt);
    if (rc) return rc;

    // TODO: do real work (load TTF/image, pipeline/export, etc.)
    print_options(opt);
    return 0;
}
