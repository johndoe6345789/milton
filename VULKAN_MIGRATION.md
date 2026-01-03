# OpenGL to Vulkan Migration Guide

## Overview

Milton now boots with SDL3 + Vulkan and uses a stub renderer that clears the swapchain and renders ImGui. Core rendering features (strokes, layers, export, picker) are not yet ported.

## Completed

- Build + dependencies: Vulkan, Volk, and glslang wired in via [conanfile.txt](conanfile.txt) and [CMakeLists.txt](CMakeLists.txt).
- Vulkan core: [src/vk.h](src/vk.h) + [src/vk_helpers.cc](src/vk_helpers.cc) handle instance/device/swapchain/render pass/pools/sync.
- SDL3 + ImGui: [src/sdl_milton.cc](src/sdl_milton.cc) initializes Vulkan and uses the ImGui Vulkan backend.
- Renderer stub: [src/renderer_vk_stub.cc](src/renderer_vk_stub.cc) clears the screen and renders ImGui.

## Remaining Work (In Order)

1. **Renderer rewrite (critical)**  
   Replace OpenGL rendering in [src/renderer.cc](src/renderer.cc) and [src/milton.cc](src/milton.cc) with Vulkan pipelines, descriptor sets, buffers, and command buffer recording. Port stroke rendering, layer blending, picker, and export path.

2. **Shader conversion (critical)**  
   Shaders are stored as SPIR-V assembly (`.spvasm`) and embedded via [generate_shaders.pl](generate_shaders.pl). Ensure pipeline layouts and bindings match the SPIR-V expectations. Key files: [src/picker.v.spvasm](src/picker.v.spvasm), [src/picker.f.spvasm](src/picker.f.spvasm), [src/layer_blend.v.spvasm](src/layer_blend.v.spvasm), [src/layer_blend.f.spvasm](src/layer_blend.f.spvasm), [src/stroke_raster.v.spvasm](src/stroke_raster.v.spvasm), [src/stroke_raster.f.spvasm](src/stroke_raster.f.spvasm), [src/outline.v.spvasm](src/outline.v.spvasm), [src/outline.f.spvasm](src/outline.f.spvasm), [src/quad.v.spvasm](src/quad.v.spvasm), [src/quad.f.spvasm](src/quad.f.spvasm), [src/postproc.f.spvasm](src/postproc.f.spvasm), [src/blur.f.spvasm](src/blur.f.spvasm), [src/exporter_rect.f.spvasm](src/exporter_rect.f.spvasm), [src/texture_fill.v.spvasm](src/texture_fill.v.spvasm), [src/texture_fill.f.spvasm](src/texture_fill.f.spvasm).

3. **Shader build system**  
   [generate_shaders.pl](generate_shaders.pl) assembles `.spvasm` and embeds bytecode into [src/shaders.gen.h](src/shaders.gen.h).

4. **Swapchain recreation**  
   Handle `VK_ERROR_OUT_OF_DATE_KHR` and resize paths; rebuild swapchain-dependent framebuffers/render targets.

5. **Platform cleanup**  
   Remove OpenGL loaders/config in platform files ([src/platform_windows.cc](src/platform_windows.cc), [src/platform_linux.cc](src/platform_linux.cc), [src/platform_mac.mm](src/platform_mac.mm), [src/platform_unix.cc](src/platform_unix.cc)) once Vulkan paths are complete. Delete obsolete OpenGL headers/helpers after references are gone: [src/gl.h](src/gl.h), [src/gl_helpers.cc](src/gl_helpers.cc), [src/gl_enums.inl](src/gl_enums.inl), [src/gl_functions.inl](src/gl_functions.inl).

6. **Validation + testing**  
   Verify feature parity and performance across GPUs (AMD/NVIDIA/Intel) and platforms (Linux/Windows/macOS).

## Renderer Entry Points (Current Data Flow)

- **Initialization:** `milton_init()` allocates the backend (`gpu_allocate_render_backend`), then calls `gpu_init`, followed by `gpu_update_background`. See `src/milton.cc`.
- **Per-frame render:** `milton_update()` computes clip bounds, calls `gpu_clip_strokes_and_update`, then `gpu_render`. See `src/milton.cc`.
- **Resize / view changes:** `upload_gui()` and `milton_resize_and_pan()` call `gpu_update_canvas`, `gpu_resize`, and `gpu_update_picker`. See `src/milton.cc`.
- **Brush/picker updates:** `gpu_update_brush_outline` runs for hover/drag, and `gpu_update_picker` runs on picker updates. See `src/milton.cc`.
- **Export/eyedropper:** `gpu_render_to_buffer` is used to read back pixels for the eyedropper and export path. See `src/milton.cc`.
- **Backend interface:** All GPU entry points are declared in `src/renderer.h` and currently stubbed in `src/renderer_vk_stub.cc`.

## Vulkan Resource Model (Target)

- **Per-frame:** swapchain framebuffers, command buffers, fences/semaphores (already in `vk::Context`).
- **Per-view (CanvasView):** a uniform buffer with `u_rotation`, `u_pan_center`, `u_zoom_center`, `u_screen_size`, `u_scale`; updated on resize/zoom/pan.
- **Per-stroke:** a GPU allocation keyed by `Stroke::render_handle` that holds vertex data for segments (attributes like `a_position`, `a_pointa`, `a_pointb`, optional debug color) plus metadata (flags, radius, brush color). Use push constants or a small UBO for brush parameters.
- **Per-layer:** an offscreen color image (RGBA8) per visible layer, with view + sampler. Additional temporary images for effects (blur, postproc) and blending.
- **Descriptors:**  
  - Set 0: per-view UBO.  
  - Set 1: per-pass textures (layer texture, canvas for eraser, blur inputs).  
  - Push constants: per-draw brush data (`u_brush_color`, `u_radius`, flags).
- **Pipelines:**  
  - Stroke pipelines (pen/eraser/primitive/debug).  
  - Layer blend pipeline (composite layer textures).  
  - Picker/GUI pipelines (ImGui handled separately).  
  - Postprocess/blur pipelines for layer effects.
- **Readback:** a staging buffer for `gpu_render_to_buffer` (eyedropper/export).
- **Legacy struct cleanup:** `MiltonGLState` in `src/milton.h` should be removed or replaced with Vulkan equivalents once renderer owns all resources.

## Shader Bindings (Current SPIR-V)

- **Set 0 / Binding 0:** `ViewUBO` (rotation, pan/zoom, screen size, scale, radius).
- **Set 0 / Binding 1:** `BrushUBO` (brush color, opacity min, hardness).
- **Set 1 / Binding 0:** primary sampler (`u_canvas` / `u_info`).
- **Set 1 / Binding 1:** per-pass parameters (`FillParams`, `BlurParams`, `OutlineParams`, `PickerParams`).
