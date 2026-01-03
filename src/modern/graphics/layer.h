// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "stroke_list.h"
#include <memory>
#include <string>
#include <vector>

namespace milton {

// Layer with stroke collection
class Layer {
public:
    explicit Layer(i32 id, const std::string& name = "Layer")
        : id_(id), name_(name) {}
    
    i32 id() const { return id_; }
    
    const std::string& name() const { return name_; }
    void set_name(const std::string& n) { name_ = n; }
    
    bool visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; }
    
    float opacity() const { return opacity_; }
    void set_opacity(float o) { opacity_ = std::clamp(o, 0.0f, 1.0f); }
    
    StrokeList& strokes() { return strokes_; }
    const StrokeList& strokes() const { return strokes_; }
    
    void add_stroke(Stroke stroke) { strokes_.push(std::move(stroke)); }
    
private:
    i32 id_;
    std::string name_;
    bool visible_ = true;
    float opacity_ = 1.0f;
    StrokeList strokes_;
};

} // namespace milton
