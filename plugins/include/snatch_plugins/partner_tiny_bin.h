/// \file
/// \brief Shared Partner Tiny binary payload definitions for plugins.
///
/// This header declares types and contracts used by snatch core and plugin stages. It is part of the extractor-transformer-exporter architecture and is consumed by build-time and runtime plugin integration.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <cstdint>

constexpr std::uint32_t SNATCH_PARTNER_TINY_BIN_MAGIC = 0x50544E42u; // "PTNB"
constexpr std::uint16_t SNATCH_PARTNER_TINY_BIN_VERSION = 1u;

struct snatch_partner_tiny_bin_data {
    std::uint32_t magic{SNATCH_PARTNER_TINY_BIN_MAGIC};
    std::uint16_t version{SNATCH_PARTNER_TINY_BIN_VERSION};
    const std::uint8_t* bytes{nullptr};
    std::uint32_t size{0};
};

