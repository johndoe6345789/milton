// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "input_events.h"
#include <unordered_map>
#include <functional>
#include <string>

namespace milton::input {

// Action binding system
template<typename ActionEnum>
class ActionBindings {
public:
    using ActionCallback = std::function<void()>;
    
    void bind_key(KeyCode key, ActionEnum action) {
        key_bindings_[key] = action;
    }
    
    void bind_mouse(MouseButton button, ActionEnum action) {
        mouse_bindings_[button] = action;
    }
    
    void set_action_callback(ActionEnum action, ActionCallback callback) {
        action_callbacks_[action] = std::move(callback);
    }
    
    void process(const KeyEvent& event) {
        if (event.type != KeyEventType::Down) return;
        auto it = key_bindings_.find(event.key);
        if (it != key_bindings_.end()) {
            execute_action(it->second);
        }
    }
    
    void process(const MouseButtonEvent& event) {
        if (event.type != MouseButtonEventType::Down) return;
        auto it = mouse_bindings_.find(event.button);
        if (it != mouse_bindings_.end()) {
            execute_action(it->second);
        }
    }
    
private:
    void execute_action(ActionEnum action) {
        auto it = action_callbacks_.find(action);
        if (it != action_callbacks_.end() && it->second) {
            it->second();
        }
    }
    
    std::unordered_map<KeyCode, ActionEnum> key_bindings_;
    std::unordered_map<MouseButton, ActionEnum> mouse_bindings_;
    std::unordered_map<ActionEnum, ActionCallback> action_callbacks_;
};

} // namespace milton::input
