// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "file_handle.h"
#include <filesystem>
#include <optional>
#include <expected>
#include <string>

namespace milton::platform {

// Error types
enum class PlatformError {
    FileNotFound,
    AccessDenied,
    IoError,
    InvalidPath,
    Unknown,
};

// Result type (C++23 std::expected or polyfill)
template<typename T>
using PlatformResult = std::expected<T, PlatformError>;

// File operations
class Platform {
public:
    static PlatformResult<FileHandle> open_file(
        const std::filesystem::path& path,
        const char* mode
    );
    
    static PlatformResult<std::vector<u8>> read_file(
        const std::filesystem::path& path
    );
    
    static PlatformResult<void> write_file(
        const std::filesystem::path& path,
        std::span<const u8> data
    );
    
    static std::filesystem::path get_config_dir();
    static std::filesystem::path get_exe_dir();
    
    // Dialog functions
    static std::optional<std::filesystem::path> open_dialog(
        const char* filter = nullptr
    );
    
    static std::optional<std::filesystem::path> save_dialog(
        const char* filter = nullptr,
        const char* default_ext = nullptr
    );
};

} // namespace milton::platform
