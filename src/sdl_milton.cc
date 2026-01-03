// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include <imgui.h>
// SDL 3 migration: Using ImGui SDL3 backend from Conan
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include "milton.h"
#include "vk.h"
#include "gui.h"
#include "persist.h"
#include "bindings.h"


static void
cursor_set_and_show(SDL_Cursor* cursor)
{
    SDL_SetCursor(cursor);
    platform_cursor_show();
}

LayoutType
get_current_keyboard_layout()
{
    LayoutType layout = LayoutType_QWERTY;  // Default to QWERTY bindings.

    char keys[] = {
        (char)SDL_GetKeyFromScancode(SDL_SCANCODE_Q, SDL_KMOD_NONE, false),
        (char)SDL_GetKeyFromScancode(SDL_SCANCODE_R, SDL_KMOD_NONE, false),
        (char)SDL_GetKeyFromScancode(SDL_SCANCODE_Y, SDL_KMOD_NONE, false),
        '\0',
    };

    if ( strcmp(keys, "qry") == 0 ) {
        layout = LayoutType_QWERTY;
    }
    else if ( strcmp(keys, "ary") == 0 ) {
        layout = LayoutType_AZERTY;
    }
    else if ( strcmp(keys, "qrz") == 0 ) {
        layout = LayoutType_QWERTZ;
    }
    else if ( strcmp(keys, "q,f") ) {
        layout = LayoutType_DVORAK;
    }
    else if ( strcmp(keys, "qwj") == 0 ) {
        layout = LayoutType_COLEMAK;
    }

    return layout;
}

