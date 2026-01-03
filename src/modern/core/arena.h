// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "common.h"
#include <memory>
#include <memory_resource>
#include <vector>
#include <cstdlib>
#include <new>

// Modern arena allocator using C++17 polymorphic memory resources
class Arena {
public:
    explicit Arena(size_t initial_size = 1024 * 1024)
        : resource_(initial_size) {}
    
    ~Arena() {
        for (void* ptr : external_allocations_) {
            std::free(ptr);
        }
    }
    
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&&) noexcept = default;
    Arena& operator=(Arena&&) noexcept = default;
    
    [[nodiscard]] void* allocate(size_t bytes, size_t alignment = alignof(std::max_align_t)) {
        void* ptr = resource_.allocate(bytes, alignment);
        total_allocated_ += bytes;
        return ptr;
    }
    
    template<typename T>
    [[nodiscard]] T* allocate() {
        return static_cast<T*>(allocate(sizeof(T), alignof(T)));
    }
    
    template<typename T>
    [[nodiscard]] T* allocate_array(size_t count) {
        return static_cast<T*>(allocate(sizeof(T) * count, alignof(T)));
    }
    
    template<typename T, typename... Args>
    [[nodiscard]] T* construct(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        return new(mem) T(std::forward<Args>(args)...);
    }
    
    [[nodiscard]] std::pmr::polymorphic_allocator<std::byte> allocator() {
        return std::pmr::polymorphic_allocator<std::byte>(&resource_);
    }
    
    [[nodiscard]] size_t total_allocated() const noexcept {
        return total_allocated_;
    }
    
    void reset() {
        resource_.release();
        for (void* ptr : external_allocations_) {
            std::free(ptr);
        }
        external_allocations_.clear();
        total_allocated_ = 0;
    }
    
    void track_external(void* ptr) {
        if (ptr) external_allocations_.push_back(ptr);
    }
    
private:
    std::pmr::monotonic_buffer_resource resource_;
    std::vector<void*> external_allocations_;
    size_t total_allocated_ = 0;
};

// RAII arena guard (resets on destruction)
class ArenaGuard {
public:
    explicit ArenaGuard(Arena& arena) : arena_(arena) {}
    ~ArenaGuard() { arena_.reset(); }
    
    ArenaGuard(const ArenaGuard&) = delete;
    ArenaGuard& operator=(const ArenaGuard&) = delete;
    
private:
    Arena& arena_;
};

// Smart pointer that uses arena allocation (requires std::functional)
#include <functional>

template<typename T>
using arena_unique_ptr = std::unique_ptr<T, std::function<void(T*)>>;

template<typename T, typename... Args>
arena_unique_ptr<T> make_arena_unique(Arena& arena, Args&&... args) {
    T* ptr = arena.construct<T>(std::forward<Args>(args)...);
    return arena_unique_ptr<T>(ptr, [](T* p) { p->~T(); });
}
