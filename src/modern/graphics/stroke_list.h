// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "stroke.h"
#include <deque>
#include <optional>

namespace milton {

// Modern stroke list with stable pointers using std::deque
class StrokeList {
public:
    StrokeList() = default;
    ~StrokeList() = default;
    
    // Non-copyable, movable
    StrokeList(const StrokeList&) = delete;
    StrokeList& operator=(const StrokeList&) = delete;
    StrokeList(StrokeList&&) = default;
    StrokeList& operator=(StrokeList&&) = default;
    
    // Add/remove strokes
    void push(const Stroke& stroke) { strokes_.push_back(stroke); }
    void push(Stroke&& stroke) { strokes_.push_back(std::move(stroke)); }
    
    std::optional<Stroke> pop() {
        if (strokes_.empty()) return std::nullopt;
        Stroke s = std::move(strokes_.back());
        strokes_.pop_back();
        return s;
    }
    
    void clear() { strokes_.clear(); }
    
    // Access
    size_t size() const noexcept { return strokes_.size(); }
    bool empty() const noexcept { return strokes_.empty(); }
    
    Stroke& operator[](size_t i) { return strokes_[i]; }
    const Stroke& operator[](size_t i) const { return strokes_[i]; }
    
    Stroke& at(size_t i) { return strokes_.at(i); }
    const Stroke& at(size_t i) const { return strokes_.at(i); }
    
    Stroke& front() { return strokes_.front(); }
    const Stroke& front() const { return strokes_.front(); }
    
    Stroke& back() { return strokes_.back(); }
    const Stroke& back() const { return strokes_.back(); }
    
    // Iterators (deque provides stable pointers)
    auto begin() noexcept { return strokes_.begin(); }
    auto end() noexcept { return strokes_.end(); }
    auto begin() const noexcept { return strokes_.begin(); }
    auto end() const noexcept { return strokes_.end(); }
    auto cbegin() const noexcept { return strokes_.cbegin(); }
    auto cend() const noexcept { return strokes_.cend(); }
    
    // Direct container access
    std::deque<Stroke>& container() noexcept { return strokes_; }
    const std::deque<Stroke>& container() const noexcept { return strokes_; }
    
private:
    std::deque<Stroke> strokes_;
};

} // namespace milton
