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