void
shortcut_handle_key(Milton* milton, PlatformState* platform, SDL_Event* event, MiltonInput* input, b32 is_keyup)
{
    ImGuiIO& io = ImGui::GetIO();

    // ImGui SDL3 backend handles keyboard input internally via ImGui_ImplSDL3_ProcessEvent()
    // so we don't need to manually update io.KeysDown, io.KeyShift, etc.
    if (!io.WantCaptureKeyboard) {
        MiltonBindings* bindings = &milton->settings->bindings;

        SDL_Keymod m = SDL_GetModState();
        SDL_Keycode k = event->key.key;

        i8 active_key = 0;
        if (k >= 1 && k <= 127) {
            active_key = k;
        }
        else {
            switch (k) {
                case SDLK_F1:           { active_key = Binding::F1;  } break;
                case SDLK_F2:           { active_key = Binding::F2;  } break;
                case SDLK_F3:           { active_key = Binding::F3;  } break;
                case SDLK_F4:           { active_key = Binding::F4;  } break;
                case SDLK_F5:           { active_key = Binding::F5;  } break;
                case SDLK_F6:           { active_key = Binding::F6;  } break;
                case SDLK_F7:           { active_key = Binding::F7;  } break;
                case SDLK_F8:           { active_key = Binding::F8;  } break;
                case SDLK_F9:           { active_key = Binding::F9;  } break;
                case SDLK_F10:          { active_key = Binding::F10; } break;
                case SDLK_F11:          { active_key = Binding::F11; } break;
                case SDLK_F12:          { active_key = Binding::F12; } break;
                case SDLK_KP_0:         { active_key = Binding::KP_0; } break;
                case SDLK_KP_1:         { active_key = Binding::KP_1; } break;
                case SDLK_KP_2:         { active_key = Binding::KP_2; } break;
                case SDLK_KP_3:         { active_key = Binding::KP_3; } break;
                case SDLK_KP_4:         { active_key = Binding::KP_4; } break;
                case SDLK_KP_5:         { active_key = Binding::KP_5; } break;
                case SDLK_KP_6:         { active_key = Binding::KP_6; } break;
                case SDLK_KP_7:         { active_key = Binding::KP_7; } break;
                case SDLK_KP_8:         { active_key = Binding::KP_8; } break;
                case SDLK_KP_9:         { active_key = Binding::KP_9; } break;
                case SDLK_KP_PLUS:      { active_key = Binding::KP_PLUS; } break;
                case SDLK_KP_MINUS:     { active_key = Binding::KP_MINUS; } break;
                case SDLK_KP_PERIOD:    { active_key = Binding::KP_PERIOD; } break;
                case SDLK_KP_DIVIDE:    { active_key = Binding::KP_DIVIDE; } break;
                case SDLK_KP_MULTIPLY:  { active_key = Binding::KP_MULTIPLY; } break;
                default: {  } break;
            }
        }

        u32 active_modifiers = 0;

        if (m & SDL_KMOD_CTRL) { active_modifiers |= Modifier_CTRL; }
        if (m & SDL_KMOD_SHIFT) { active_modifiers |= Modifier_SHIFT; }
        if (m & SDL_KMOD_GUI) { active_modifiers |= Modifier_WIN; }
        if (m & SDL_KMOD_ALT) { active_modifiers |= Modifier_ALT; }
        if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_SPACE]) { active_modifiers |= Modifier_SPACE; }

        if (is_keyup) {

            // Switch on k again, this time catching when some of the modifiers were released.
            switch (k) {
                case SDLK_LSHIFT:
                case SDLK_RSHIFT: {
                    active_modifiers |= Modifier_SHIFT;
                } break;
                case SDLK_LALT:
                case SDLK_RALT: {
                    active_modifiers |= Modifier_ALT;
                } break;
                case SDLK_LGUI:
                case SDLK_RGUI: {
                    active_modifiers |= Modifier_WIN;
                } break;
                case SDLK_LCTRL:
                case SDLK_RCTRL: {
                    active_modifiers |= Modifier_CTRL;
                } break;
            }

            for (sz i = Action_COUNT + 1; i < Action_COUNT_WITH_RELEASE; ++i) {
                Binding* b = &bindings->bindings[i];
                if ( (!event->key.repeat || b->accepts_repeats) &&
                     active_modifiers == b->modifiers &&
                     active_key == b->bound_key &&
                     b->on_release &&
                     b->action != Action_NONE ) {
                    binding_dispatch_action(b->action, input, milton, platform->pointer);
                    platform->force_next_frame = true;
                }
            }
        }
        // keydown
        else  {
            for (sz i = 0; i < Action_COUNT; ++i) {
                Binding* b = &bindings->bindings[i];

                if ( (!event->key.repeat || b->accepts_repeats) &&
                     active_modifiers == b->modifiers &&
                     active_key == b->bound_key &&
                     !b->on_release &&
                     b->action != Action_NONE ) {
                    binding_dispatch_action(b->action, input, milton, platform->pointer);
                    platform->force_next_frame = true;
                }
            }

        }
        if ( k == SDLK_SPACE && !is_keyup ) {
            platform->is_space_down = true;
        }
    }
}

void
panning_update(PlatformState* platform)
{
    auto reset_pan_start = [platform]() {
        platform->pan_start = VEC2L(platform->pointer);
        platform->pan_point = platform->pan_start;  // No huge pan_delta at beginning of pan.
    };

    platform->was_panning = platform->is_panning;

    // Panning from GUI menu, waiting for input
    if ( platform->waiting_for_pan_input ) {
        if ( platform->is_pointer_down ) {
            platform->waiting_for_pan_input = false;
            platform->is_panning = true;
            reset_pan_start();
        }
        // Space cancels waiting
        if ( platform->is_space_down ) {
            platform->waiting_for_pan_input = false;
        }
    }
    else {
        if ( platform->is_panning ) {
            if ( (!platform->is_pointer_down && !platform->is_space_down)
                 || !platform->is_pointer_down ) {
                platform->is_panning = false;
            }
            else {
                platform->pan_point = VEC2L(platform->pointer);
            }
        }
        else {
            if ( (platform->is_space_down && platform->is_pointer_down)
                 || platform->is_middle_button_down ) {
                platform->is_panning = true;
                reset_pan_start();
            }
        }
    }
}

