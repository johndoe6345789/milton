// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "input_types.h"
#include "../vector.h"

namespace milton::input {

// Key event
enum class KeyEventType { Down, Up };

struct KeyEvent {
    KeyEventType type;
    KeyCode key;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    bool repeat = false;
};

// Mouse events
enum class MouseButtonEventType { Down, Up };

struct MouseButtonEvent {
    MouseButtonEventType type;
    MouseButton button;
    v2i position;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

struct MouseMoveEvent {
    v2i position;
    v2i delta;
    bool left_down = false;
    bool right_down = false;
    bool middle_down = false;
};

struct MouseWheelEvent {
    v2i position;
    i32 delta; // Positive = scroll up, negative = scroll down
};

} // namespace milton::input
