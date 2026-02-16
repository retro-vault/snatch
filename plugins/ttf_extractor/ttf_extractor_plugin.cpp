#include "snatch/plugin.h"
#include "snatch/plugin_util.h"

#include "snatch/extracted_font.h"
#include "snatch/ttf_extractor.h"

#include <optional>
#include <string>

namespace {

struct ttf_extract_owner {
    extracted_font font{};
    snatch_font view{};
};

static ttf_extract_owner g_owner;

std::optional<int> parse_int_kv(const plugin_kv_view& kv, std::string_view key) {
    if (const auto raw = kv.get(key); raw && !raw->empty()) {
        return plugin_parse_int(*raw);
    }
    return std::nullopt;
}

bool parse_proportional(const plugin_kv_view& kv, bool fallback, char* errbuf, unsigned errbuf_len) {
    if (const auto mode = kv.get("font_mode"); mode && !mode->empty()) {
        if (*mode == "fixed") return false;
        if (*mode == "proportional") return true;
        plugin_set_err(errbuf, errbuf_len, "ttf_extractor: font_mode must be fixed|proportional");
        return fallback;
    }
    return plugin_parse_bool(kv.get("proportional"), fallback);
}

int extract_ttf(
    const char* input_path,
    const snatch_kv* options,
    unsigned options_count,
    snatch_font* out_font,
    char* errbuf,
    unsigned errbuf_len
) {
    if (!input_path || input_path[0] == '\0') {
        plugin_set_err(errbuf, errbuf_len, "ttf_extractor: input path is empty");
        return 10;
    }
    if (!out_font) {
        plugin_set_err(errbuf, errbuf_len, "ttf_extractor: out_font is null");
        return 11;
    }

    const plugin_kv_view kv{options, options_count};

    ttf_extract_options opt{};
    opt.input_file = input_path;

    if (const auto v = parse_int_kv(kv, "first_ascii"); v.has_value()) opt.first_ascii = *v;
    if (const auto v = parse_int_kv(kv, "last_ascii"); v.has_value()) opt.last_ascii = *v;
    if (const auto v = parse_int_kv(kv, "font_size"); v.has_value()) opt.font_size = *v;

    opt.proportional = parse_proportional(kv, false, errbuf, errbuf_len);
    if (errbuf && errbuf[0] != '\0') return 12;

    ttf_extractor extractor;
    extracted_font extracted;
    std::string extract_err;
    if (!extractor.extract(opt, extracted, extract_err)) {
        plugin_set_err(errbuf, errbuf_len, std::string("ttf_extractor: ") + extract_err);
        return 13;
    }

    g_owner.font = std::move(extracted);
    g_owner.view = g_owner.font.as_plugin_font();
    *out_font = g_owner.view;
    return 0;
}

const snatch_plugin_info k_info = {
    "ttf_extractor",
    "Extracts bitmap glyphs from TTF input",
    "snatch project",
    "ttf",
    "extractor",
    SNATCH_PLUGIN_ABI_VERSION,
    SNATCH_PLUGIN_KIND_EXTRACTOR,
    nullptr,
    nullptr,
    &extract_ttf
};

} // namespace

extern "C" SNATCH_PLUGIN_API int snatch_plugin_get(const snatch_plugin_info** out) {
    if (!out) return 1;
    *out = &k_info;
    return 0;
}
