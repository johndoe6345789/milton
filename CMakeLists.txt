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
)

target_include_directories(Milton PRIVATE
  src
)

# Find packages before using them
find_package(OpenGL REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(stb CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig)

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
  stb::stb
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

add_custom_command(
  COMMAND $<TARGET_FILE:shadergen>
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  DEPENDS ${ShaderSources}
  OUTPUT src/shaders.gen.h
)
