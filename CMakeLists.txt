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

find_package(OpenGL REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(Threads REQUIRED)

target_link_libraries(Milton
  OpenGL::OpenGL
  SDL2::SDL2main
  SDL2::SDL2
  Threads::Threads
)

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
