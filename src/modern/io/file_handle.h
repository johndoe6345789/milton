// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "../common.h"
#include <filesystem>
#include <string>
#include <system_error>

namespace milton::platform {

// File handle with RAII
class FileHandle {
public:
    FileHandle() = default;
    explicit FileHandle(void* handle) : handle_(handle) {}
    ~FileHandle();
    
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    
    FileHandle(FileHandle&& other) noexcept 
        : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    
    FileHandle& operator=(FileHandle&& other) noexcept {
        if (this != &other) {
            close();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    
    bool is_open() const { return handle_ != nullptr; }
    void* get() const { return handle_; }
    void close();
    
private:
    void* handle_ = nullptr;
};

} // namespace milton::platform
