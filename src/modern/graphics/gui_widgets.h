// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "../vector.h"
#include "../utils.h"

namespace milton::gui {

// Export region selector
class Exporter {
public:
    enum class State {
        Empty,
        GrowingRect,
        Selected
    };
    
    Exporter() = default;
    
    void start_selection(v2i start_point);
    void update_selection(v2i current_point);
    void finalize_selection();
    void reset();
    
    State get_state() const { return state_; }
    Rect get_rect() const;
    int get_scale() const { return scale_; }
    void set_scale(int scale) { scale_ = scale; }
    
private:
    State state_ = State::Empty;
    v2i pivot_;
    v2i needle_;
    int scale_ = 1;
};

// GUI button
struct Button {
    Rect rect;
    bool is_pressed = false;
    bool is_hovered = false;
    
    bool check_click(v2i mouse_pos, bool mouse_down);
};

} // namespace milton::gui
