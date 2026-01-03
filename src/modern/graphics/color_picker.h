// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "../vector.h"
#include "../utils.h"
#include "color.h"
#include <vector>
#include <optional>

namespace milton::gui {

// HSV color picker
class ColorPicker {
public:
    struct HSVColor {
        float hue;        // 0-360
        float saturation; // 0-1
        float value;      // 0-1
        
        v4f to_rgba() const;
        static HSVColor from_rgba(v4f rgba);
    };
    
    ColorPicker(v2i center, int radius);
    
    enum class PickResult {
        NoChange,
        ColorChanged,
    };
    
    PickResult update(v2i mouse_pos, bool mouse_down);
    
    HSVColor get_hsv() const { return hsv_; }
    v4f get_rgba() const { return hsv_.to_rgba(); }
    void set_color(v4f rgba);
    
    Rect get_bounds() const { return bounds_; }
    const std::vector<u32>& get_pixels() const { return pixels_; }
    void regenerate_pixels();
    
    void add_color_button(v4f rgba);
    std::optional<v4f> check_color_button_click(v2i mouse_pos);
    
private:
    v2i center_;
    int bounds_radius_;
    Rect bounds_;
    float wheel_radius_;
    float wheel_half_width_;
    
    std::vector<u32> pixels_;
    HSVColor hsv_;
    std::vector<v4f> color_buttons_;
};

} // namespace milton::gui
