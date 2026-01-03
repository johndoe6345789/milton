// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

// Modern C++20 Milton Headers
// All files split into focused modules <150 LOC each

// Core utilities
#include "core/memory_compat.h"
#include "core/arena.h"
#include "core/arena_allocator.h"
#include "core/profiler.h"
#include "core/profiler_impl.h"

// Graphics system - Color
#include "graphics/color.h"
#include "graphics/color_hsv.h"

// Graphics system - Stroke
#include "graphics/brush.h"
#include "graphics/stroke.h"
#include "graphics/stroke_builder.h"
#include "graphics/stroke_list.h"

// Graphics system - Canvas
#include "graphics/layer.h"
#include "graphics/canvas.h"
#include "graphics/canvas_history.h"

// Graphics system - GUI
#include "graphics/color_picker.h"
#include "graphics/gui_widgets.h"
#include "graphics/gui.h"

// Input system
#include "input/input_types.h"
#include "input/input_events.h"
#include "input/input_state.h"
#include "input/input_bindings.h"

// I/O system
#include "io/file_handle.h"
#include "io/platform.h"
#include "io/platform_utils.h"
#include "io/serializer.h"
