// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "profiler.h"
#include <unordered_map>
#include <vector>
#include <string>

#if MILTON_ENABLE_PROFILING

namespace milton::profiler {

// Profiler singleton
class Profiler {
public:
    static Profiler& instance();
    
    void init();
    void reset();
    void begin_frame();
    void end_frame();
    
    void begin_zone(Zone zone);
    void end_zone(Zone zone);
    
    const ZoneData& get_zone_data(Zone zone) const;
    double get_fps() const { return fps_; }
    double get_frame_time_ms() const { return frame_time_ms_; }
    
    void print_stats() const;
    std::string get_stats_string() const;
    
    struct FrameStats {
        double frame_time_ms = 0.0;
        double polling_ms = 0.0;
        double update_ms = 0.0;
        double raster_ms = 0.0;
        double gpu_ms = 0.0;
    };
    
    const std::vector<FrameStats>& get_frame_history() const { 
        return frame_history_; 
    }
    
private:
    Profiler() = default;
    
    bool initialized_ = false;
    ZoneData zones_[static_cast<size_t>(Zone::Count)];
    std::unordered_map<Zone, TimePoint> active_zones_;
    
    TimePoint frame_start_;
    double fps_ = 0.0;
    double frame_time_ms_ = 0.0;
    
    std::vector<FrameStats> frame_history_;
    static constexpr size_t MAX_FRAME_HISTORY = 300;
};

} // namespace milton::profiler

#endif // MILTON_ENABLE_PROFILING
