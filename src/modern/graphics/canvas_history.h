// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "canvas.h"
#include <vector>
#include <optional>
#include <variant>

namespace milton {

// History system for undo/redo
enum class HistoryActionType {
    AddStroke,
    RemoveStroke,
    ModifyLayer,
    AddLayer,
    RemoveLayer,
};

struct HistoryElement {
    HistoryActionType type;
    i32 layer_id;
    std::optional<Stroke> stroke;
    
    // Future: add layer modification data
};

class CanvasHistory {
public:
    explicit CanvasHistory(size_t max_size = 1000)
        : max_size_(max_size) {}
    
    void push(HistoryElement element);
    std::optional<HistoryElement> undo();
    std::optional<HistoryElement> redo();
    
    bool can_undo() const { return current_pos_ > 0; }
    bool can_redo() const { return current_pos_ < history_.size(); }
    
    void clear();
    
private:
    std::vector<HistoryElement> history_;
    size_t current_pos_ = 0;
    size_t max_size_;
};

inline void CanvasHistory::push(HistoryElement element) {
    // Truncate redo history
    if (current_pos_ < history_.size()) {
        history_.erase(history_.begin() + current_pos_, history_.end());
    }
    
    history_.push_back(std::move(element));
    
    // Limit history size
    if (history_.size() > max_size_) {
        history_.erase(history_.begin());
    } else {
        current_pos_++;
    }
}

inline std::optional<HistoryElement> CanvasHistory::undo() {
    if (!can_undo()) return std::nullopt;
    return history_[--current_pos_];
}

inline std::optional<HistoryElement> CanvasHistory::redo() {
    if (!can_redo()) return std::nullopt;
    return history_[current_pos_++];
}

inline void CanvasHistory::clear() {
    history_.clear();
    current_pos_ = 0;
}

} // namespace milton
