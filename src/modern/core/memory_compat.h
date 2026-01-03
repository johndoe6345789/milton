// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "arena.h"

// Compatibility wrappers for legacy code that uses old arena API
// These bridge between old-style mlt_* functions and new Arena class

// Legacy allocator signatures
inline void* mlt_calloc(size_t count, size_t size, const char* tag = "General") {
    (void)tag; // Modern allocator doesn't track tags separately
    return std::calloc(count, size);
}

inline void mlt_free(void* ptr, const char* tag = "General") {
    (void)tag;
    std::free(ptr);
}

inline void* arena_alloc_array(Arena* arena, size_t count, size_t elem_size) {
    if (!arena) return nullptr;
    return arena->allocate(count * elem_size, alignof(std::max_align_t));
}

inline void* arena_alloc_elem(Arena* arena, size_t size) {
    if (!arena) return nullptr;
    return arena->allocate(size, alignof(std::max_align_t));
}

inline void arena_reset(Arena* arena) {
    if (arena) arena->reset();
}

inline void arena_clear(Arena* arena) {
    if (arena) arena->reset();
}
