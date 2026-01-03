// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "platform.h"
#include "../common.h"
#include <span>
#include <vector>
#include <type_traits>

namespace milton::persist {

// Binary serializer with error handling
class BinarySerializer {
public:
    explicit BinarySerializer(std::vector<u8>& buffer)
        : buffer_(buffer), read_pos_(0) {}
    
    // Writing
    template<typename T>
    requires std::is_trivially_copyable_v<T>
    void write(const T& value) {
        const u8* bytes = reinterpret_cast<const u8*>(&value);
        buffer_.insert(buffer_.end(), bytes, bytes + sizeof(T));
    }
    
    void write_bytes(std::span<const u8> data) {
        buffer_.insert(buffer_.end(), data.begin(), data.end());
    }
    
    void write_string(const std::string& str) {
        write<u32>(static_cast<u32>(str.size()));
        write_bytes(std::span<const u8>(
            reinterpret_cast<const u8*>(str.data()), str.size()));
    }
    
    // Reading
    template<typename T>
    requires std::is_trivially_copyable_v<T>
    platform::PlatformResult<T> read() {
        if (read_pos_ + sizeof(T) > buffer_.size()) {
            return std::unexpected(platform::PlatformError::IoError);
        }
        
        T value;
        std::memcpy(&value, buffer_.data() + read_pos_, sizeof(T));
        read_pos_ += sizeof(T);
        return value;
    }
    
    platform::PlatformResult<std::span<const u8>> read_bytes(size_t count) {
        if (read_pos_ + count > buffer_.size()) {
            return std::unexpected(platform::PlatformError::IoError);
        }
        
        std::span<const u8> data(buffer_.data() + read_pos_, count);
        read_pos_ += count;
        return data;
    }
    
    platform::PlatformResult<std::string> read_string() {
        auto size_result = read<u32>();
        if (!size_result) return std::unexpected(size_result.error());
        
        auto data_result = read_bytes(*size_result);
        if (!data_result) return std::unexpected(data_result.error());
        
        return std::string(
            reinterpret_cast<const char*>(data_result->data()),
            data_result->size()
        );
    }
    
    size_t position() const { return read_pos_; }
    void seek(size_t pos) { read_pos_ = pos; }
    size_t size() const { return buffer_.size(); }
    
private:
    std::vector<u8>& buffer_;
    size_t read_pos_;
};

} // namespace milton::persist
