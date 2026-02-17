/// \file
/// \brief Implementation of glyph analysis and route optimization utilities.
///
/// This source file implements one part of the snatch pipeline architecture. It contributes to extracting, transforming, exporting, or orchestrating bitmap data in a plugin-driven workflow.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include "snatch/glyph_algorithms.h"

#include <algorithm>
#include <cmath>

namespace {

/// \brief bit_is_set.
inline bool bit_is_set(const unsigned char* row, int x) {
    const int byte_index = x / 8;
    const int bit_index = 7 - (x % 8);
    return (row[byte_index] & (1u << bit_index)) != 0;
}

} // namespace

/// \brief glyph_bitmap_analyzer::rightmost_set_bit.
int glyph_bitmap_analyzer::rightmost_set_bit(const snatch_glyph_bitmap& glyph) {
    return bounds(glyph).right;
}

/// \brief glyph_bitmap_analyzer::leftmost_set_bit.
int glyph_bitmap_analyzer::leftmost_set_bit(const snatch_glyph_bitmap& glyph) {
    return bounds(glyph).left;
}

/// \brief glyph_bitmap_analyzer::bounds.
glyph_bounds glyph_bitmap_analyzer::bounds(const snatch_glyph_bitmap& glyph) {
    glyph_bounds out;
    if (!glyph.data || glyph.width <= 0 || glyph.height <= 0 || glyph.stride_bytes <= 0) return out;
    out.left = glyph.width;
    out.top = glyph.height;
    out.right = -1;
    out.bottom = -1;

    for (int y = 0; y < glyph.height; ++y) {
        const unsigned char* row = glyph.data + static_cast<size_t>(y * glyph.stride_bytes);
        for (int x = 0; x < glyph.width; ++x) {
            if (bit_is_set(row, x)) {
                out.left = std::min(out.left, x);
                out.right = std::max(out.right, x);
                out.top = std::min(out.top, y);
                out.bottom = std::max(out.bottom, y);
            }
        }
    }
    out.empty = (out.right < 0);
    if (out.empty) {
        out.left = -1;
        out.top = -1;
    }
    return out;
}

/// \brief glyph_bitmap_analyzer::foreground_pixels.
std::vector<glyph_pixel> glyph_bitmap_analyzer::foreground_pixels(const snatch_glyph_bitmap& glyph, std::uint8_t color) {
    std::vector<glyph_pixel> out;
    if (!glyph.data || glyph.width <= 0 || glyph.height <= 0 || glyph.stride_bytes <= 0) return out;
    out.reserve(static_cast<size_t>(glyph.width * glyph.height));
    for (int y = 0; y < glyph.height; ++y) {
        const unsigned char* row = glyph.data + static_cast<size_t>(y * glyph.stride_bytes);
        for (int x = 0; x < glyph.width; ++x) {
            if (bit_is_set(row, x)) out.push_back(glyph_pixel{x, y, color, false});
        }
    }
    return out;
}

glyph_route_cost_model::glyph_route_cost_model(
    int color_threshold,
    int pen_lift_cost,
    int color_change_cost,
    int max_free_line_run
) :
    color_threshold_(std::max(0, color_threshold)),
    pen_lift_cost_(std::max(0, pen_lift_cost)),
    color_change_cost_(std::max(0, color_change_cost)),
    max_free_line_run_(std::max(1, max_free_line_run)) {}

/// \brief glyph_route_cost_model::same_color.
bool glyph_route_cost_model::same_color(const glyph_pixel& a, const glyph_pixel& b) const {
    return std::abs(static_cast<int>(a.color) - static_cast<int>(b.color)) <= color_threshold_;
}

/// \brief glyph_route_cost_model::transition_cost.
int glyph_route_cost_model::transition_cost(const glyph_pixel& a, const glyph_pixel& b, int& dx, int& dy) const {
    dx = a.x - b.x;
    dy = a.y - b.y;
    int dist = std::max(std::abs(dx), std::abs(dy));
    if (dist > 1) {
        dist += pen_lift_cost_;
    } else if (!same_color(a, b)) {
        dist += color_change_cost_;
    }
    return dist;
}

/// \brief glyph_route_cost_model::total_cost.
int glyph_route_cost_model::total_cost(const std::vector<glyph_pixel>& route) const {
    if (route.size() < 2) return 0;
    int sum = 0;
    int dx = 0;
    int dy = 0;
    int prev_dx = 0;
    int prev_dy = 0;
    int line_len = 0;
    for (size_t i = 1; i < route.size(); ++i) {
        int step_cost = transition_cost(route[i - 1], route[i], dx, dy);
        if (step_cost == 1 && dx == prev_dx && dy == prev_dy && line_len < max_free_line_run_) {
            ++line_len;
            step_cost = 0;
        } else {
            line_len = 0;
        }
        sum += step_cost;
        prev_dx = dx;
        prev_dy = dy;
    }
    return sum;
}

glyph_route_optimizer::glyph_route_optimizer(glyph_route_cost_model model) : cost_model_(std::move(model)) {}

/// \brief glyph_route_optimizer::two_opt_swap.
std::vector<glyph_pixel> glyph_route_optimizer::two_opt_swap(const std::vector<glyph_pixel>& route, int i, int k) {
    std::vector<glyph_pixel> result;
    result.reserve(route.size());

    for (int n = 0; n <= i - 1; ++n) result.push_back(route[static_cast<size_t>(n)]);
    for (int n = k; n >= i; --n) result.push_back(route[static_cast<size_t>(n)]);
    for (size_t n = static_cast<size_t>(k + 1); n < route.size(); ++n) result.push_back(route[n]);
    return result;
}

/// \brief glyph_route_optimizer::tsp_2opt.
std::vector<glyph_pixel> glyph_route_optimizer::tsp_2opt(const std::vector<glyph_pixel>& route) const {
    if (route.size() < 3) return route;

    std::vector<glyph_pixel> best = route;
    int best_cost = cost_model_.total_cost(best);
    const int swappable = static_cast<int>(best.size()) - 1;

    bool improved = true;
    while (improved) {
        improved = false;
        for (int i = 0; i < swappable - 1; ++i) {
            for (int k = i + 1; k < swappable; ++k) {
                std::vector<glyph_pixel> candidate = two_opt_swap(best, i, k);
                const int candidate_cost = cost_model_.total_cost(candidate);
                if (candidate_cost < best_cost) {
                    best = std::move(candidate);
                    best_cost = candidate_cost;
                    improved = true;
                    goto restart_scan;
                }
            }
        }
restart_scan:
        continue;
    }
    return best;
}
