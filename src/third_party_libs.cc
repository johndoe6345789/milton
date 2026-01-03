// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#if defined(_WIN32)
#pragma warning(push,0)
#endif

    // All third-party libraries now come from Conan

    // STB image write implementation
    #define STB_IMAGE_WRITE_IMPLEMENTATION
    #include <stb_image_write.h>

    // EasyTab implementation (header-only library for tablet input)
    #define EASYTAB_IMPLEMENTATION
    #include "easytab.h"

    // ImGui backends
    #define IMGUI_IMPL_OPENGL_LOADER_CUSTOM

    // X11 headers define Status as int which conflicts with ImGui's tex->Status
    #ifdef Status
    #undef Status
    #endif

    #include <imgui_impl_opengl3.cpp>
    #include <imgui_impl_sdl3.cpp>

#if defined(_WIN32)
#pragma warning(pop)
#endif
