#include "snatch/plugin.h"
#include "snatch/plugin_util.h"

#include <fstream>
#include <sstream>
#include <string>

namespace {

int dummy_export_font(
    const snatch_font* font,
    const char* output_path,
    const snatch_kv* options,
    unsigned options_count,
    char* errbuf,
    unsigned errbuf_len
) {
    if (!font) {
        plugin_set_err(errbuf, errbuf_len, "dummy: font is null");
        return 10;
    }

    if (!output_path || output_path[0] == '\0') {
        plugin_set_err(errbuf, errbuf_len, "dummy: output path is empty");
        return 11;
    }

    std::ofstream out{output_path, std::ios::out | std::ios::trunc};
    if (!out.is_open()) {
        plugin_set_err(errbuf, errbuf_len, "dummy: cannot open output file");
        return 12;
    }

    std::ostringstream text;
    text << "plugin=dummy\n";
    text << "name=" << ((font->name && font->name[0] != '\0') ? font->name : "(unnamed)") << '\n';
    text << "glyph_width=" << font->glyph_width << '\n';
    text << "glyph_height=" << font->glyph_height << '\n';
    text << "options_count=" << options_count << '\n';

    for (unsigned i = 0; i < options_count && options; ++i) {
        const char* key = options[i].key ? options[i].key : "";
        const char* value = options[i].value ? options[i].value : "";
        text << "option[" << i << "]=" << key << ":" << value << '\n';
    }

    out << text.str();

    if (!out.good()) {
        plugin_set_err(errbuf, errbuf_len, "dummy: failed while writing output");
        return 13;
    }
    return 0;
}

const snatch_plugin_info k_dummy_info = {
    "dummy",
    "Debug/testing exporter plugin that writes diagnostic text",
    "snatch project",
    "txt",
    "debug-dump",
    SNATCH_PLUGIN_ABI_VERSION,
    SNATCH_PLUGIN_KIND_EXPORTER,
    nullptr,
    &dummy_export_font
};

} // namespace

extern "C" SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out) {
    if (!out) return 1;
    *out = &k_dummy_info;
    return 0;
}