MiltonInput
sdl_event_loop(Milton* milton, PlatformState* platform)
{
    MiltonInput milton_input = {};
    milton_input.mode_to_set = MiltonMode::MODE_COUNT;

    b32 pointer_up = false;

    v2i input_point = {};

    platform->num_pressure_results = 0;
    platform->num_point_results = 0;
    platform->keyboard_layout = get_current_keyboard_layout();

    SDL_Event event;
    while ( SDL_PollEvent(&event) ) {
        ImGui_ImplSDL3_ProcessEvent(&event);

        SDL_Keymod keymod = SDL_GetModState();

#if 0
        if ( (keymod & KMOD_ALT) )
        {
            milton_input.mode_to_set = MiltonMode_EYEDROPPER;
        }
#endif


#if defined(_MSC_VER)
#pragma warning (push)
#pragma warning (disable : 4061)
#endif
        switch ( event.type ) {
            case SDL_EVENT_QUIT: {
                platform_cursor_show();
                milton_try_quit(milton);
            } break;
            // SDL_SYSWMEVENT removed in SDL 3 - tablet input should be handled via
            // platform-specific polling or other SDL 3 input mechanisms
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                if ( event.button.windowID != platform->window_id ) {
                    break;
                }

                if (   (event.button.button == SDL_BUTTON_LEFT && ( EasyTab == NULL || !EasyTab->PenInProximity))
                     || event.button.button == SDL_BUTTON_MIDDLE
                     // Ignoring right click events for now
                     /*|| event.button.button == SDL_BUTTON_RIGHT*/ ) {

                    if ( ImGui::GetIO().WantCaptureMouse ) {
                        platform->force_next_frame = true;
                    }
                    else {
                        v2l long_point = { event.button.x, event.button.y };

                        platform_point_to_pixel(platform, &long_point);

                        v2i point = v2i{(int)long_point.x, (int)long_point.y};

                        if ( !platform->is_panning && point.x >= 0 && point.y > 0 ) {
                            milton_input.click = point;

                            platform->is_pointer_down = true;
                            platform->pointer = point;
                            platform->is_middle_button_down = (event.button.button == SDL_BUTTON_MIDDLE);

                            if ( platform->num_point_results < MAX_INPUT_BUFFER_ELEMS ) {
                                milton_input.points[platform->num_point_results++] = VEC2L(point);
                            }
                            if ( platform->num_pressure_results < MAX_INPUT_BUFFER_ELEMS ) {
                                milton_input.pressures[platform->num_pressure_results++] = NO_PRESSURE_INFO;
                            }
                        }
                    }
                }
            } break;
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                if ( event.button.windowID != platform->window_id ) {
                    break;
                }
                if ( event.button.button == SDL_BUTTON_LEFT
                     || event.button.button == SDL_BUTTON_MIDDLE
                     || event.button.button == SDL_BUTTON_RIGHT ) {
                    if ( event.button.button == SDL_BUTTON_MIDDLE ) {
                        platform->is_middle_button_down = false;
                    }
                    if ( ImGui::GetIO().WantCaptureMouse ) {
                        // NOTE(ameen): button-click events that cause UI changes have 1 frame delay to update.
                        platform->force_next_frame = true;
                    }
                    pointer_up = true;
                    milton_input.flags |= MiltonInputFlags_CLICKUP;
                    milton_input.flags |= MiltonInputFlags_END_STROKE;
                }
            } break;
            case SDL_EVENT_MOUSE_MOTION: {
                if (event.motion.windowID != platform->window_id) {
                    break;
                }

                input_point = {event.motion.x, event.motion.y};

                platform_point_to_pixel_i(platform, &input_point);

                platform->pointer = input_point;

                // In case the wacom driver craps out, or anything goes wrong (like the event queue
                // overflowing ;)) then we default to receiving WM_MOUSEMOVE. If we catch a single
                // point, then it's fine. It will get filtered out in milton_stroke_input

                if (EasyTab == NULL || !EasyTab->PenInProximity) {
                    if (platform->is_pointer_down) {
                        if (!platform->is_panning &&
                            (input_point.x >= 0 && input_point.y >= 0)) {
                            if (platform->num_point_results < MAX_INPUT_BUFFER_ELEMS) {
                                milton_input.points[platform->num_point_results++] = VEC2L(input_point);
                            }
                            if (platform->num_pressure_results < MAX_INPUT_BUFFER_ELEMS) {
                                milton_input.pressures[platform->num_pressure_results++] = NO_PRESSURE_INFO;
                            }
                        }
                    }
                }
                break;
            }
            case SDL_EVENT_MOUSE_WHEEL: {
                if ( event.wheel.windowID != platform->window_id ) {
                    break;
                }
                if ( !ImGui::GetIO().WantCaptureMouse ) {
                    milton_input.scale += event.wheel.y;
                    v2i zoom_center = platform->pointer;

                    milton_set_zoom_at_point(milton, zoom_center);
                    // ImGui has a delay of 1 frame when displaying zoom info.
                    // Force next frame to have the value up to date.
                    platform->force_next_frame = true;
                }

                break;
            }
            case SDL_EVENT_KEY_DOWN: {
                shortcut_handle_key(milton, platform, &event, &milton_input, /*is_keyup*/false);
            } break;
            case SDL_EVENT_KEY_UP: {
                if ( event.key.windowID != platform->window_id ) {
                    break;
                }

                SDL_Keycode keycode = event.key.key;

                if ( keycode == SDLK_SPACE ) {
                    platform->is_space_down = false;
                }
                shortcut_handle_key(milton, platform, &event, &milton_input, /*is_keyup*/true);
            } break;
            // SDL3 window events are now individual event types instead of SDL_WINDOWEVENT with subtypes
            case SDL_EVENT_WINDOW_MOVED: {
                if ( platform->window_id != event.window.windowID ) {
                    break;
                }
                platform->num_point_results = 0;
                platform->num_pressure_results = 0;
                platform->is_pointer_down = false;
            } break;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                if ( platform->window_id != event.window.windowID ) {
                    break;
                }
                v2i size = { event.window.data1, event.window.data2 };
                platform_point_to_pixel_i(platform, &size);

                platform->width = size.w;
                platform->height = size.h;

                milton_input.flags |= MiltonInputFlags_FULL_REFRESH;
#if USE_GL_3_2
                glViewport(0, 0, platform->width, platform->height);
#endif
            } break;
            case SDL_EVENT_WINDOW_MOUSE_LEAVE: {
                if ( event.window.windowID != platform->window_id ) {
                    break;
                }
                if ( milton->current_mode != MiltonMode::DRAG_BRUSH_SIZE ) {
                    platform_cursor_show();
                }
            } break;
            // --- A couple of events we might want to catch later...
            case SDL_EVENT_WINDOW_MOUSE_ENTER: {
            } break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED: {
            } break;
            default: {
                break;
            }
        }
