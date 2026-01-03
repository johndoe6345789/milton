// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "input_events.h"
#include <variant>
#include <vector>

namespace milton::input {

// Stylus/tablet input
struct StylusEvent {
    v2i position;
    f32 pressure = 1.0f;
    f32 tilt_x = 0.0f;
    f32 tilt_y = 0.0f;
    bool touching = false;
};

// Input event variant (type-safe union)
using InputEvent = std::variant<
    KeyEvent,
    MouseButtonEvent,
    MouseMoveEvent,
    MouseWheelEvent,
    StylusEvent
>;

// Input state tracker
class InputState {
public:
    InputState() = default;
    
    void update(const InputEvent& event);
    void clear();
    
    // Query state
    bool is_key_down(KeyCode key) const;
    bool is_mouse_button_down(MouseButton button) const;
    v2i mouse_position() const { return mouse_pos_; }
    ModifierKeys modifiers() const { return modifiers_; }
    
private:
    std::vector<KeyCode> pressed_keys_;
    std::vector<MouseButton> pressed_buttons_;
    v2i mouse_pos_{0, 0};
    ModifierKeys modifiers_;
};

} // namespace milton::input
