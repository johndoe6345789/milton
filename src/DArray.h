// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// Modern C++20 dynamic array using std::vector
// Maintains backward-compatible API while eliminating manual memory management

#pragma once

#include "common.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <cassert>

// Wrapper around std::vector to maintain API compatibility
// Provides legacy member variables that mirror internal vector state
template <typename T>
struct DArray
{
    // Public members for backward compatibility - these mirror the vector state
    i64 count;
    i64 capacity;
    T* data;

private:
    std::vector<T> vec_;
    
    // Update public members to match vector state
    void sync() {
        count = static_cast<i64>(vec_.size());
        capacity = static_cast<i64>(vec_.capacity());
        // Important: data pointer must remain valid even for empty vectors
        // vec_.data() on empty vector behavior is implementation-defined
        data = vec_.size() > 0 ? vec_.data() : nullptr;
    }

public:
    DArray() : count(0), capacity(0), data(nullptr), vec_() {}
    
    explicit DArray(i64 cap) : count(0), capacity(0), data(nullptr), vec_() {
        if (cap > 0) {
            vec_.reserve(static_cast<size_t>(cap));
            sync();
        }
    }
    
    // Deep copy
    DArray(const DArray& other) : count(0), capacity(0), data(nullptr), vec_(other.vec_) {
        sync();
    }
    
    DArray& operator=(const DArray& other) {
        if (this != &other) {
            vec_ = other.vec_;
            sync();
        }
        return *this;
    }
    
    // Move operations
    DArray(DArray&& other) noexcept 
        : count(other.count), capacity(other.capacity), data(other.data), vec_(std::move(other.vec_)) {
        other.count = 0;
        other.capacity = 0;
        other.data = nullptr;
    }
    
    DArray& operator=(DArray&& other) noexcept {
        if (this != &other) {
            vec_ = std::move(other.vec_);
            count = other.count;
            capacity = other.capacity;
            data = other.data;
            other.count = 0;
            other.capacity = 0;
            other.data = nullptr;
        }
        return *this;
    }
    
    ~DArray() = default;

    // Array access operators
    [[nodiscard]] T& operator[](i64 i) noexcept {
        assert(i >= 0 && i < count);
        return vec_[static_cast<size_t>(i)];
    }
    
    [[nodiscard]] const T& operator[](i64 i) const noexcept {
        assert(i >= 0 && i < count);
        return vec_[static_cast<size_t>(i)];
    }
    
    // Modern accessors
    [[nodiscard]] size_t size() const noexcept { return vec_.size(); }
    [[nodiscard]] bool empty() const noexcept { return vec_.empty(); }
    
    void push_back(const T& elem) {
        vec_.push_back(elem);
        sync();
    }
    
    void push_back(T&& elem) {
        vec_.push_back(std::move(elem));
        sync();
    }
    
    template<typename... Args>
    T& emplace_back(Args&&... args) {
        T& ref = vec_.emplace_back(std::forward<Args>(args)...);
        sync();
        return ref;
    }
    
    [[nodiscard]] T& back() {
        return vec_.back();
    }
    
    [[nodiscard]] const T& back() const {
        return vec_.back();
    }
    
    void pop_back() { 
        assert(!vec_.empty());
        vec_.pop_back();
        sync();
    }
    
    void reserve(size_t cap) {
        vec_.reserve(cap);
        sync();
    }
    
    void clear() noexcept {
        vec_.clear();
        sync();
    }
    
    void resize(size_t s) {
        vec_.resize(s);
        sync();
    }
    
    // Iterators
    auto begin() noexcept { return vec_.begin(); }
    auto end() noexcept { return vec_.end(); }
    auto begin() const noexcept { return vec_.begin(); }
    auto end() const noexcept { return vec_.end(); }
    auto cbegin() const noexcept { return vec_.cbegin(); }
    auto cend() const noexcept { return vec_.cend(); }
    
    // Access internal vector for advanced usage
    [[nodiscard]] std::vector<T>& vector() noexcept { return vec_; }
    [[nodiscard]] const std::vector<T>& vector() const noexcept { return vec_; }
};

// Legacy function wrappers for gradual migration
template <typename T>
[[nodiscard]] DArray<T> dynamic_array(i64 capacity)
{
    return DArray<T>(capacity);
}

template <typename T>
void grow(DArray<T>* arr)
{
    if (!arr) return;
    i64 new_capacity = arr->capacity == 0 ? 32 : arr->capacity * 2;
    arr->reserve(static_cast<size_t>(new_capacity));
}

template <typename T>
void reserve(DArray<T>* arr, i64 size)
{
    if (arr && size > 0) {
        arr->reserve(static_cast<size_t>(size));
    }
}

template <typename T>
[[nodiscard]] T* push(DArray<T>* arr, const T& elem)
{
    if (!arr) return nullptr;
    arr->push_back(elem);
    return &arr->back();
}

template <typename T>
[[nodiscard]] T* get(DArray<T>* arr, i64 i)
{
    if (!arr || i < 0 || i >= arr->count) return nullptr;
    return &(*arr)[i];
}

template <typename T>
[[nodiscard]] T* peek(DArray<T>* arr)
{
    if (!arr || arr->empty()) return nullptr;
    return &arr->back();
}

template <typename T>
[[nodiscard]] T pop(DArray<T>* arr)
{
    assert(arr && !arr->empty());
    T elem = arr->back();
    arr->pop_back();
    return elem;
}

template <typename T>
[[nodiscard]] i64 count(const DArray<T>* arr)
{
    return arr ? arr->count : 0;
}

template <typename T>
void reset(DArray<T>* arr)
{
    if (arr) {
        arr->clear();
    }
}

template <typename T>
void release(DArray<T>* arr)
{
    // RAII handles cleanup automatically - this is now a no-op
    // Kept for API compatibility during migration
    if (arr) {
        arr->clear();
    }
}

// Range-based for loop support
template <typename T>
[[nodiscard]] auto begin(DArray<T>& arr) noexcept
{
    return arr.begin();
}

template <typename T>
[[nodiscard]] auto end(DArray<T>& arr) noexcept
{
    return arr.end();
}

template <typename T>
[[nodiscard]] auto begin(const DArray<T>& arr) noexcept
{
    return arr.begin();
}

template <typename T>
[[nodiscard]] auto end(const DArray<T>& arr) noexcept
{
    return arr.end();
}




