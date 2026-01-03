// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "../common.h"
#include "../vector.h"
#include <cmath>
#include <algorithm>

namespace milton::color {

// RGBA color conversions
inline constexpr u32 to_u32(v4f c) {
    u32 r = static_cast<u32>(std::clamp(c.r, 0.0f, 1.0f) * 255.0f);
    u32 g = static_cast<u32>(std::clamp(c.g, 0.0f, 1.0f) * 255.0f);
    u32 b = static_cast<u32>(std::clamp(c.b, 0.0f, 1.0f) * 255.0f);
    u32 a = static_cast<u32>(std::clamp(c.a, 0.0f, 1.0f) * 255.0f);
    return (a << 24) | (b << 16) | (g << 8) | r;
}

inline constexpr v4f to_v4f(u32 color) {
    return v4f{
        static_cast<float>(color & 0xFF) / 255.0f,
        static_cast<float>((color >> 8) & 0xFF) / 255.0f,
        static_cast<float>((color >> 16) & 0xFF) / 255.0f,
        static_cast<float>((color >> 24) & 0xFF) / 255.0f
    };
}

inline constexpr v4f to_rgba(v3f rgb, float alpha = 1.0f) {
    return v4f{rgb.r, rgb.g, rgb.b, alpha};
}

// Blending (alpha compositing)
inline v4f blend(v4f dst, v4f src) {
    float src_alpha = src.a;
    float dst_alpha = dst.a * (1.0f - src_alpha);
    float out_alpha = src_alpha + dst_alpha;
    
    if (out_alpha < 0.0001f) return {0, 0, 0, 0};
    
    return v4f{
        (src.r * src_alpha + dst.r * dst_alpha) / out_alpha,
        (src.g * src_alpha + dst.g * dst_alpha) / out_alpha,
        (src.b * src_alpha + dst.b * dst_alpha) / out_alpha,
        out_alpha
    };
}

// Premultiplied alpha
inline constexpr v4f to_premultiplied(v3f rgb, float alpha) {
    return v4f{rgb.r * alpha, rgb.g * alpha, rgb.b * alpha, alpha};
}

inline u32 un_premultiply(u32 color) {
    u32 a = (color >> 24) & 0xFF;
    if (a == 0) return 0;
    if (a == 255) return color;
    
    float inv_alpha = 255.0f / static_cast<float>(a);
    u32 r = static_cast<u32>(std::clamp((color & 0xFF) * inv_alpha, 0.0f, 255.0f));
    u32 g = static_cast<u32>(std::clamp(((color >> 8) & 0xFF) * inv_alpha, 0.0f, 255.0f));
    u32 b = static_cast<u32>(std::clamp(((color >> 16) & 0xFF) * inv_alpha, 0.0f, 255.0f));
    
    return (a << 24) | (b << 16) | (g << 8) | r;
}

// Clamping utilities
inline constexpr v3f clamp_01(v3f color) {
    return v3f{
        std::clamp(color.r, 0.0f, 1.0f),
        std::clamp(color.g, 0.0f, 1.0f),
        std::clamp(color.b, 0.0f, 1.0f)
    };
}

inline constexpr v4f clamp_01(v4f color) {
    return v4f{
        std::clamp(color.r, 0.0f, 1.0f),
        std::clamp(color.g, 0.0f, 1.0f),
        std::clamp(color.b, 0.0f, 1.0f),
        std::clamp(color.a, 0.0f, 1.0f)
    };
}

// Common colors
namespace colors {
    inline constexpr v4f white() { return {1.0f, 1.0f, 1.0f, 1.0f}; }
    inline constexpr v4f black() { return {0.0f, 0.0f, 0.0f, 1.0f}; }
    inline constexpr v4f red() { return {1.0f, 0.0f, 0.0f, 1.0f}; }
    inline constexpr v4f green() { return {0.0f, 1.0f, 0.0f, 1.0f}; }
    inline constexpr v4f blue() { return {0.0f, 0.0f, 1.0f, 1.0f}; }
    inline constexpr v4f transparent() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
}

} // namespace milton::color
