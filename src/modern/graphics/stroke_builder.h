// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "stroke.h"
#include <vector>
#include <optional>

namespace milton {

// Helper for building strokes incrementally
class StrokeBuilder {
public:
    explicit StrokeBuilder(i32 id, i32 layer_id, const BrushState& brush)
        : stroke_(id, layer_id, brush) {}
    
    void add_point(v2l point, f32 pressure = 1.0f) {
        stroke_.add_point(point, pressure);
    }
    
    Stroke finish() {
        stroke_.update_bounds();
        return std::move(stroke_);
    }
    
    const Stroke& current() const { return stroke_; }
    
private:
    Stroke stroke_;
};

// Stroke smoothing/filtering
inline std::vector<v2l> smooth_points(std::span<const v2l> points, int window_size = 3) {
    if (points.size() < static_cast<size_t>(window_size)) {
        return std::vector<v2l>(points.begin(), points.end());
    }
    
    std::vector<v2l> smoothed;
    smoothed.reserve(points.size());
    
    int half_window = window_size / 2;
    
    for (size_t i = 0; i < points.size(); ++i) {
        i64 sum_x = 0, sum_y = 0;
        int count = 0;
        
        for (int j = -half_window; j <= half_window; ++j) {
            int idx = static_cast<int>(i) + j;
            if (idx >= 0 && idx < static_cast<int>(points.size())) {
                sum_x += points[idx].x;
                sum_y += points[idx].y;
                count++;
            }
        }
        
        smoothed.push_back({sum_x / count, sum_y / count});
    }
    
    return smoothed;
}

} // namespace milton
