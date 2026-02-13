#include "snatch/plugin.h"

#include <cstdio>
#include <cstring>

namespace {

int write_message(FILE* f, const char* key, const char* value) {
    if (!f || !key || !value) return 1;
    return std::fprintf(f, "%s=%s\n", key, value) < 0 ? 1 : 0;
}

int dummy_export_font(
    const snatch_font* font,
    const char* output_path,
    const snatch_kv* options,
    unsigned options_count,
    char* errbuf,
    unsigned errbuf_len
) {
    if (!font) {
        if (errbuf && errbuf_len > 0) {
            std::snprintf(errbuf, errbuf_len, "dummy: font is null");
        }
        return 10;
    }

    if (!output_path || output_path[0] == '\0') {
        if (errbuf && errbuf_len > 0) {
            std::snprintf(errbuf, errbuf_len, "dummy: output path is empty");
        }
        return 11;
    }

    FILE* f = std::fopen(output_path, "w");
    if (!f) {
        if (errbuf && errbuf_len > 0) {
            std::snprintf(errbuf, errbuf_len, "dummy: cannot open output file");
        }
        return 12;
    }

    int rc = 0;
    rc |= write_message(f, "plugin", "dummy");
    rc |= write_message(f, "name", (font->name && font->name[0] != '\0') ? font->name : "(unnamed)");

    if (std::fprintf(f, "glyph_width=%d\nglyph_height=%d\n", font->glyph_width, font->glyph_height) < 0) {
        rc = 1;
    }

    if (std::fprintf(f, "options_count=%u\n", options_count) < 0) {
        rc = 1;
    }

    for (unsigned i = 0; i < options_count && options; ++i) {
        const char* key = options[i].key ? options[i].key : "";
        const char* value = options[i].value ? options[i].value : "";
        if (std::fprintf(f, "option[%u]=%s:%s\n", i, key, value) < 0) {
            rc = 1;
            break;
        }
    }

    std::fclose(f);

    if (rc != 0) {
        if (errbuf && errbuf_len > 0) {
            std::snprintf(errbuf, errbuf_len, "dummy: failed while writing output");
        }
        return 13;
    }
    return 0;
}

const snatch_plugin_info k_dummy_info = {
    "dummy",
    "Debug/testing exporter plugin that writes diagnostic text",
    "snatch project",
    SNATCH_PLUGIN_ABI_VERSION,
    &dummy_export_font
};

} // namespace

extern "C" SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out) {
    if (!out) return 1;
    *out = &k_dummy_info;
    return 0;
}
