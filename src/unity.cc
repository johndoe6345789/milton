// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// NOTE: Modern C++20 headers exist in src/modern/ but are not yet integrated
// They will replace legacy code incrementally to avoid conflicts

// Legacy implementations
#include "vk_helpers.cc"
#include "localization.cc"
#include "milton.cc"
#include "renderer_vk.cc"
#include "sdl_milton.cc"
#include "utils.cc"
#include "vector.cc"

#if defined(_WIN32)
    #include "platform_windows.cc"
#elif defined(__linux__)
    #include "platform_unix.cc"
    #include "platform_linux.cc"
#elif defined(__MACH__)
    #include "platform_unix.cc"
    #include "platform_mac.mm"
#endif
#if !defined(TESTING)
    #if defined(_WIN32)
       #include "platform_main_windows.cc"
    #elif defined(__linux__)
//       #include "platform_main_unix.cc"
//       #include "platform_main_linux.cc"
    #elif defined(__MACH__)
       // #include "platform_main_unix.cc"
    #endif
#else // TESTING
   #include "tests.cc"
#endif

#include "third_party_libs.cc"
