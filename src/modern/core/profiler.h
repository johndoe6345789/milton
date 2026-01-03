// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "common.h"
#include <chrono>

#if MILTON_ENABLE_PROFILING

namespace milton::profiler {

using Clock = std::chrono::high_resolution_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;

// Profiling zones
enum class Zone {
    RenderCanvas, RenderStrokes, RenderGUI, RenderPresent,
    InputPolling, InputUpdate,
    CanvasUpdate, CanvasClipping, CanvasRaster,
    StrokeCook, StrokeUpload, StrokeClip,
    GPUUpload, GPUDraw, GPUWait,
    SystemFrame,
    Count
};

// Zone timing data
struct ZoneData {
    const char* name = nullptr;
    Duration total_time{0};
    u64 call_count = 0;
    Duration min_time{Duration::max()};
    Duration max_time{Duration::min()};
    Duration last_time{0};
    
    void reset();
    double average_ms() const;
    double total_ms() const;
    double last_ms() const;
};

// RAII profiling scope
class ScopedZone {
public:
    explicit ScopedZone(Zone zone);
    ~ScopedZone();
    
    ScopedZone(const ScopedZone&) = delete;
    ScopedZone& operator=(const ScopedZone&) = delete;
    
private:
    Zone zone_;
    TimePoint start_;
};

// Zone name lookup
const char* zone_name(Zone zone);

#define PROFILE_SCOPE(zone) milton::profiler::ScopedZone _profile_scope_##__LINE__(milton::profiler::Zone::zone)
#define PROFILE_BEGIN(zone) milton::profiler::Profiler::instance().begin_zone(milton::profiler::Zone::zone)
#define PROFILE_END(zone) milton::profiler::Profiler::instance().end_zone(milton::profiler::Zone::zone)

} // namespace milton::profiler

#else // !MILTON_ENABLE_PROFILING

namespace milton::profiler {
    enum class Zone { Count = 0 };
    class ScopedZone {
    public:
        explicit ScopedZone(Zone) {}
    };
}

#define PROFILE_SCOPE(zone) do {} while(0)
#define PROFILE_BEGIN(zone) do {} while(0)
#define PROFILE_END(zone) do {} while(0)

#endif // MILTON_ENABLE_PROFILING
