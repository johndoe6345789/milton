// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "brush.h"
#include "../common.h"
#include "../vector.h"
#include "../utils.h"
#include <vector>
#include <memory>
#include <optional>
#include <algorithm>
#include <span>

namespace milton {

// Modern stroke with dynamic points
class Stroke {
public:
    Stroke() = default;
    explicit Stroke(i32 id, i32 layer_id, const BrushState& brush)
        : id_(id), layer_id_(layer_id), brush_(brush) {}
    
    // Accessors
    i32 id() const { return id_; }
    i32 layer_id() const { return layer_id_; }
    const BrushState& brush() const { return brush_; }
    BrushState& brush() { return brush_; }
    
    const std::vector<v2l>& points() const { return points_; }
    const std::vector<f32>& pressures() const { return pressures_; }
    
    size_t point_count() const { return points_.size(); }
    bool empty() const { return points_.empty(); }
    
    Rect bounds() const { return bounds_; }
    StrokeFlags flags() const { return flags_; }
    void set_flags(StrokeFlags f) { flags_ = f; }
    
    // Point management
    void add_point(v2l point, f32 pressure = 1.0f);
    void reserve(size_t count);
    void clear();
    
    // Bounding box
    void update_bounds();
    
private:
    i32 id_ = 0;
    i32 layer_id_ = 0;
    BrushState brush_;
    std::vector<v2l> points_;
    std::vector<f32> pressures_;
    Rect bounds_{0, 0, 0, 0};
    StrokeFlags flags_ = StrokeFlags::None;
};

// Inline implementations
inline void Stroke::add_point(v2l point, f32 pressure) {
    points_.push_back(point);
    pressures_.push_back(pressure);
    
    if (points_.size() == 1) {
        bounds_ = Rect{static_cast<i32>(point.x), static_cast<i32>(point.y), 0, 0};
    } else {
        i32 px = static_cast<i32>(point.x);
        i32 py = static_cast<i32>(point.y);
        bounds_.left = std::min(bounds_.left, px);
        bounds_.top = std::min(bounds_.top, py);
        bounds_.right = std::max(bounds_.right, px);
        bounds_.bottom = std::max(bounds_.bottom, py);
    }
}

inline void Stroke::reserve(size_t count) {
    points_.reserve(count);
    pressures_.reserve(count);
}

inline void Stroke::clear() {
    points_.clear();
    pressures_.clear();
    bounds_ = {0, 0, 0, 0};
}

inline void Stroke::update_bounds() {
    if (points_.empty()) {
        bounds_ = {0, 0, 0, 0};
        return;
    }
    
    auto first = points_[0];
    i32 min_x = static_cast<i32>(first.x);
    i32 min_y = static_cast<i32>(first.y);
    i32 max_x = min_x;
    i32 max_y = min_y;
    
    for (const auto& p : points_) {
        i32 px = static_cast<i32>(p.x);
        i32 py = static_cast<i32>(p.y);
        min_x = std::min(min_x, px);
        min_y = std::min(min_y, py);
        max_x = std::max(max_x, px);
        max_y = std::max(max_y, py);
    }
    
    bounds_ = Rect{min_x, min_y, max_x - min_x, max_y - min_y};
}

} // namespace milton
