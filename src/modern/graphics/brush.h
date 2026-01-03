// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "../common.h"
#include "../vector.h"
#include "../utils.h"
#include <vector>
#include <span>

namespace milton {

// Modern brush state with value semantics
struct BrushState {
    f32 radius = 10.0f;
    v3f color{0.0f, 0.0f, 0.0f};
    f32 alpha = 1.0f;
    f32 hardness = 0.8f;
    
    f32 pressure_opacity_min = 0.0f;
    f32 pressure_opacity_max = 1.0f;
    
    constexpr BrushState() = default;
};

// Stroke flags
enum class StrokeFlags : u32 {
    None = 0,
    PressureToOpacity = 1 << 0,
    DistanceToOpacity = 1 << 1,
    Eraser = 1 << 2,
    RelativeToCanvas = 1 << 3,
};

inline StrokeFlags operator|(StrokeFlags a, StrokeFlags b) {
    return static_cast<StrokeFlags>(static_cast<u32>(a) | static_cast<u32>(b));
}

inline bool has_flag(StrokeFlags flags, StrokeFlags flag) {
    return (static_cast<u32>(flags) & static_cast<u32>(flag)) != 0;
}

} // namespace milton
