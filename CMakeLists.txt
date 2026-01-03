cmake_minimum_required(VERSION 3.16)
project(Milton)

# Prefer GLVND when available and silence the dev warning on newer CMake.
if(POLICY CMP0072)
  cmake_policy(SET CMP0072 NEW)
  set(OpenGL_GL_PREFERENCE GLVND)
endif()

add_executable(shadergen
  src/shadergen.cc
)

file(GLOB ShaderSources src/*.glsl)

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
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})

# Find packages before using them
find_package(OpenGL REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig)

# Get imgui package root directory from target properties
get_target_property(IMGUI_INCLUDE_DIRS imgui::imgui INTERFACE_INCLUDE_DIRECTORIES)
# Extract the package directory (remove the $<BUILD_INTERFACE:...> generator expression)
string(REGEX REPLACE "\\$<BUILD_INTERFACE:([^>]+)>.*" "\\1" IMGUI_PACKAGE_INCLUDE "${IMGUI_INCLUDE_DIRS}")
get_filename_component(IMGUI_PACKAGE_DIR "${IMGUI_PACKAGE_INCLUDE}" DIRECTORY)

# STB is header-only - get from stb target
get_target_property(STB_INCLUDE_DIRS stb::stb INTERFACE_INCLUDE_DIRECTORIES)
string(REGEX REPLACE "\\$<BUILD_INTERFACE:([^>]+)>.*" "\\1" STB_INCLUDE "${STB_INCLUDE_DIRS}")

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
  OpenGL::OpenGL
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

if(UNIX)
  target_compile_options(Milton PRIVATE
    -std=c++11
    -Wno-missing-braces
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-unused-result
    -Wno-write-strings
    -Wno-c++11-compat-deprecated-writable-strings
    -Wno-null-dereference
    -Wno-format
    -fno-strict-aliasing
    -fno-omit-frame-pointer
    -Wno-extern-c-compat
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

add_dependencies(Milton shadergen)
