# Milton - Build Instructions

Milton is a paint application with tablet support, recently upgraded to **SDL 3.2.20**.

## Prerequisites

### Required Tools
- **CMake** 3.16 or higher
- **Conan** 2.0+ (package manager)
- **Perl** 5.10+ (required for build scripts)
- **C++ Compiler** with C++11 support (GCC 15+ recommended for Linux)
- **OpenGL** development libraries

### Perl Modules
The following Perl modules are required for shader generation and build scripts:

#### Core Modules (Essential)
- `FindBin` - locate Perl modules
- `IPC::Cmd` - execute external commands
- `File::Compare` - compare files
- `Time::Piece` - date/time handling
- `threads` - multithreading
- `Thread::Queue` - thread-safe queues

#### Full Module Installation

##### Linux (Fedora/RHEL)
```bash
# Core Perl and development tools
sudo dnf install perl perl-devel

# Install all required modules
sudo dnf install perl-FindBin perl-IPC-Cmd perl-File-Compare perl-Time-Piece \
                 perl-threads perl-Thread-Queue perl-Queue-DBI
```

##### Linux (Ubuntu/Debian)
```bash
sudo apt-get install perl perl-modules libfile-compare-perl libipc-cmd-perl \
                     libtime-piece-perl libthread-queue-perl
```

#### macOS
```bash
# Perl comes with macOS, but install via Homebrew for latest
brew install perl

# Then use CPAN or cpanminus for modules
cpan File::Compare IPC::Cmd Time::Piece
```

#### Windows
Download and install ActivePerl or Strawberry Perl from [perl.org](https://www.perl.org/)

### System Dependencies (after Perl/CMake/Conan installed)

#### Linux (Fedora/RHEL)
```bash
sudo dnf install cmake gcc-c++ opengl-devel xorg-x11-devel
```

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get install cmake g++ libgl1-mesa-dev libx11-dev
```

#### macOS
```bash
brew install cmake
# Xcode Command Line Tools required
xcode-select --install
```

#### Windows
- Visual Studio 2015+ with C++ support
- Or MinGW with GCC

### Step 1: Install Dependencies with Conan

```bash
cd /path/to/milton
conan install .
```

This will:
- Create a `build` directory automatically
- Download SDL 3.2.20 and its dependencies
- Generate CMake configuration files (`CMakeDeps`, `CMakeToolchain`)
- Create build environment scripts (`conanbuild.sh`, `conanrun.sh`)

### Step 2: Configure with CMake

```bash
cd build
# Using Conan's preset (requires CMake 3.23+)
cmake .. --preset conan-release

# Or manually with toolchain
cmake .. -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
```

### Step 3: Build

```bash
cmake --build . -- -j$(nproc)
# Or use make directly
make -j$(nproc)
```

### Step 4: Run

```bash
./Milton
```

## Project Structure

```
milton/
├── src/                    # Source code
│   ├── sdl_milton.cc      # Main SDL event loop
│   ├── platform_*.cc      # Platform-specific code (Linux, Windows, macOS)
│   ├── *.glsl             # OpenGL shaders
│   └── ...
├── CMakeLists.txt         # CMake build configuration
├── conanfile.txt          # Conan dependencies
└── README.md              # This file
```

## SDL 3 Migration

This project has been updated from **SDL 2.30.10** to **SDL 3.2.20**. Key changes:

### Major API Changes
- **Event API**: `SDL_SYSWMEVENT` removed - tablet input now handled via polling
- **Window Info**: `SDL_GetWindowWMInfo()` removed - replaced with `SDL_GetProperty()`
- **Event Fields**: `windowID` → `window_id` across all event structures

### Platform-Specific Wrappers
New wrapper functions handle platform window handles via SDL 3's property API:
- `platform_get_native_window_pointer()` - Gets HWND (Windows), Window (Linux), NSWindow (macOS)
- `platform_get_native_display_pointer()` - Gets Display pointer (Linux X11 only)
- `platform_handle_tablet_input()` - Polls tablet input each frame

### Updated Files
- [src/sdl_milton.cc](src/sdl_milton.cc) - Event handling
- [src/platform.h](src/platform.h) - Platform interface
- [src/platform_linux.cc](src/platform_linux.cc) - Linux implementation
- [src/platform_windows.cc](src/platform_windows.cc) - Windows implementation
- [src/platform_mac.mm](src/platform_mac.mm) - macOS implementation

## Features

- **Drawing Tools**: Pen, eraser, brush size adjustment
- **Tablet Support**: Wacom and compatible tablets (via EasyTab)
- **Layers**: Basic layer blending
- **Export**: Rectangle selection and image export
- **OpenGL Rendering**: Hardware-accelerated painting

## Platform Notes

### Linux/X11
- Uses X11 for native window access
- EasyTab tablet support enabled
- Wayland support via SDL 3

### Windows
- Native Windows API integration
- DPI awareness enabled
- EasyTab tablet support enabled

### macOS
- Cocoa integration
- Metal rendering support

## Troubleshooting

### "Package 'sdl/3.2.20' not resolved"
- Ensure Conan remotes are configured: `conan remote list`
- Try: `conan remove "*" --confirm` and re-run `conan install ..` (from the build directory)

### CMake configuration fails
- Verify CMake is in PATH: `cmake --version`
- Check Conan toolchain exists: `conan_toolchain.cmake`
- Ensure all dependencies are installed

### Build fails with missing headers
- Run `conan install .. --build=missing` to build SDL from source (from the build directory)
- Check that OpenGL development files are installed

### Tablet input not working
- Verify EasyTab is properly initialized (check console output)
- On Linux: Ensure X11 libraries are available
- On Windows: Install latest graphics drivers

## Development

### Cleaning Build Artifacts
To clean all generated files and start fresh:
```bash
./clean.sh
```

This removes the build directory, Conan-generated files, and CMake config files from the root.

### Generating Shader Headers
Shaders are pre-compiled. To regenerate:
```bash
cmake --build . --target shadergen
./shadergen
```

### Code Style
- C++11 compatible
- Platform-specific code wrapped in `#if defined(_MSC_VER)` etc.
- Comments explain SDL 3 compatibility considerations

## License

See LICENSE file for details.

## Recent Changes

### SDL 3 Migration (January 2026)
- Updated from SDL 2.30.10 to SDL 3.2.20
- Removed deprecated SYSWMEVENT handling
- Implemented SDL property-based window handle retrieval
- All platforms now compile without errors
- Tablet input support restored via polling

## See Also

- [SDL 3 Documentation](https://wiki.libsdl.org/SDL3/)
- [Conan Package Manager](https://conan.io/)
- [CMake Documentation](https://cmake.org/cmake/help/latest/)
