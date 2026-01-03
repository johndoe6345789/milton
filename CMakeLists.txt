cmake_minimum_required(VERSION 3.16)
project(Milton)

# Prefer GLVND when available and silence the dev warning on newer CMake.
if(POLICY CMP0072)
  cmake_policy(SET CMP0072 NEW)
  set(OpenGL_GL_PREFERENCE GLVND)
endif()

if(WIN32)
  set(DIRECTX 0)
endif()

add_executable(shadergen
  src/shadergen.cc
)

file(GLOB ShaderSources src/*.glsl third_party/*.glsl)

add_executable(Milton WIN32 MACOSX_BUNDLE
  src/unity.cc
  src/shaders.gen.h
)

target_include_directories(Milton PRIVATE
  src
  third_party
  third_party/imgui
)

# Default build type to Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DMILTON_DEBUG=1)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_definitions(-DMILTON_DEBUG=0)
else()
  message(FATAL_ERROR
    "Unknown build type: ${CMAKE_BUILD_TYPE}. Supported build types: Release, Debug.")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

message(STATUS "Building ${CMAKE_BUILD_TYPE}")

if(UNIX)
  set(UnixCFlags
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
    #-Werror
  )

  if(APPLE)
    set(UnixCFlags "${UnixCFlags}" -ObjC++)
  endif()

  target_compile_options(shadergen PRIVATE ${UnixCFlags})
  target_compile_options(Milton PRIVATE ${UnixCFlags})
endif()

# SDL2:
# - On Linux: provided by Conan via CMakeDeps => find_package(SDL2 CONFIG REQUIRED)
# - On macOS/Windows: uses vendored SDL2 in third_party/SDL2-2.0.8 (as before)
set(SDL2DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2-2.0.8)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

  find_package(OpenGL REQUIRED)
  find_package(GTK2 2.6 REQUIRED gtk)
  find_package(X11 REQUIRED)
  find_library(XINPUT_LIBRARY libXi.so)
  find_package(Threads REQUIRED)

  # Conan 2.x: CMakeDeps generates SDL2 CONFIG package with imported targets.
  find_package(SDL2 CONFIG REQUIRED)

  if(XINPUT_LIBRARY STREQUAL "XINPUT_LIBRARY-NOTFOUND")
    message(FATAL_ERROR "Could not find libXi.so")
  endif()

  if(NOT OPENGL_FOUND)
    message(FATAL_ERROR "Could not find OpenGL libraries")
  endif()

  if(NOT GTK2_FOUND)
    message(FATAL_ERROR "Could not find GTK2 libraries")
  endif()

  if(NOT X11_FOUND)
    message(FATAL_ERROR "Could not find X11 libraries")
  endif()

  target_include_directories(Milton PRIVATE
    ${GTK2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
    ${OPENGL_INCLUDE_DIR}
  )

  target_link_libraries(Milton
    ${GTK2_LIBRARIES}
    ${X11_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${XINPUT_LIBRARY}
    SDL2::SDL2main
    SDL2::SDL2
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
  )

else()
  add_subdirectory(${SDL2DIR})
  target_link_libraries(Milton SDL2-static)
endif()

if(APPLE)
  target_link_libraries(Milton "-framework OpenGL")
endif()

if(WIN32 OR APPLE)
  target_include_directories(Milton PRIVATE
    ${SDL2DIR}/include
  )
endif()

add_custom_command(TARGET Milton POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_LIST_DIR}/milton_icon.ico
    ${CMAKE_CURRENT_LIST_DIR}/third_party/Carlito.ttf
    ${CMAKE_CURRENT_LIST_DIR}/third_party/Carlito.LICENSE
    $<TARGET_FILE_DIR:Milton>
)

add_dependencies(Milton shadergen)

add_custom_command(
  COMMAND $<TARGET_FILE:shadergen>
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  DEPENDS ${ShaderSources}
  OUTPUT src/shaders.gen.h
)
