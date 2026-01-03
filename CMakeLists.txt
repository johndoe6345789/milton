cmake_minimum_required(VERSION 3.16)
project(Milton)

# Prevent in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In-source builds are not allowed. Please use the 'build' folder:\n  mkdir -p build\n  cd build\n  cmake ..\n  cmake --build .")
endif()

# Prefer GLVND when available and silence the dev warning on newer CMake.
if(POLICY CMP0072)
  cmake_policy(SET CMP0072 NEW)
  set(OpenGL_GL_PREFERENCE GLVND)
endif()

# Generate shader header using Perl script
file(GLOB ShaderSources src/*.spvasm)

# Find Perl
find_package(Perl REQUIRED)

# Find spirv-as for assembling SPIR-V
find_program(SPIRV_AS spirv-as)
if(NOT SPIRV_AS)
  message(FATAL_ERROR "spirv-as not found. Install spirv-tools or set SPIRV_AS.")
endif()

# Custom command to generate shaders.gen.h using Perl script
add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/src/shaders.gen.h
  COMMAND ${CMAKE_COMMAND} -E env SPIRV_AS=${SPIRV_AS}
          ${PERL_EXECUTABLE} ${CMAKE_SOURCE_DIR}/generate_shaders.pl ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/shaders.gen.h
  DEPENDS ${ShaderSources} ${CMAKE_SOURCE_DIR}/generate_shaders.pl
  COMMENT "Generating shader header file with Perl"
  VERBATIM
)

# Custom target to force shader generation
add_custom_target(generate_shaders DEPENDS ${CMAKE_SOURCE_DIR}/src/shaders.gen.h)

add_executable(Milton WIN32 MACOSX_BUNDLE
  src/unity.cc
  src/shaders.gen.h
  # ImGui SDL3 backend is now included in unity build via third_party_libs.cc
  # src/imgui_impl_sdl3.cpp
  # SDL3 has native threading support, no shim needed
  # src/sdl3_threading_shim.c
)

target_include_directories(Milton PRIVATE
  src
)

# Find packages first to get their variables
set(CMAKE_MODULE_PATH
  "${CMAKE_BINARY_DIR}"
  "${CMAKE_BINARY_DIR}/generators"
  ${CMAKE_MODULE_PATH}
)
set(CMAKE_PREFIX_PATH
  "${CMAKE_BINARY_DIR}"
  "${CMAKE_BINARY_DIR}/generators"
  ${CMAKE_PREFIX_PATH}
)

# Find packages before using them
find_package(Vulkan REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(stb CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig)
find_package(X11)

# Get imgui package root directory - use conan variables directly
# Conan provides imgui_INCLUDE_DIRS_RELEASE variable
if(DEFINED imgui_INCLUDE_DIRS_RELEASE)
  list(GET imgui_INCLUDE_DIRS_RELEASE 0 IMGUI_PACKAGE_INCLUDE)
elseif(DEFINED imgui_INCLUDE_DIR_RELEASE)
  set(IMGUI_PACKAGE_INCLUDE "${imgui_INCLUDE_DIR_RELEASE}")
else()
  # Fallback - get from target and evaluate
  get_target_property(IMGUI_INCLUDE_DIRS imgui::imgui INTERFACE_INCLUDE_DIRECTORIES)
  # Use COMMAND_EXPAND_LISTS to handle generator expressions
  string(GENEX_STRIP "${IMGUI_INCLUDE_DIRS}" IMGUI_INCLUDE_DIRS_STRIPPED)
  list(GET IMGUI_INCLUDE_DIRS_STRIPPED 0 IMGUI_PACKAGE_INCLUDE)
endif()
get_filename_component(IMGUI_PACKAGE_DIR "${IMGUI_PACKAGE_INCLUDE}" DIRECTORY)

# STB is header-only - get from conan variables
if(DEFINED stb_INCLUDE_DIRS_RELEASE)
  list(GET stb_INCLUDE_DIRS_RELEASE 0 STB_INCLUDE)
elseif(DEFINED stb_INCLUDE_DIR_RELEASE)
  set(STB_INCLUDE "${stb_INCLUDE_DIR_RELEASE}")
else()
  get_target_property(STB_INCLUDE_DIRS stb::stb INTERFACE_INCLUDE_DIRECTORIES)
  string(GENEX_STRIP "${STB_INCLUDE_DIRS}" STB_INCLUDE_DIRS_STRIPPED)
  list(GET STB_INCLUDE_DIRS_STRIPPED 0 STB_INCLUDE)
endif()

target_include_directories(Milton PRIVATE
  "${STB_INCLUDE}"
  "${IMGUI_PACKAGE_DIR}/res/bindings"
)

# Find GTK3 for Linux file dialogs
if(UNIX AND NOT APPLE)
  if(PkgConfig_FOUND)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    include_directories(${GTK3_INCLUDE_DIRS})
  endif()
endif()

# Link libraries to target
target_link_libraries(Milton
  PRIVATE
  Vulkan::Vulkan
  SDL3::Headers
  SDL3::SDL3
  imgui::imgui
  libjpeg-turbo::turbojpeg-static
  Threads::Threads
)

# Link GTK3 on Linux
if(UNIX AND NOT APPLE AND GTK3_FOUND)
  target_link_libraries(Milton PRIVATE ${GTK3_LIBRARIES})
endif()

# Optional X11/XInput for tablet support (no hard dependency; works on Wayland too).
if(UNIX AND X11_FOUND AND X11_Xi_FOUND)
  target_link_libraries(Milton PRIVATE ${X11_Xi_LIB})
endif()

if(UNIX)
  target_compile_options(Milton PRIVATE
    -std=c++11
    -Wno-missing-braces
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-unused-result
    -Wno-write-strings
    -Wno-null-dereference
    -Wno-format
    -fno-strict-aliasing
    -fno-omit-frame-pointer
  )

  if(APPLE)
    target_compile_options(Milton PRIVATE -ObjC++)
  endif()
endif()

add_custom_command(TARGET Milton POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_LIST_DIR}/milton_icon.ico
    $<TARGET_FILE_DIR:Milton>
)

add_dependencies(Milton generate_shaders)
