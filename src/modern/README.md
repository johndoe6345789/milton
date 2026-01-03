# Milton Modern C++20 Headers

All modern headers have been split into focused modules under 150 lines of code each.

## Directory Structure

```
modern/
├── core/              # Core utilities (5 files)
├── graphics/          # Rendering, strokes, canvas, GUI (11 files)
├── input/             # Input handling and bindings (4 files)
├── io/                # File I/O and serialization (4 files)
├── milton_modern.h    # Master include file
└── README.md          # This file
```

## Core Utilities (5 files, 342 LOC)

- **memory_compat.h** (38 LOC) - Legacy API compatibility wrappers
- **arena.h** (100 LOC) - Arena allocator with std::pmr
- **arena_allocator.h** (61 LOC) - STL allocator adapter for Arena
- **profiler.h** (80 LOC) - Profiling zones, types, and RAII scope
- **profiler_impl.h** (64 LOC) - Profiler singleton implementation

## Graphics System (11 files, 751 LOC)

### Color (2 files)
- **color.h** (97 LOC) - RGBA conversions, blending, premultiplied alpha
- **color_hsv.h** (59 LOC) - HSV color space conversions

### Stroke (4 files)
- **brush.h** (44 LOC) - BrushState struct and StrokeFlags enum
- **stroke.h** (111 LOC) - Modern Stroke class with dynamic points
- **stroke_builder.h** (63 LOC) - StrokeBuilder and smoothing utilities
- **stroke_list.h** (69 LOC) - StrokeList using std::deque for stable pointers

### Canvas (3 files)
- **layer.h** (43 LOC) - Layer class with stroke collection
- **canvas.h** (65 LOC) - Canvas class with layer hierarchy
- **canvas_history.h** (81 LOC) - Undo/redo history system

### GUI (3 files)
- **color_picker.h** (58 LOC) - HSV color picker widget
- **gui_widgets.h** (48 LOC) - Exporter and Button widgets
- **gui.h** (51 LOC) - Main MiltonGui class

## Input System (4 files, 207 LOC)

- **input_types.h** (48 LOC) - KeyCode, MouseButton, ModifierKeys enums
- **input_events.h** (48 LOC) - Key, mouse, wheel event structs
- **input_state.h** (51 LOC) - InputState tracker with std::variant events
- **input_bindings.h** (60 LOC) - ActionBindings template for key/mouse mapping

## I/O System (4 files, 260 LOC)

- **file_handle.h** (45 LOC) - RAII FileHandle wrapper
- **platform.h** (58 LOC) - Platform class with file operations
- **platform_utils.h** (73 LOC) - Safe string utilities, endian-safe I/O
- **serializer.h** (84 LOC) - BinarySerializer with std::expected error handling

## Statistics

- **Total files**: 26 headers
- **Total LOC**: ~1,560 lines
- **Largest file**: stroke.h (111 LOC)
- **Average file size**: 60 LOC
- **All files**: Under 150 LOC ✓

## Modern C++20 Features Used

- `std::deque` for stable pointers in StrokeList
- `std::unique_ptr` for ownership semantics
- `std::optional` for nullable values
- `std::variant` for type-safe unions (InputEvent)
- `std::expected` for error handling without exceptions
- `std::pmr::monotonic_buffer_resource` for arena allocation
- `std::chrono::high_resolution_clock` for profiling
- `std::filesystem::path` for cross-platform paths
- Template constraints with `requires` clause
- Designated initializers
- `[[nodiscard]]` attributes
- CTAD (Class Template Argument Deduction)

## Usage

Include the master header to get all modern APIs:

```cpp
#include "modern/milton_modern.h"
```

Or include individual subsystems:

```cpp
#include "modern/graphics/stroke.h"
#include "modern/input/input_state.h"
```

## Migration from Legacy Code

The modern headers coexist with legacy code. To migrate:

1. Include modern headers alongside legacy ones
2. Use modern APIs in new code
3. Gradually replace legacy code with modern equivalents
4. Legacy compatibility wrappers in `core/memory_compat.h`

## Design Principles

- **Single Responsibility**: Each file has one clear purpose
- **Small Files**: All files under 150 LOC for easy comprehension
- **Modern C++**: Uses C++20 features for safety and expressiveness
- **No Exceptions**: Uses `std::expected` for error handling
- **Value Semantics**: Prefers values over pointers
- **RAII**: Automatic resource management
- **Type Safety**: Strong typing, minimal casts