#if defined(_MSC_VER)
#pragma warning (pop)
#endif
        if ( platform->should_quit ) {
            break;
        }
    }  // ---- End of SDL event loop
    
    // SDL 3: Handle tablet input via polling (replaces old SYSWMEVENT mechanism)
    platform_handle_tablet_input(platform);

    if ( pointer_up ) {
        // Add final point
        if ( !platform->is_panning && platform->is_pointer_down ) {
            milton_input.flags |= MiltonInputFlags_END_STROKE;
            input_point = { event.button.x, event.button.y };

            platform_point_to_pixel_i(platform, &input_point);

            if ( platform->num_point_results < MAX_INPUT_BUFFER_ELEMS ) {
                milton_input.points[platform->num_point_results++] = VEC2L(input_point);
            }
        }
        platform->is_pointer_down = false;

        platform->num_point_results = 0;
    }

    return milton_input;
}

// ---- milton_main

int
milton_main(bool is_fullscreen, char* file_to_open)
{
    {
        static char* release_string
#if MILTON_DEBUG
                = "Debug";
#else
                = "Release";
#endif

        milton_log("Running Milton %d.%d.%d (%s) \n", MILTON_MAJOR_VERSION, MILTON_MINOR_VERSION, MILTON_MICRO_VERSION, release_string);
    }
    // Note: Possible crash regarding SDL_main entry point.
    // Note: Event handling, File I/O and Threading are initialized by default
    milton_log("Initializing SDL... ");
    SDL_Init(SDL_INIT_VIDEO);
    milton_log("Done.\n");

    PlatformState platform = {};

    PlatformSettings prefs = {};

    milton_log("Loading preferences...\n");
    if ( platform_settings_load(&prefs) ) {
        milton_log("Prefs file window size: %dx%d\n", prefs.width, prefs.height);
    }

    i32 window_width = 1280;
    i32 window_height = 800;
    {
        if (prefs.width > 0 && prefs.height > 0) {
            if ( !is_fullscreen ) {
                window_width = prefs.width;
                window_height = prefs.height;
            }
            else {
                // TODO: Does this work on retina mac?
                milton_log("Running fullscreen\n");
                const SDL_DisplayMode* dm = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());

                window_width = dm->w;
                window_height = dm->h;
            }
        }
    }

    milton_log("Window dimensions: %dx%d \n", window_width, window_height);

    platform.ui_scale = 1.0f;

    platform.keyboard_layout = get_current_keyboard_layout();

    milton_log("Creating Milton Window with Vulkan\n");

    SDL_Window* window = NULL;

    Uint32 sdl_window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    if (is_fullscreen) {
        sdl_window_flags |= SDL_WINDOW_FULLSCREEN;
    }
    else {
        sdl_window_flags |= SDL_WINDOW_RESIZABLE;
    }

    window = SDL_CreateWindow("Milton", window_width, window_height, sdl_window_flags);

    if ( !window ) {
        milton_log("SDL Error: %s\n", SDL_GetError());
        milton_die_gracefully("SDL could not create window\n");
    }

    platform.window = window;

    // Milton works in pixels, but macOS works distinguishing "points" and
    // "pixels", with most APIs working in points.

    v2l size_px = { window_width, window_height };
    platform_point_to_pixel(&platform, &size_px);

    platform.width = size_px.w;
    platform.height = size_px.h;

    // Initialize Vulkan
    if ( !vk::init(window) ) {
        milton_die_gracefully("Could not initialize Vulkan\n");
    }

    // Init ImGUI
    ImGui::CreateContext();

    // Setup Vulkan ImGui backend
    ImGui_ImplSDL3_InitForVulkan(window);
    
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_2;
    init_info.Instance = vk::g_vk_context.instance;
    init_info.PhysicalDevice = vk::g_vk_context.physical_device;
    init_info.Device = vk::g_vk_context.device;
    init_info.QueueFamily = vk::g_vk_context.graphics_queue_family;
    init_info.Queue = vk::g_vk_context.graphics_queue;
    init_info.DescriptorPool = vk::g_vk_context.descriptor_pool;
    init_info.DescriptorPoolSize = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = vk::g_vk_context.swapchain_image_count;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.PipelineInfoMain.RenderPass = vk::g_vk_context.render_pass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.UseDynamicRendering = false;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    init_info.MinAllocationSize = 0;

    ImGui_ImplVulkan_Init(&init_info);

    // ==== Initialize milton

    Milton* milton = arena_bootstrap(Milton, root_arena, 1024*1024);

    // Platform-specific setup - SDL 3 compatibility
    // Pass window to platform_init for SDL_GetProperty access
