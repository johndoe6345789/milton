// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "../common.h"
#include "../vector.h"
#include <cmath>

namespace milton::color {

// HSV color space conversions
inline v3f hsv_to_rgb(v3f hsv) {
    float h = hsv.h;
    float s = hsv.s;
    float v = hsv.v;
    
    float c = v * s;
    float h_prime = h / 60.0f;
    float x = c * (1.0f - std::abs(std::fmod(h_prime, 2.0f) - 1.0f));
    
    v3f rgb1;
    if (h_prime < 1.0f)      rgb1 = {c, x, 0};
    else if (h_prime < 2.0f) rgb1 = {x, c, 0};
    else if (h_prime < 3.0f) rgb1 = {0, c, x};
    else if (h_prime < 4.0f) rgb1 = {0, x, c};
    else if (h_prime < 5.0f) rgb1 = {x, 0, c};
    else                     rgb1 = {c, 0, x};
    
    float m = v - c;
    return v3f{rgb1.r + m, rgb1.g + m, rgb1.b + m};
}

inline v3f rgb_to_hsv(v3f rgb) {
    float max_val = std::max({rgb.r, rgb.g, rgb.b});
    float min_val = std::min({rgb.r, rgb.g, rgb.b});
    float delta = max_val - min_val;
    
    v3f hsv;
    hsv.v = max_val;
    hsv.s = (max_val > 0.0f) ? (delta / max_val) : 0.0f;
    
    if (delta > 0.0f) {
        if (max_val == rgb.r) {
            hsv.h = 60.0f * std::fmod((rgb.g - rgb.b) / delta, 6.0f);
        } else if (max_val == rgb.g) {
            hsv.h = 60.0f * ((rgb.b - rgb.r) / delta + 2.0f);
        } else {
            hsv.h = 60.0f * ((rgb.r - rgb.g) / delta + 4.0f);
        }
        if (hsv.h < 0.0f) hsv.h += 360.0f;
    } else {
        hsv.h = 0.0f;
    }
    
    return hsv;
}

} // namespace milton::color
