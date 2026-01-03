// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "arena.h"
#include <memory_resource>

// STL allocator adapter for Arena
template<typename T>
class ArenaAllocator {
public:
    using value_type = T;
    
    explicit ArenaAllocator(Arena& arena) noexcept
        : arena_(&arena) {}
    
    template<typename U>
    ArenaAllocator(const ArenaAllocator<U>& other) noexcept
        : arena_(other.arena_) {}
    
    [[nodiscard]] T* allocate(size_t n) {
        return arena_->allocate_array<T>(n);
    }
    
    void deallocate(T*, size_t) noexcept {
        // Arena doesn't free individual allocations
    }
    
    template<typename U>
    struct rebind {
        using other = ArenaAllocator<U>;
    };
    
    template<typename U>
    bool operator==(const ArenaAllocator<U>& other) const noexcept {
        return arena_ == other.arena_;
    }
    
    template<typename U>
    bool operator!=(const ArenaAllocator<U>& other) const noexcept {
        return !(*this == other);
    }
    
    Arena* arena_;
    
    template<typename U>
    friend class ArenaAllocator;
};

// Convenience aliases
template<typename T>
using arena_vector = std::vector<T, ArenaAllocator<T>>;

template<typename K, typename V>
using arena_map = std::map<K, V, std::less<K>, 
    ArenaAllocator<std::pair<const K, V>>>;

template<typename T>
using arena_string = std::basic_string<T, std::char_traits<T>, 
    ArenaAllocator<T>>;
