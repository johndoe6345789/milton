// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "platform.h"
#include "../common.h"
#include <span>
#include <vector>
#include <string>
#include <cstring>

namespace milton::platform {

// Safe string utilities
inline void safe_string_copy(char* dst, const char* src, size_t dst_size) {
    if (dst_size == 0) return;
    std::strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

inline std::string to_utf8(const std::filesystem::path& path) {
    return path.string();
}

// Endian-safe I/O
inline u16 read_u16_le(std::span<const u8> data, size_t offset) {
    return static_cast<u16>(data[offset]) |
           (static_cast<u16>(data[offset + 1]) << 8);
}

inline u32 read_u32_le(std::span<const u8> data, size_t offset) {
    return static_cast<u32>(data[offset]) |
           (static_cast<u32>(data[offset + 1]) << 8) |
           (static_cast<u32>(data[offset + 2]) << 16) |
           (static_cast<u32>(data[offset + 3]) << 24);
}

inline u64 read_u64_le(std::span<const u8> data, size_t offset) {
    return static_cast<u64>(data[offset]) |
           (static_cast<u64>(data[offset + 1]) << 8) |
           (static_cast<u64>(data[offset + 2]) << 16) |
           (static_cast<u64>(data[offset + 3]) << 24) |
           (static_cast<u64>(data[offset + 4]) << 32) |
           (static_cast<u64>(data[offset + 5]) << 40) |
           (static_cast<u64>(data[offset + 6]) << 48) |
           (static_cast<u64>(data[offset + 7]) << 56);
}

inline void write_u16_le(std::span<u8> data, size_t offset, u16 value) {
    data[offset] = static_cast<u8>(value);
    data[offset + 1] = static_cast<u8>(value >> 8);
}

inline void write_u32_le(std::span<u8> data, size_t offset, u32 value) {
    data[offset] = static_cast<u8>(value);
    data[offset + 1] = static_cast<u8>(value >> 8);
    data[offset + 2] = static_cast<u8>(value >> 16);
    data[offset + 3] = static_cast<u8>(value >> 24);
}

inline void write_u64_le(std::span<u8> data, size_t offset, u64 value) {
    data[offset] = static_cast<u8>(value);
    data[offset + 1] = static_cast<u8>(value >> 8);
    data[offset + 2] = static_cast<u8>(value >> 16);
    data[offset + 3] = static_cast<u8>(value >> 24);
    data[offset + 4] = static_cast<u8>(value >> 32);
    data[offset + 5] = static_cast<u8>(value >> 40);
    data[offset + 6] = static_cast<u8>(value >> 48);
    data[offset + 7] = static_cast<u8>(value >> 56);
}

} // namespace milton::platform
