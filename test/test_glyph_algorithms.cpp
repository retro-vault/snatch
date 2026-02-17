/// \file
/// \brief Unit tests for glyph analysis and routing algorithms.
///
/// This test source validates behavior of core parsing, extraction, transformation, and export flows. It helps ensure regressions are caught early for the plugin-driven pipeline.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include <gtest/gtest.h>

#include "snatch/glyph_algorithms.h"

namespace {

/// \brief set_bit.
void set_bit(std::vector<unsigned char>& data, int stride, int x, int y) {
    const size_t row = static_cast<size_t>(y * stride);
    const int byte_index = x / 8;
    const int bit_index = 7 - (x % 8);
    data[row + static_cast<size_t>(byte_index)] |= static_cast<unsigned char>(1u << bit_index);
}

} // namespace

TEST(glyph_algorithms, analyzer_detects_bounds_and_pixels) {
    const int width = 5;
    const int height = 3;
    const int stride = 1;
    std::vector<unsigned char> bits(static_cast<size_t>(stride * height), 0);
    set_bit(bits, stride, 1, 0);
    set_bit(bits, stride, 4, 1);
    set_bit(bits, stride, 2, 2);

    snatch_glyph_bitmap glyph{};
    glyph.width = width;
    glyph.height = height;
    glyph.stride_bytes = stride;
    glyph.data = bits.data();

    EXPECT_EQ(glyph_bitmap_analyzer::leftmost_set_bit(glyph), 1);
    EXPECT_EQ(glyph_bitmap_analyzer::rightmost_set_bit(glyph), 4);

    const auto pixels = glyph_bitmap_analyzer::foreground_pixels(glyph, 7);
    ASSERT_EQ(pixels.size(), 3u);
    EXPECT_EQ(pixels[0].color, 7);
}

TEST(glyph_algorithms, tsp2opt_reduces_route_cost) {
    std::vector<glyph_pixel> route = {
        {0, 0, 1, false},
        {5, 0, 1, false},
        {0, 1, 1, false},
        {5, 1, 1, false}
    };

    glyph_route_cost_model cost_model;
    glyph_route_optimizer optimizer(cost_model);

    const int before = cost_model.total_cost(route);
    const auto optimized = optimizer.tsp_2opt(route);
    const int after = cost_model.total_cost(optimized);

    EXPECT_LE(after, before);
    EXPECT_LT(after, before);
}
