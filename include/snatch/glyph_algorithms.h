/// \file
/// \brief Bitmap glyph analysis and routing algorithm interfaces.
///
/// This header declares types and contracts used by snatch core and plugin stages. It is part of the extractor-transformer-exporter architecture and is consumed by build-time and runtime plugin integration.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <cstdint>
#include <vector>

#include "snatch/plugin.h"

struct glyph_pixel {
    int x{0};
    int y{0};
    std::uint8_t color{1};
    bool move{false};
};

struct glyph_bounds {
    int left{-1};
    int right{-1};
    int top{-1};
    int bottom{-1};
    bool empty{true};
};

class glyph_bitmap_analyzer {
public:
    static glyph_bounds bounds(const snatch_glyph_bitmap& glyph);
    static int rightmost_set_bit(const snatch_glyph_bitmap& glyph);
    static int leftmost_set_bit(const snatch_glyph_bitmap& glyph);
    static std::vector<glyph_pixel> foreground_pixels(const snatch_glyph_bitmap& glyph, std::uint8_t color = 1);
};

class glyph_route_cost_model {
public:
    explicit glyph_route_cost_model(
        int color_threshold = 0,
        int pen_lift_cost = 3,
        int color_change_cost = 2,
        int max_free_line_run = 4
    );

    bool same_color(const glyph_pixel& a, const glyph_pixel& b) const;
    int transition_cost(const glyph_pixel& a, const glyph_pixel& b, int& dx, int& dy) const;
    int total_cost(const std::vector<glyph_pixel>& route) const;

private:
    int color_threshold_{0};
    int pen_lift_cost_{3};
    int color_change_cost_{2};
    int max_free_line_run_{4};
};

class glyph_route_optimizer {
public:
    explicit glyph_route_optimizer(glyph_route_cost_model model = glyph_route_cost_model{});

    std::vector<glyph_pixel> tsp_2opt(const std::vector<glyph_pixel>& route) const;

private:
    static std::vector<glyph_pixel> two_opt_swap(const std::vector<glyph_pixel>& route, int i, int k);

    glyph_route_cost_model cost_model_;
};
