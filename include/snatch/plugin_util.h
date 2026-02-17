/// \file
/// \brief Shared helper utilities for plugin option parsing and errors.
///
/// This header declares types and contracts used by snatch core and plugin stages. It is part of the extractor-transformer-exporter architecture and is consumed by build-time and runtime plugin integration.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "snatch/plugin.h"

class plugin_kv_view {
public:
    plugin_kv_view(const snatch_kv* items, unsigned count) : items_(items), count_(count) {}

    [[nodiscard]] std::optional<std::string_view> get(std::string_view key) const {
        if (!items_) return std::nullopt;
        for (unsigned i = count_; i > 0; --i) {
            const snatch_kv& kv = items_[i - 1];
            if (!kv.key || !kv.value) continue;
            if (key == kv.key) return std::string_view{kv.value};
        }
        return std::nullopt;
    }

private:
    const snatch_kv* items_{nullptr};
    unsigned count_{0};
};

inline void plugin_set_err(char* errbuf, unsigned errbuf_len, std::string_view message) {
    if (!errbuf || errbuf_len == 0) return;
    const std::size_t n = std::min<std::size_t>(message.size(), errbuf_len - 1);
    std::copy_n(message.data(), n, errbuf);
    errbuf[n] = '\0';
}

inline std::optional<int> plugin_parse_int(std::string_view s) {
    if (s.empty()) return std::nullopt;
    int out = 0;
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    if (ec != std::errc{} || ptr != s.data() + s.size()) return std::nullopt;
    return out;
}

inline bool plugin_parse_bool(std::optional<std::string_view> raw, bool default_value) {
    if (!raw || raw->empty()) return default_value;
    if (*raw == "1" || *raw == "true" || *raw == "yes") return true;
    if (*raw == "0" || *raw == "false" || *raw == "no") return false;
    return default_value;
}

inline std::optional<std::array<std::uint8_t, 3>> plugin_parse_hex_rgb(std::string_view s) {
    if (s.empty()) return std::nullopt;
    if (s.front() == '#') s.remove_prefix(1);
    if (s.size() != 6) return std::nullopt;

    unsigned value = 0;
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value, 16);
    if (ec != std::errc{} || ptr != s.data() + s.size() || value > 0xFFFFFFu) return std::nullopt;

    return std::array<std::uint8_t, 3>{
        static_cast<std::uint8_t>((value >> 16) & 0xFFu),
        static_cast<std::uint8_t>((value >> 8) & 0xFFu),
        static_cast<std::uint8_t>(value & 0xFFu),
    };
}
