// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "color_picker.h"
#include "gui_widgets.h"
#include <string>
#include <unordered_map>
#include <functional>

namespace milton::gui {

// Main GUI state
class MiltonGui {
public:
    enum class Flags {
        None = 0,
        ShowingPreview = 1 << 0,
    };
    
    MiltonGui();
    
    ColorPicker& get_picker() { return picker_; }
    const ColorPicker& get_picker() const { return picker_; }
    
    Exporter& get_exporter() { return exporter_; }
    const Exporter& get_exporter() const { return exporter_; }
    
    void add_button(std::string name, Rect rect);
    std::optional<std::string> check_button_click(v2i mouse_pos, bool mouse_down);
    
    void set_flag(Flags flag, bool enabled);
    bool has_flag(Flags flag) const;
    
private:
    ColorPicker picker_;
    Exporter exporter_;
    std::unordered_map<std::string, Button> buttons_;
    int flags_ = 0;
};

// ImGui integration helpers
namespace imgui {
    void begin_frame();
    void end_frame();
    void show_color_panel(ColorPicker& picker);
    void show_brush_panel(int& brush_size, float& brush_alpha);
} // namespace imgui

} // namespace milton::gui
