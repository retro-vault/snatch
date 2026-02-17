/// \file
/// \brief Shared passthrough image payload definitions for plugins.
///
/// This header declares types and contracts used by snatch core and plugin stages. It is part of the extractor-transformer-exporter architecture and is consumed by build-time and runtime plugin integration.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <cstdint>

constexpr std::uint32_t SNATCH_IMAGE_PASSTHROUGH_MAGIC = 0x49505448u; // "IPTH"
constexpr std::uint16_t SNATCH_IMAGE_PASSTHROUGH_VERSION = 1u;

struct snatch_image_passthrough_data {
    std::uint32_t magic{SNATCH_IMAGE_PASSTHROUGH_MAGIC};
    std::uint16_t version{SNATCH_IMAGE_PASSTHROUGH_VERSION};
    std::uint16_t width{0};
    std::uint16_t height{0};
    std::uint16_t stride{0}; // bytes per row, 8-bit grayscale
    const std::uint8_t* pixels{nullptr};
};
