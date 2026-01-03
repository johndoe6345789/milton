// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "layer.h"
#include "../core/memory_modern.h"
#include <vector>
#include <memory>
#include <optional>

namespace milton {

// Canvas view transformation
struct CanvasView {
    v2i screen_size{1280, 800};
    i64 scale = 1024; // Zoom level
    v2i zoom_center{0, 0};
    v2l pan_center{0, 0};
    v3f background_color{1.0f, 1.0f, 1.0f};
    i32 working_layer_id = 0;
    f32 angle = 0.0f; // Rotation
    
    v2l screen_to_canvas(v2i screen_point) const;
    v2i canvas_to_screen(v2l canvas_point) const;
};

// Canvas with layer hierarchy
class Canvas {
public:
    explicit Canvas(Arena& arena);
    ~Canvas() = default;
    
    // Non-copyable, movable
    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;
    Canvas(Canvas&&) = default;
    Canvas& operator=(Canvas&&) = default;
    
    // Layer management
    Layer* add_layer(const std::string& name = "Layer");
    void remove_layer(i32 layer_id);
    Layer* get_layer(i32 layer_id);
    const Layer* get_layer(i32 layer_id) const;
    Layer* working_layer();
    const Layer* working_layer() const;
    void set_working_layer(i32 layer_id);
    
    const std::vector<std::unique_ptr<Layer>>& layers() const { return layers_; }
    
    // View
    CanvasView& view() { return view_; }
    const CanvasView& view() const { return view_; }
    
    // Stroke management
    void add_stroke_to_layer(i32 layer_id, Stroke stroke);
    
private:
    Arena& arena_;
    std::vector<std::unique_ptr<Layer>> layers_;
    CanvasView view_;
    i32 next_layer_id_ = 1;
};

} // namespace milton