#if defined(_MSC_VER)
#pragma warning (push, 0)
#endif
    platform_init(&platform, window);
#if defined(_MSC_VER)
#pragma warning (pop)
#endif

    platform.ui_scale = platform_ui_scale(&platform);
    milton_log("UI scale is %f\n", platform.ui_scale);
    // Initialize milton
    PATH_CHAR* file_to_open_ = NULL;
    PATH_CHAR buffer[MAX_PATH] = {};

    if ( file_to_open ) {
        file_to_open_ = (PATH_CHAR*)buffer;
    }

    str_to_path_char(file_to_open, (PATH_CHAR*)file_to_open_, MAX_PATH*sizeof(*file_to_open_));

    milton_init(milton, platform.width, platform.height, platform.ui_scale, (PATH_CHAR*)file_to_open_);
    milton->platform = &platform;
    milton->gui->menu_visible = true;
    if ( is_fullscreen ) {
        milton->gui->menu_visible = false;
    }

    milton_resize_and_pan(milton, {}, {platform.width, platform.height});

    platform.window_id = SDL_GetWindowID(window);

    i32 display_hz = platform_monitor_refresh_hz();

    platform_setup_cursor(&milton->root_arena, &platform);

    // Sometimes SDL sets the window position such that it's impossible to move
    // without using Windows shortcuts that not everyone knows. Check if this
    // is the case and set a good default.
    if (!is_fullscreen) {
        const int pixel_padding = platform_titlebar_height(&platform);
        int x = 0, y = 0;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_SetWindowPosition(window, min(max(0, x), platform.width - pixel_padding), min(max(pixel_padding, y), platform.height  - pixel_padding));
    }

    // ImGui setup
    {
        milton_log("ImGUI setup\n");
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = NULL;  // Don't save any imgui.ini file
        PATH_CHAR fname[MAX_PATH] = TO_PATH_STR("Carlito.ttf");
        platform_fname_at_exe(fname, MAX_PATH);
        FILE* fd = platform_fopen(fname, TO_PATH_STR("rb"));

        if ( fd ) {
            size_t  ttf_sz = 0;
            void*   ttf_data = NULL;
            //ImFont* im_font =  io.Fonts->ImFontAtlas::AddFontFromFileTTF("carlito.ttf", 14);
            // Load file to memory
            if ( fseek(fd, 0, SEEK_END) == 0 ) {
                long ttf_sz_long = ftell(fd);
                if ( ttf_sz_long != -1 ) {
                    ttf_sz = (size_t)ttf_sz_long;
                    if ( fseek(fd, 0, SEEK_SET) == 0 ) {
                        ttf_data = ImGui::MemAlloc(ttf_sz);
                        if ( ttf_data ) {
                            if ( fread(ttf_data, 1, ttf_sz, fd) == ttf_sz ) {
                                ImFont* im_font = io.Fonts->ImFontAtlas::AddFontFromMemoryTTF(ttf_data, (int)ttf_sz, int(14*platform.ui_scale));
                            }
                            else {
                                milton_log("WARNING: Error reading TTF file\n");
                            }
                        }
                        else {
                            milton_log("WARNING: could not allocate data for font!\n");
                        }
                    }
                }
            }
            fclose(fd);
        }
    }
    // Initialize system cursors
    {
        platform.cursor_default   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
        platform.cursor_hand      = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
        platform.cursor_crosshair = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
        platform.cursor_sizeall   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);

        cursor_set_and_show(platform.cursor_default);
    }

    // ---- Main loop ----

    while ( !platform.should_quit ) {
        PROFILE_GRAPH_END(system);
        PROFILE_GRAPH_BEGIN(polling);

        u64 frame_start_us = perf_counter();

        ImGuiIO& imgui_io = ImGui::GetIO();

        MiltonInput milton_input = sdl_event_loop(milton, &platform);

        // Handle pen orientation to switch to eraser or pen.
        if ( EasyTab != NULL && EasyTab->PenInProximity ) {
            static int previous_orientation = 0;

            // TODO: This logic needs to handle primitives, not just eraser/pen
            bool changed = false;
            if ( EasyTab->Orientation.Altitude < 0 && previous_orientation >= 0 ) {
                milton_input.mode_to_set = MiltonMode::ERASER;
                changed = true;
            }
            else if ( EasyTab->Orientation.Altitude > 0 && previous_orientation <= 0 ) {
                milton_input.mode_to_set = MiltonMode::PEN;
                changed = true;
            }
            if ( changed ) {
                previous_orientation = EasyTab->Orientation.Altitude;
            }
        }

        panning_update(&platform);

        static b32 first_run = true;
        if ( first_run ) {
            first_run = false;
            milton_input.flags = MiltonInputFlags_FULL_REFRESH;
        }

        {
            float x = 0.0f;
            float y = 0.0f;
            SDL_GetMouseState(&x, &y);

            // Convert x,y to pixels
            {
                v2l v = { (long)x, (long)y };
                platform_point_to_pixel(&platform, &v);
                x = v.x;
                y = v.y;
            }

            // NOTE: Calling SDL_SetCursor more than once seems to cause flickering.

            // Handle system cursor and platform state related to current_mode
            {
                    static b32 was_exporting = false;

                    if ( platform.is_panning || platform.waiting_for_pan_input ) {
                        cursor_set_and_show(platform.cursor_sizeall);
                    }
                    // Show resize icon
                    #if !MILTON_HARDWARE_BRUSH_CURSOR
                        #define PAD 20
                        else if (x > milton->view->screen_size.w - PAD
                             || x < PAD
                             || y > milton->view->screen_size.h - PAD
                             || y < PAD ) {
                            cursor_set_and_show(platform.cursor_default);
                        }
                        #undef PAD
                    #endif
                    else if ( ImGui::GetIO().WantCaptureMouse ) {
                        cursor_set_and_show(platform.cursor_default);
                    }
                    else if ( milton->current_mode == MiltonMode::EXPORTING ) {
                        cursor_set_and_show(platform.cursor_crosshair);
                        was_exporting = true;
                    }
                    else if ( was_exporting ) {
                        cursor_set_and_show(platform.cursor_default);
                        was_exporting = false;
                    }
                    else if ( milton->current_mode == MiltonMode::EYEDROPPER ) {
                        cursor_set_and_show(platform.cursor_crosshair);
                        platform.is_pointer_down = false;
                    }
                    else if ( milton->gui->visible
                              && is_inside_rect_scalar(get_bounds_for_picker_and_colors(&milton->gui->picker), x,y) ) {
                        cursor_set_and_show(platform.cursor_default);
                    }
                    else if ( milton->current_mode == MiltonMode::PEN ||
                              milton->current_mode == MiltonMode::ERASER ||
                              mode_is_for_primitives(milton->current_mode) ) {
                        #if MILTON_HARDWARE_BRUSH_CURSOR
                            cursor_set_and_show(platform.cursor_brush);
                        #else
                            platform_cursor_hide();
                        #endif
                    }
                    else if ( milton->current_mode == MiltonMode::HISTORY ) {
                        cursor_set_and_show(platform.cursor_default);
                    }
                    else if ( milton->current_mode == MiltonMode::DRAG_BRUSH_SIZE ) {
                        platform_cursor_hide();
                    }
                    else if ( milton->current_mode != MiltonMode::PEN || milton->current_mode != MiltonMode::ERASER ) {
                        platform_cursor_hide();
                    }
                }
        }
        // NOTE:
        //  Previous Milton versions had a hack where SDL was modified to call
        //  milton_osx_tablet_hook, where it would fill up some arrays.
        //  Here we would call milton_osx_poll_pressures to access those arrays.
        //
        //  OSX support is currently in limbo. Those two functions still exist
        //  but are not called anywhere.
        //    -Sergio 2018/07/08

        i32 input_flags = (i32)milton_input.flags;

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Avoid the case where we stop changing the brush size when we hover over GUI elements.
        if ( milton->current_mode == MiltonMode::DRAG_BRUSH_SIZE ) {
            ImGui::GetIO().WantCaptureMouse = false;
        }

        // Clear our pointer input because we captured an ImGui widget!
        if ( ImGui::GetIO().WantCaptureMouse ) {
            platform.num_point_results = 0;
            platform.is_pointer_down = false;
            input_flags |= MiltonInputFlags_IMGUI_GRABBED_INPUT;
        }

        milton_imgui_tick(&milton_input, &platform, milton, &prefs);

        // Clear pan delta if we are zooming
        if ( milton_input.scale != 0 ) {
            milton_input.pan_delta = {};
        }
        else if ( platform.is_panning ) {
            input_flags |= MiltonInputFlags_PANNING;
            platform.num_point_results = 0;
        }
        else if ( platform.was_panning ) {
            // Just finished panning. Refresh the screen.
            input_flags |= MiltonInputFlags_FULL_REFRESH;
        }

        if ( platform.num_pressure_results < platform.num_point_results ) {
            platform.num_point_results = platform.num_pressure_results;
        }

        milton_input.flags = (MiltonInputFlags)( input_flags | (int)milton_input.flags );

        mlt_assert (platform.num_point_results <= platform.num_pressure_results);

        milton_input.input_count = platform.num_point_results;

        v2l pan_delta = platform.pan_point - platform.pan_start;
        if (    pan_delta.x != 0
             || pan_delta.y != 0
             || platform.width != milton->view->screen_size.x
             || platform.height != milton->view->screen_size.y ) {
            milton_resize_and_pan(milton, pan_delta, {platform.width, platform.height});
        }
        milton_input.pan_delta = pan_delta;

        // Reset pan_start. Delta is not cumulative.
        platform.pan_start = platform.pan_point;

        // ==== Update and render
        PROFILE_GRAPH_END(polling);
        PROFILE_GRAPH_BEGIN(GL);
        milton_update_and_render(milton, &milton_input);
        if ( !(milton->flags & MiltonStateFlags_RUNNING) ) {
            platform.should_quit = true;
        }
        
        // Render ImGui on top
        {
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            ImGui::Render();
            
            // ImGui will be rendered to the command buffer in gpu_render
            // The actual Vulkan present happens in gpu_render's command submission
        }
        PROFILE_GRAPH_END(GL);
        PROFILE_GRAPH_BEGIN(system);
        platform_event_tick();

        // Sleep if the frame took less time than the refresh rate.
        u64 frame_time_us = perf_counter() - frame_start_us;

        f32 expected_us = (f32)1000000 / display_hz;
        if ( frame_time_us < expected_us ) {
            f32 to_sleep_us = expected_us - frame_time_us;
            //  milton_log("Sleeping at least %d ms\n", (u32)(to_sleep_us/1000));
            SDL_Delay((u32)(to_sleep_us/1000));
        }
        #if REDRAW_EVERY_FRAME
        platform.force_next_frame = true;
        #endif
        // IMGUI events might update until the frame after they are created.
        if ( !platform.force_next_frame ) {
            SDL_WaitEvent(NULL);
        }
        else {
            platform.force_next_frame = false;
        }
    }

    platform_deinit(&platform);

    arena_free(&milton->root_arena);

    // Save preferences.
    v2l size =  { platform.width,platform.height };
    platform_pixel_to_point(&platform, &size);

    prefs.width  = size.w;
    prefs.height = size.h;
    platform_settings_save(&prefs);

    SDL_Quit();

    return 0;
}
