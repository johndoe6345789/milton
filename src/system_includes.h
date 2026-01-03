// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui.h>


#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(push, 0)
#endif  // _WIN32 && _MSC_VER

#if defined(__clang__)
#pragma clang system_header
#endif

#define GetWindowFont _GetWindowFont

#ifdef _WIN32
/* #define VC_EXTRALEAN */
/* #define WIN32_LEAN_AND_MEAN */
#include <windows.h>
#include <windowsx.h>
#define GCL_HICON -14
#endif


// SDL
#include <SDL3/SDL.h>
// SDL_syswm.h was removed in SDL 3, native window access is now via SDL_GetProperty

// Platform independent includes:
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <xmmintrin.h>
#include <emmintrin.h>

#if defined(_WIN32)

#include "vk.h"

#elif defined(__linux__)

#include "vk.h"

#include <dlfcn.h>  // Dynamic library loading.

#elif defined (__MACH__)

#include "vk.h"

#endif // Vulkan includes

#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(pop)
#endif  // _WIN32 && _MSC_VER

