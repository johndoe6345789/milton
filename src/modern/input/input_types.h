// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "../vector.h"
#include <chrono>
#include <variant>
#include <optional>

namespace milton::input {

// Key codes
enum class KeyCode {
    Unknown = 0,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Space, Enter, Escape, Tab, Backspace, Delete,
    Left, Right, Up, Down,
    LeftBracket, RightBracket, Minus, Equal,
    LeftShift, RightShift, LeftCtrl, RightCtrl,
    LeftAlt, RightAlt, LeftMeta, RightMeta,
};

// Mouse buttons
enum class MouseButton {
    None = 0,
    Left,
    Middle,
    Right,
    X1,
    X2,
};

// Modifier keys
struct ModifierKeys {
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    bool meta = false; // Windows/Command key
};

const char* key_code_to_string(KeyCode key);
const char* mouse_button_to_string(MouseButton button);

} // namespace milton::input
