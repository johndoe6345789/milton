// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// Vulkan renderer - minimal implementation to get Milton running
// Full rendering features to be implemented incrementally

#include "shaders.gen.h"

#include "color.h"
#include "vk.h"
#include "gui.h"
#include "milton.h"
#include "vector.h"
#include "utils.h"
#include "DArray.h"

#include <cmath>
#include <cstring>

#define MAX_DEPTH_VALUE (1<<20)
#define RENDER_CHUNK_SIZE_LOG2 28

enum RenderElementFlags
{
    RenderElementFlags_NONE = 0,
    RenderElementFlags_LAYER                = 1<<0,
    RenderElementFlags_PRESSURE_TO_OPACITY  = 1<<1,
    RenderElementFlags_DISTANCE_TO_OPACITY  = 1<<2,
    RenderElementFlags_ERASER               = 1<<3,
};

struct ViewUBO
{
    float rotation[4];
    float rotation_inverse[4];
    int32_t pan_center[2];
    int32_t zoom_center[2];
    float screen_size[2];
    int32_t scale;
    int32_t radius;
};

struct BrushUBO
{
    float color[4];
    float opacity_min;
    float hardness;
    float pad[2];
};

struct RenderElement
{
    vk::Buffer vbo_stroke;
    vk::Buffer vbo_pointa;
    vk::Buffer vbo_pointb;
    vk::Buffer indices;

    uint32_t index_count;

    v4f color;
    i32 radius;
    f32 min_opacity;
    f32 hardness;

    int flags;
};

// Vulkan render backend - replaces OpenGL backend
struct RenderBackend
{
    VkCommandBuffer command_buffers[8];  // One per swapchain image
    VkFramebuffer framebuffers[8];

    VkPipeline quad_pipeline;
    VkPipelineLayout quad_pipeline_layout;
    vk::Buffer quad_vertex_buffer;

    VkPipeline blend_pipeline;
    VkPipelineLayout blend_pipeline_layout;
    VkDescriptorSetLayout blend_set_layout;
    VkDescriptorSet blend_set;
    VkSampler blend_sampler;
    vk::Image canvas_image;
    VkFramebuffer canvas_framebuffer;
    VkRenderPass canvas_render_pass;
    VkImageLayout canvas_layout;
    vk::Buffer readback_buffer;
    VkDeviceSize readback_size;

    VkPipeline stroke_pipeline;
    VkPipelineLayout stroke_pipeline_layout;
    VkDescriptorSetLayout stroke_set_layout;
    VkDescriptorSet stroke_set;
    vk::Buffer view_ubo;
    vk::Buffer brush_ubo;
    ViewUBO view_ubo_data;
    BrushUBO brush_ubo_data;

    VkPipeline picker_pipeline;
    VkPipelineLayout picker_pipeline_layout;
    VkDescriptorSetLayout picker_set_layout;
    VkDescriptorSet picker_set;
    vk::Buffer picker_ubo;
    vk::Buffer picker_vertex_buffer;
    vk::Buffer picker_norm_buffer;
    PickerUBO picker_ubo_data;

    VkPipeline outline_pipeline;
    VkPipelineLayout outline_pipeline_layout;
    VkDescriptorSetLayout outline_set_layout;
    VkDescriptorSet outline_set;
    vk::Buffer outline_ubo;
    vk::Buffer outline_vertex_buffer;
    vk::Buffer outline_sizes_buffer;

    DArray<RenderElement*> clip_array;
    
    v2i render_center;
    f32 viewport_limits[2];
    
    // Canvas state (stub)
    v3f background_color;
    i32 scale;
    i32 stroke_z;
    i32 width;
    i32 height;
    
    // Flags
    int flags;
    
    b32 initialized;
};

struct QuadVertex
{
    float x;
    float y;
};

struct StrokeVertex
{
    float x;
    float y;
    float z;
};

struct PickerUBO
{
    float pointa[2];
    float pointb[2];
    float pointc[2];
    float triangle_point[2];
    float screen_size[2];
    float pad0[2];
    float color[4];
    float angle;
    float pad1[3];
    float colors[5][4];
};

struct OutlineUBO
{
    int32_t radius;
    int32_t fill;
    float pad0[2];
    float color[4];
};

static bool create_fullscreen_quad(RenderBackend* renderer)
{
    QuadVertex vertices[] = {
        {-1.0f, -1.0f},
        { 1.0f, -1.0f},
        {-1.0f,  1.0f},
        { 1.0f,  1.0f},
    };

    if (!vk::create_buffer(sizeof(vertices),
                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           &renderer->quad_vertex_buffer)) {
        return false;
    }

    void* mapped = nullptr;
    vkMapMemory(vk::g_vk_context.device, renderer->quad_vertex_buffer.memory, 0,
                renderer->quad_vertex_buffer.size, 0, &mapped);
    std::memcpy(mapped, vertices, sizeof(vertices));
    vkUnmapMemory(vk::g_vk_context.device, renderer->quad_vertex_buffer.memory);

    return true;
}

static bool create_stroke_pipeline(RenderBackend* renderer)
{
    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 2;
    layout_info.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(vk::g_vk_context.device, &layout_info, nullptr,
                                    &renderer->stroke_set_layout) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = vk::g_vk_context.descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &renderer->stroke_set_layout;

    if (vkAllocateDescriptorSets(vk::g_vk_context.device, &alloc_info, &renderer->stroke_set) != VK_SUCCESS) {
        return false;
    }

    if (!vk::create_buffer(sizeof(ViewUBO),
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           &renderer->view_ubo)) {
        return false;
    }

    if (!vk::create_buffer(sizeof(BrushUBO),
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           &renderer->brush_ubo)) {
        return false;
    }

    VkDescriptorBufferInfo view_info = {};
    view_info.buffer = renderer->view_ubo.buffer;
    view_info.offset = 0;
    view_info.range = sizeof(ViewUBO);

    VkDescriptorBufferInfo brush_info = {};
    brush_info.buffer = renderer->brush_ubo.buffer;
    brush_info.offset = 0;
    brush_info.range = sizeof(BrushUBO);

    VkWriteDescriptorSet writes[2] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = renderer->stroke_set;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &view_info;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = renderer->stroke_set;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &brush_info;

    vkUpdateDescriptorSets(vk::g_vk_context.device, 2, writes, 0, nullptr);

    VkShaderModule vert_module = vk::create_shader_module(g_stroke_raster_v_spv, g_stroke_raster_v_spv_size);
    VkShaderModule frag_module = vk::create_shader_module(g_stroke_raster_f_spv, g_stroke_raster_f_spv_size);
    if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
        return false;
    }

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vk::shader_stage_info(vert_module, VK_SHADER_STAGE_VERTEX_BIT),
        vk::shader_stage_info(frag_module, VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    VkVertexInputBindingDescription bindings_desc[3] = {};
    bindings_desc[0].binding = 0;
    bindings_desc[0].stride = sizeof(StrokeVertex);
    bindings_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindings_desc[1].binding = 1;
    bindings_desc[1].stride = sizeof(StrokeVertex);
    bindings_desc[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindings_desc[2].binding = 2;
    bindings_desc[2].stride = sizeof(StrokeVertex);
    bindings_desc[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[3] = {};
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = 0;
    attrs[1].location = 1;
    attrs[1].binding = 1;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = 0;
    attrs[2].location = 2;
    attrs[2].binding = 2;
    attrs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[2].offset = 0;

    VkPipelineVertexInputStateCreateInfo vertex_input = {};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 3;
    vertex_input.pVertexBindingDescriptions = bindings_desc;
    vertex_input.vertexAttributeDescriptionCount = 3;
    vertex_input.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                            VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT |
                                            VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blend = {};
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &color_blend_attachment;

    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    renderer->stroke_pipeline_layout = vk::create_pipeline_layout(&renderer->stroke_set_layout, 1, nullptr, 0);
    if (renderer->stroke_pipeline_layout == VK_NULL_HANDLE) {
        vkDestroyShaderModule(vk::g_vk_context.device, vert_module, nullptr);
        vkDestroyShaderModule(vk::g_vk_context.device, frag_module, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blend;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = renderer->stroke_pipeline_layout;
    pipeline_info.renderPass = vk::g_vk_context.render_pass;
    pipeline_info.subpass = 0;

    renderer->stroke_pipeline = vk::create_graphics_pipeline(&pipeline_info);

    vkDestroyShaderModule(vk::g_vk_context.device, vert_module, nullptr);
    vkDestroyShaderModule(vk::g_vk_context.device, frag_module, nullptr);

    return renderer->stroke_pipeline != VK_NULL_HANDLE;
}

static v2i relative_to_render_center(RenderBackend* renderer, v2l point)
{
    v2i result = VEC2I(point - VEC2L(renderer->render_center * (1 << RENDER_CHUNK_SIZE_LOG2)));
    return result;
}

static void upload_view_ubo(RenderBackend* renderer)
{
    void* mapped = nullptr;
    vkMapMemory(vk::g_vk_context.device, renderer->view_ubo.memory, 0,
                renderer->view_ubo.size, 0, &mapped);
    std::memcpy(mapped, &renderer->view_ubo_data, sizeof(renderer->view_ubo_data));
    vkUnmapMemory(vk::g_vk_context.device, renderer->view_ubo.memory);
}

static void upload_brush_ubo(RenderBackend* renderer)
{
    void* mapped = nullptr;
    vkMapMemory(vk::g_vk_context.device, renderer->brush_ubo.memory, 0,
                renderer->brush_ubo.size, 0, &mapped);
    std::memcpy(mapped, &renderer->brush_ubo_data, sizeof(renderer->brush_ubo_data));
    vkUnmapMemory(vk::g_vk_context.device, renderer->brush_ubo.memory);
}

static void upload_buffer(vk::Buffer* buffer, const void* data, size_t size)
{
    void* mapped = nullptr;
    vkMapMemory(vk::g_vk_context.device, buffer->memory, 0, buffer->size, 0, &mapped);
    std::memcpy(mapped, data, size);
    vkUnmapMemory(vk::g_vk_context.device, buffer->memory);
}

static bool create_or_resize_buffer(vk::Buffer* buffer, VkDeviceSize size, VkBufferUsageFlags usage)
{
    if (buffer->buffer != VK_NULL_HANDLE && buffer->size >= size) {
        return true;
    }
    vk::destroy_buffer(buffer);
    return vk::create_buffer(size, usage,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             buffer);
}
static bool create_simple_pipeline(RenderBackend* renderer)
{
    VkShaderModule vert_module = vk::create_shader_module(g_simple_v_spv, g_simple_v_spv_size);
    VkShaderModule frag_module = vk::create_shader_module(g_simple_f_spv, g_simple_f_spv_size);
    if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
        return false;
    }

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vk::shader_stage_info(vert_module, VK_SHADER_STAGE_VERTEX_BIT),
        vk::shader_stage_info(frag_module, VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    VkVertexInputBindingDescription binding = {};
    binding.binding = 0;
    binding.stride = sizeof(QuadVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attribute = {};
    attribute.location = 0;
    attribute.binding = 0;
    attribute.format = VK_FORMAT_R32G32_SFLOAT;
    attribute.offset = 0;

    VkPipelineVertexInputStateCreateInfo vertex_input = {};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = 1;
    vertex_input.pVertexAttributeDescriptions = &attribute;

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                            VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT |
                                            VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blend = {};
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &color_blend_attachment;

    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    renderer->quad_pipeline_layout = vk::create_pipeline_layout(nullptr, 0, nullptr, 0);
    if (renderer->quad_pipeline_layout == VK_NULL_HANDLE) {
        vkDestroyShaderModule(vk::g_vk_context.device, vert_module, nullptr);
        vkDestroyShaderModule(vk::g_vk_context.device, frag_module, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blend;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = renderer->quad_pipeline_layout;
    pipeline_info.renderPass = vk::g_vk_context.render_pass;
    pipeline_info.subpass = 0;

    renderer->quad_pipeline = vk::create_graphics_pipeline(&pipeline_info);

    vkDestroyShaderModule(vk::g_vk_context.device, vert_module, nullptr);
    vkDestroyShaderModule(vk::g_vk_context.device, frag_module, nullptr);

    return renderer->quad_pipeline != VK_NULL_HANDLE;
}

static bool create_canvas_target(RenderBackend* renderer)
{
    vk::destroy_image(&renderer->canvas_image);
    if (renderer->canvas_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(vk::g_vk_context.device, renderer->canvas_framebuffer, nullptr);
        renderer->canvas_framebuffer = VK_NULL_HANDLE;
    }
    if (renderer->canvas_render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(vk::g_vk_context.device, renderer->canvas_render_pass, nullptr);
        renderer->canvas_render_pass = VK_NULL_HANDLE;
    }

    if (!vk::create_image(vk::g_vk_context.swapchain_extent.width,
                          vk::g_vk_context.swapchain_extent.height,
                          vk::g_vk_context.swapchain_format,
                          VK_IMAGE_TILING_OPTIMAL,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                          &renderer->canvas_image)) {
        return false;
    }

    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = renderer->canvas_image.image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = vk::g_vk_context.swapchain_format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    if (vkCreateImageView(vk::g_vk_context.device, &view_info, nullptr, &renderer->canvas_image.view) != VK_SUCCESS) {
        return false;
    }

    VkAttachmentDescription color_attachment = {};
    color_attachment.format = vk::g_vk_context.swapchain_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference color_ref = {};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if (vkCreateRenderPass(vk::g_vk_context.device, &render_pass_info, nullptr,
                           &renderer->canvas_render_pass) != VK_SUCCESS) {
        return false;
    }

    VkFramebufferCreateInfo framebuffer_info = {};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = renderer->canvas_render_pass;
    framebuffer_info.attachmentCount = 1;
    framebuffer_info.pAttachments = &renderer->canvas_image.view;
    framebuffer_info.width = vk::g_vk_context.swapchain_extent.width;
    framebuffer_info.height = vk::g_vk_context.swapchain_extent.height;
    framebuffer_info.layers = 1;

    if (vkCreateFramebuffer(vk::g_vk_context.device, &framebuffer_info, nullptr,
                            &renderer->canvas_framebuffer) != VK_SUCCESS) {
        return false;
    }

    renderer->canvas_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    return true;
}

static bool ensure_readback_buffer(RenderBackend* renderer, VkDeviceSize size)
{
    if (renderer->readback_buffer.buffer != VK_NULL_HANDLE && renderer->readback_size >= size) {
        return true;
    }
    vk::destroy_buffer(&renderer->readback_buffer);
    renderer->readback_size = size;
    return vk::create_buffer(size,
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             &renderer->readback_buffer);
}

static bool create_blend_pipeline(RenderBackend* renderer)
{
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &binding;

    if (vkCreateDescriptorSetLayout(vk::g_vk_context.device, &layout_info, nullptr,
                                    &renderer->blend_set_layout) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = vk::g_vk_context.descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &renderer->blend_set_layout;

    if (vkAllocateDescriptorSets(vk::g_vk_context.device, &alloc_info, &renderer->blend_set) != VK_SUCCESS) {
        return false;
    }

    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(vk::g_vk_context.device, &sampler_info, nullptr, &renderer->blend_sampler) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorImageInfo image_info = {};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = renderer->canvas_image.view;
    image_info.sampler = renderer->blend_sampler;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = renderer->blend_set;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &image_info;
    vkUpdateDescriptorSets(vk::g_vk_context.device, 1, &write, 0, nullptr);

    VkShaderModule vert_module = vk::create_shader_module(g_layer_blend_v_spv, g_layer_blend_v_spv_size);
    VkShaderModule frag_module = vk::create_shader_module(g_layer_blend_f_spv, g_layer_blend_f_spv_size);
    if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
        return false;
    }

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vk::shader_stage_info(vert_module, VK_SHADER_STAGE_VERTEX_BIT),
        vk::shader_stage_info(frag_module, VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    VkVertexInputBindingDescription binding_desc = {};
    binding_desc.binding = 0;
    binding_desc.stride = sizeof(QuadVertex);
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attribute = {};
    attribute.location = 0;
    attribute.binding = 0;
    attribute.format = VK_FORMAT_R32G32_SFLOAT;
    attribute.offset = 0;

    VkPipelineVertexInputStateCreateInfo vertex_input = {};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding_desc;
    vertex_input.vertexAttributeDescriptionCount = 1;
    vertex_input.pVertexAttributeDescriptions = &attribute;

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                            VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT |
                                            VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blend = {};
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &color_blend_attachment;

    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    VkDescriptorSetLayout layouts[] = {renderer->stroke_set_layout, renderer->blend_set_layout};
    renderer->blend_pipeline_layout = vk::create_pipeline_layout(layouts, 2, nullptr, 0);
    if (renderer->blend_pipeline_layout == VK_NULL_HANDLE) {
        vkDestroyShaderModule(vk::g_vk_context.device, vert_module, nullptr);
        vkDestroyShaderModule(vk::g_vk_context.device, frag_module, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blend;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = renderer->blend_pipeline_layout;
    pipeline_info.renderPass = vk::g_vk_context.render_pass;
    pipeline_info.subpass = 0;

    renderer->blend_pipeline = vk::create_graphics_pipeline(&pipeline_info);

    vkDestroyShaderModule(vk::g_vk_context.device, vert_module, nullptr);
    vkDestroyShaderModule(vk::g_vk_context.device, frag_module, nullptr);

    return renderer->blend_pipeline != VK_NULL_HANDLE;
}

static bool create_picker_pipeline(RenderBackend* renderer)
{
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 1;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &binding;

    if (vkCreateDescriptorSetLayout(vk::g_vk_context.device, &layout_info, nullptr,
                                    &renderer->picker_set_layout) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = vk::g_vk_context.descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &renderer->picker_set_layout;

    if (vkAllocateDescriptorSets(vk::g_vk_context.device, &alloc_info, &renderer->picker_set) != VK_SUCCESS) {
        return false;
    }

    if (!vk::create_buffer(sizeof(PickerUBO),
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           &renderer->picker_ubo)) {
        return false;
    }

    VkDescriptorBufferInfo ubo_info = {};
    ubo_info.buffer = renderer->picker_ubo.buffer;
    ubo_info.offset = 0;
    ubo_info.range = sizeof(PickerUBO);

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = renderer->picker_set;
    write.dstBinding = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &ubo_info;
    vkUpdateDescriptorSets(vk::g_vk_context.device, 1, &write, 0, nullptr);

    VkShaderModule vert_module = vk::create_shader_module(g_picker_v_spv, g_picker_v_spv_size);
    VkShaderModule frag_module = vk::create_shader_module(g_picker_f_spv, g_picker_f_spv_size);
    if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
        return false;
    }

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vk::shader_stage_info(vert_module, VK_SHADER_STAGE_VERTEX_BIT),
        vk::shader_stage_info(frag_module, VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    VkVertexInputBindingDescription bindings_desc[2] = {};
    bindings_desc[0].binding = 0;
    bindings_desc[0].stride = sizeof(QuadVertex);
    bindings_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindings_desc[1].binding = 1;
    bindings_desc[1].stride = sizeof(QuadVertex);
    bindings_desc[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[2] = {};
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset = 0;
    attrs[1].location = 1;
    attrs[1].binding = 1;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = 0;

    VkPipelineVertexInputStateCreateInfo vertex_input = {};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 2;
    vertex_input.pVertexBindingDescriptions = bindings_desc;
    vertex_input.vertexAttributeDescriptionCount = 2;
    vertex_input.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                            VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT |
                                            VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blend = {};
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &color_blend_attachment;

    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    VkDescriptorSetLayout layouts[] = {renderer->stroke_set_layout, renderer->picker_set_layout};
    renderer->picker_pipeline_layout = vk::create_pipeline_layout(layouts, 2, nullptr, 0);
    if (renderer->picker_pipeline_layout == VK_NULL_HANDLE) {
        vkDestroyShaderModule(vk::g_vk_context.device, vert_module, nullptr);
        vkDestroyShaderModule(vk::g_vk_context.device, frag_module, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blend;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = renderer->picker_pipeline_layout;
    pipeline_info.renderPass = vk::g_vk_context.render_pass;
    pipeline_info.subpass = 0;

    renderer->picker_pipeline = vk::create_graphics_pipeline(&pipeline_info);

    vkDestroyShaderModule(vk::g_vk_context.device, vert_module, nullptr);
    vkDestroyShaderModule(vk::g_vk_context.device, frag_module, nullptr);

    return renderer->picker_pipeline != VK_NULL_HANDLE;
}

static bool create_outline_pipeline(RenderBackend* renderer)
{
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 1;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &binding;

    if (vkCreateDescriptorSetLayout(vk::g_vk_context.device, &layout_info, nullptr,
                                    &renderer->outline_set_layout) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = vk::g_vk_context.descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &renderer->outline_set_layout;

    if (vkAllocateDescriptorSets(vk::g_vk_context.device, &alloc_info, &renderer->outline_set) != VK_SUCCESS) {
        return false;
    }

    if (!vk::create_buffer(sizeof(OutlineUBO),
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           &renderer->outline_ubo)) {
        return false;
    }

    VkDescriptorBufferInfo ubo_info = {};
    ubo_info.buffer = renderer->outline_ubo.buffer;
    ubo_info.offset = 0;
    ubo_info.range = sizeof(OutlineUBO);

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = renderer->outline_set;
    write.dstBinding = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &ubo_info;
    vkUpdateDescriptorSets(vk::g_vk_context.device, 1, &write, 0, nullptr);

    VkShaderModule vert_module = vk::create_shader_module(g_outline_v_spv, g_outline_v_spv_size);
    VkShaderModule frag_module = vk::create_shader_module(g_outline_f_spv, g_outline_f_spv_size);
    if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
        return false;
    }

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vk::shader_stage_info(vert_module, VK_SHADER_STAGE_VERTEX_BIT),
        vk::shader_stage_info(frag_module, VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    VkVertexInputBindingDescription bindings_desc[2] = {};
    bindings_desc[0].binding = 0;
    bindings_desc[0].stride = sizeof(QuadVertex);
    bindings_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindings_desc[1].binding = 1;
    bindings_desc[1].stride = sizeof(QuadVertex);
    bindings_desc[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[2] = {};
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset = 0;
    attrs[1].location = 1;
    attrs[1].binding = 1;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = 0;

    VkPipelineVertexInputStateCreateInfo vertex_input = {};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 2;
    vertex_input.pVertexBindingDescriptions = bindings_desc;
    vertex_input.vertexAttributeDescriptionCount = 2;
    vertex_input.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                            VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT |
                                            VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blend = {};
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &color_blend_attachment;

    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    VkDescriptorSetLayout layouts[] = {renderer->stroke_set_layout, renderer->outline_set_layout};
    renderer->outline_pipeline_layout = vk::create_pipeline_layout(layouts, 2, nullptr, 0);
    if (renderer->outline_pipeline_layout == VK_NULL_HANDLE) {
        vkDestroyShaderModule(vk::g_vk_context.device, vert_module, nullptr);
        vkDestroyShaderModule(vk::g_vk_context.device, frag_module, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blend;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = renderer->outline_pipeline_layout;
    pipeline_info.renderPass = vk::g_vk_context.render_pass;
    pipeline_info.subpass = 0;

    renderer->outline_pipeline = vk::create_graphics_pipeline(&pipeline_info);

    vkDestroyShaderModule(vk::g_vk_context.device, vert_module, nullptr);
    vkDestroyShaderModule(vk::g_vk_context.device, frag_module, nullptr);

    return renderer->outline_pipeline != VK_NULL_HANDLE;
}
RenderBackend* gpu_allocate_render_backend(Arena* arena)
{
    RenderBackend* r = arena_alloc_elem(arena, RenderBackend);
    return r;
}

b32 gpu_init(RenderBackend* renderer, CanvasView* view, ColorPicker* picker)
{
    if (!vk::g_vk_context.device) {
        milton_log("ERROR: Vulkan not initialized\n");
        return false;
    }
    
    // Get viewport limits from Vulkan
    renderer->viewport_limits[0] = (f32)vk::g_vk_context.device_properties.limits.maxImageDimension2D;
    renderer->viewport_limits[1] = (f32)vk::g_vk_context.device_properties.limits.maxImageDimension2D;
    
    // Create framebuffers for each swapchain image
    for (uint32_t i = 0; i < vk::g_vk_context.swapchain_image_count; i++) {
        VkFramebufferCreateInfo framebuffer_info = {};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = vk::g_vk_context.render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = &vk::g_vk_context.swapchain_image_views[i];
        framebuffer_info.width = vk::g_vk_context.swapchain_extent.width;
        framebuffer_info.height = vk::g_vk_context.swapchain_extent.height;
        framebuffer_info.layers = 1;
        
        if (vkCreateFramebuffer(vk::g_vk_context.device, &framebuffer_info, nullptr, 
                               &renderer->framebuffers[i]) != VK_SUCCESS) {
            milton_log("Failed to create framebuffer\n");
            return false;
        }
    }
    
    // Allocate command buffers
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = vk::g_vk_context.command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = vk::g_vk_context.swapchain_image_count;
    
    if (vkAllocateCommandBuffers(vk::g_vk_context.device, &alloc_info, 
                                renderer->command_buffers) != VK_SUCCESS) {
        milton_log("Failed to allocate command buffers\n");
        return false;
    }

    if (!create_fullscreen_quad(renderer)) {
        milton_log("Failed to create fullscreen quad buffer\n");
        return false;
    }

    if (!create_simple_pipeline(renderer)) {
        milton_log("Failed to create simple pipeline\n");
        return false;
    }

    if (!create_stroke_pipeline(renderer)) {
        milton_log("Failed to create stroke pipeline\n");
        return false;
    }

    if (!create_canvas_target(renderer)) {
        milton_log("Failed to create canvas target\n");
        return false;
    }

    if (!create_blend_pipeline(renderer)) {
        milton_log("Failed to create blend pipeline\n");
        return false;
    }

    if (!create_picker_pipeline(renderer)) {
        milton_log("Failed to create picker pipeline\n");
        return false;
    }

    if (!create_outline_pipeline(renderer)) {
        milton_log("Failed to create outline pipeline\n");
        return false;
    }

    renderer->clip_array = dynamic_array<RenderElement*>(64);
    
    renderer->background_color = v3f{1.0f, 1.0f, 1.0f};
    renderer->scale = MILTON_DEFAULT_SCALE;
    renderer->stroke_z = MAX_DEPTH_VALUE - 20;
    renderer->width = vk::g_vk_context.swapchain_extent.width;
    renderer->height = vk::g_vk_context.swapchain_extent.height;
    renderer->view_ubo_data.screen_size[0] = (float)renderer->width;
    renderer->view_ubo_data.screen_size[1] = (float)renderer->height;
    upload_view_ubo(renderer);
    renderer->initialized = true;
    
    milton_log("Vulkan renderer initialized\n");
    return true;
}

void imm_begin_frame(RenderBackend* renderer)
{
    // Stub
}

void imm_rect(RenderBackend* renderer, float left, float right, float top, float bottom, float line_width)
{
    // Stub - immediate mode rectangle drawing
}

void gpu_update_brush_outline(RenderBackend* renderer, i32 cx, i32 cy, i32 radius,
                              BrushOutlineEnum outline_enum, v4f color)
{
    float radius_plus_girth = radius + 4.0f;

    auto w = (float)renderer->width;
    auto h = (float)renderer->height;

    QuadVertex data[] = {
        {2 * ((cx - radius_plus_girth) / w) - 1, -2 * ((cy - radius_plus_girth) / h) + 1},
        {2 * ((cx - radius_plus_girth) / w) - 1, -2 * ((cy + radius_plus_girth) / h) + 1},
        {2 * ((cx + radius_plus_girth) / w) - 1, -2 * ((cy + radius_plus_girth) / h) + 1},
        {2 * ((cx + radius_plus_girth) / w) - 1, -2 * ((cy - radius_plus_girth) / h) + 1},
    };

    QuadVertex sizes[] = {
        {-radius_plus_girth, -radius_plus_girth},
        {-radius_plus_girth,  radius_plus_girth},
        { radius_plus_girth,  radius_plus_girth},
        { radius_plus_girth, -radius_plus_girth},
    };

    if (create_or_resize_buffer(&renderer->outline_vertex_buffer,
                                sizeof(data),
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)) {
        upload_buffer(&renderer->outline_vertex_buffer, data, sizeof(data));
    }
    if (create_or_resize_buffer(&renderer->outline_sizes_buffer,
                                sizeof(sizes),
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)) {
        upload_buffer(&renderer->outline_sizes_buffer, sizes, sizeof(sizes));
    }

    OutlineUBO outline = {};
    outline.radius = radius;
    outline.fill = (outline_enum == BrushOutline_FILL) ? 1 : 0;
    outline.color[0] = color.r;
    outline.color[1] = color.g;
    outline.color[2] = color.b;
    outline.color[3] = color.a;
    upload_buffer(&renderer->outline_ubo, &outline, sizeof(outline));
}

static void destroy_swapchain_framebuffers(RenderBackend* renderer)
{
    for (uint32_t i = 0; i < vk::g_vk_context.swapchain_image_count; i++) {
        if (renderer->framebuffers[i] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(vk::g_vk_context.device, renderer->framebuffers[i], nullptr);
            renderer->framebuffers[i] = VK_NULL_HANDLE;
        }
    }
}

static bool create_swapchain_framebuffers(RenderBackend* renderer)
{
    for (uint32_t i = 0; i < vk::g_vk_context.swapchain_image_count; i++) {
        VkFramebufferCreateInfo framebuffer_info = {};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = vk::g_vk_context.render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = &vk::g_vk_context.swapchain_image_views[i];
        framebuffer_info.width = vk::g_vk_context.swapchain_extent.width;
        framebuffer_info.height = vk::g_vk_context.swapchain_extent.height;
        framebuffer_info.layers = 1;

        if (vkCreateFramebuffer(vk::g_vk_context.device, &framebuffer_info, nullptr,
                               &renderer->framebuffers[i]) != VK_SUCCESS) {
            milton_log("Failed to create framebuffer\n");
            return false;
        }
    }
    return true;
}

void gpu_resize(RenderBackend* renderer, CanvasView* view)
{
    (void)view;
    destroy_swapchain_framebuffers(renderer);
    if (!vk::recreate_swapchain(vk::g_vk_context.window_handle)) {
        milton_log("Failed to recreate swapchain\n");
        return;
    }
    if (!create_swapchain_framebuffers(renderer)) {
        milton_log("Failed to recreate swapchain framebuffers\n");
        return;
    }
    if (!create_canvas_target(renderer)) {
        milton_log("Failed to recreate canvas target\n");
        return;
    }
    if (!create_blend_pipeline(renderer)) {
        milton_log("Failed to recreate blend pipeline\n");
        return;
    }
    renderer->width = vk::g_vk_context.swapchain_extent.width;
    renderer->height = vk::g_vk_context.swapchain_extent.height;
}

void gpu_update_picker(RenderBackend* renderer, ColorPicker* picker)
{
    v2f a = picker->data.a;
    v2f b = picker->data.b;
    v2f c = picker->data.c;
    Rect bounds = picker_get_bounds(picker);
    int w = bounds.right - bounds.left;
    int h = bounds.bottom - bounds.top;

    auto transform = [&](v2f p) { return v2f{2 * p.x / w - 1 - .25f, 2 * p.y / h - 1 - 0.35f}; };
    a = transform(a);
    b = transform(b);
    c = transform(c);

    renderer->picker_ubo_data.pointa[0] = a.x;
    renderer->picker_ubo_data.pointa[1] = a.y;
    renderer->picker_ubo_data.pointb[0] = b.x;
    renderer->picker_ubo_data.pointb[1] = b.y;
    renderer->picker_ubo_data.pointc[0] = c.x;
    renderer->picker_ubo_data.pointc[1] = c.y;

    v3f hsv = picker->data.hsv;
    v3f rgb = hsv_to_rgb(hsv);
    renderer->picker_ubo_data.color[0] = rgb.r;
    renderer->picker_ubo_data.color[1] = rgb.g;
    renderer->picker_ubo_data.color[2] = rgb.b;
    renderer->picker_ubo_data.color[3] = 1.0f;
    renderer->picker_ubo_data.angle = picker->data.hsv.h;

    v2f point = lerp(picker->data.b, lerp(picker->data.a, picker->data.c, hsv.s), hsv.v);
    point = transform(point);
    renderer->picker_ubo_data.triangle_point[0] = point.x;
    renderer->picker_ubo_data.triangle_point[1] = point.y;
    renderer->picker_ubo_data.screen_size[0] = (float)renderer->width;
    renderer->picker_ubo_data.screen_size[1] = (float)renderer->height;

    ColorButton* button = picker->color_buttons;
    for (int i = 0; i < 5; ++i) {
        v4f color = button ? button->rgba : v4f{};
        renderer->picker_ubo_data.colors[i][0] = color.r;
        renderer->picker_ubo_data.colors[i][1] = color.g;
        renderer->picker_ubo_data.colors[i][2] = color.b;
        renderer->picker_ubo_data.colors[i][3] = color.a;
        if (button) {
            button = button->next;
        }
    }

    upload_buffer(&renderer->picker_ubo, &renderer->picker_ubo_data, sizeof(renderer->picker_ubo_data));

    Rect rect = get_bounds_for_picker_and_colors(picker);
    v2i screen_size = {renderer->width, renderer->height};
    float top = (float)rect.top / screen_size.h;
    float bottom = (float)rect.bottom / screen_size.h;
    float left = (float)rect.left / screen_size.w;
    float right = (float)rect.right / screen_size.w;
    top = (top * 2.0f - 1.0f) * -1;
    bottom = (bottom * 2.0f - 1.0f) * -1;
    left = left * 2.0f - 1.0f;
    right = right * 2.0f - 1.0f;

    QuadVertex data[] = {
        {left, top},
        {left, bottom},
        {right, bottom},
        {right, top},
    };

    float ratio = (float)(rect.bottom - rect.top) / (float)(rect.right - rect.left);
    ratio = (ratio * 2) - 1;
    QuadVertex norm[] = {
        {-1, -1},
        {-1, ratio},
        {1, ratio},
        {1, -1},
    };

    if (create_or_resize_buffer(&renderer->picker_vertex_buffer,
                                sizeof(data),
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)) {
        upload_buffer(&renderer->picker_vertex_buffer, data, sizeof(data));
    }
    if (create_or_resize_buffer(&renderer->picker_norm_buffer,
                                sizeof(norm),
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)) {
        upload_buffer(&renderer->picker_norm_buffer, norm, sizeof(norm));
    }
}

void gpu_update_scale(RenderBackend* renderer, i32 scale)
{
    renderer->scale = scale;
    renderer->view_ubo_data.scale = scale;
    upload_view_ubo(renderer);
}

void gpu_update_export_rect(RenderBackend* renderer, Exporter* exporter)
{
    // Stub
}

void gpu_update_background(RenderBackend* renderer, v3f background_color)
{
    renderer->background_color = background_color;
}

void gpu_update_canvas(RenderBackend* renderer, CanvasState* canvas, CanvasView* view)
{
    v2i center = view->zoom_center;
    v2l pan = view->pan_center;

    v2i new_render_center = VEC2I(pan / (i64)(1 << RENDER_CHUNK_SIZE_LOG2));
    if (new_render_center != renderer->render_center) {
        renderer->render_center = new_render_center;
        gpu_free_strokes(renderer, canvas);
    }

    f32 cos_angle = cosf(view->angle);
    f32 sin_angle = sinf(view->angle);

    renderer->view_ubo_data.rotation[0] = cos_angle;
    renderer->view_ubo_data.rotation[1] = sin_angle;
    renderer->view_ubo_data.rotation[2] = -sin_angle;
    renderer->view_ubo_data.rotation[3] = cos_angle;

    renderer->view_ubo_data.rotation_inverse[0] = cos_angle;
    renderer->view_ubo_data.rotation_inverse[1] = -sin_angle;
    renderer->view_ubo_data.rotation_inverse[2] = sin_angle;
    renderer->view_ubo_data.rotation_inverse[3] = cos_angle;

    v2i pan_center = relative_to_render_center(renderer, pan);
    renderer->view_ubo_data.pan_center[0] = pan_center.x;
    renderer->view_ubo_data.pan_center[1] = pan_center.y;
    renderer->view_ubo_data.zoom_center[0] = center.x;
    renderer->view_ubo_data.zoom_center[1] = center.y;
    renderer->view_ubo_data.screen_size[0] = (float)view->screen_size.x;
    renderer->view_ubo_data.screen_size[1] = (float)view->screen_size.y;
    renderer->view_ubo_data.scale = view->scale;
    renderer->view_ubo_data.radius = 1;

    upload_view_ubo(renderer);
}

void gpu_get_viewport_limits(RenderBackend* renderer, float* out_viewport_limits)
{
    out_viewport_limits[0] = renderer->viewport_limits[0];
    out_viewport_limits[1] = renderer->viewport_limits[1];
}

i32 gpu_get_num_clipped_strokes(Layer* root_layer)
{
    return 0;  // Stub
}

void gpu_reset_stroke(RenderBackend* r, RenderHandle handle)
{
    RenderElement* re = reinterpret_cast<RenderElement*>(handle);
    if (!re) {
        return;
    }

    vk::destroy_buffer(&re->vbo_stroke);
    vk::destroy_buffer(&re->vbo_pointa);
    vk::destroy_buffer(&re->vbo_pointb);
    vk::destroy_buffer(&re->indices);
    *re = {};
}

void gpu_cook_stroke(Arena* arena, RenderBackend* renderer, Stroke* stroke, CookStrokeOpt cook_option)
{
    RenderElement** p_render_element = reinterpret_cast<RenderElement**>(&stroke->render_handle);
    RenderElement* render_element = *p_render_element;
    if (render_element == NULL) {
        render_element = arena_alloc_elem(arena, RenderElement);
        *p_render_element = render_element;
    }

    renderer->stroke_z = (renderer->stroke_z + 1) % (MAX_DEPTH_VALUE - 1);
    const i32 stroke_z = renderer->stroke_z + 1;

    if (cook_option == CookStroke_NEW && render_element->vbo_stroke.buffer != VK_NULL_HANDLE) {
        return;
    }

    auto npoints = stroke->num_points;
    if (npoints == 1) {
        Stroke duplicate = *stroke;
        duplicate.num_points = 2;
        Arena scratch_arena = arena_push(arena);
        duplicate.points = arena_alloc_array(&scratch_arena, 2, v2l);
        duplicate.pressures = arena_alloc_array(&scratch_arena, 2, f32);
        duplicate.points[0] = stroke->points[0];
        duplicate.points[1] = stroke->points[0];
        duplicate.pressures[0] = stroke->pressures[0];
        duplicate.pressures[1] = stroke->pressures[0];

        gpu_cook_stroke(&scratch_arena, renderer, &duplicate, cook_option);
        stroke->render_handle = duplicate.render_handle;
        arena_pop(&scratch_arena);
        return;
    }

    if (npoints <= 1) {
        return;
    }

    const size_t count_attribs = 4 * ((size_t)npoints - 1);
    const size_t count_indices = 6 * ((size_t)npoints - 1);

    v3f* bounds;
    v3f* apoints;
    v3f* bpoints;
    u16* indices;
    Arena scratch_arena = arena_push(arena,
                                     count_attribs * sizeof(decltype(*bounds)) +
                                     2 * count_attribs * sizeof(decltype(*apoints)) +
                                     count_indices * sizeof(decltype(*indices)));

    bounds = arena_alloc_array(&scratch_arena, count_attribs, v3f);
    apoints = arena_alloc_array(&scratch_arena, count_attribs, v3f);
    bpoints = arena_alloc_array(&scratch_arena, count_attribs, v3f);
    indices = arena_alloc_array(&scratch_arena, count_indices, u16);

    size_t bounds_i = 0;
    size_t apoints_i = 0;
    size_t bpoints_i = 0;
    size_t indices_i = 0;

    for (i64 i = 0; i < npoints - 1; ++i) {
        v2i point_i = relative_to_render_center(renderer, stroke->points[i]);
        v2i point_j = relative_to_render_center(renderer, stroke->points[i + 1]);

        Brush brush = stroke->brush;
        float radius_i = stroke->pressures[i] * brush.radius;
        float radius_j = stroke->pressures[i + 1] * brush.radius;

        u16 idx = (u16)bounds_i;
        if (point_i == point_j) {
            i32 min_x = min(point_i.x - radius_i, point_j.x - radius_j);
            i32 min_y = min(point_i.y - radius_i, point_j.y - radius_j);
            i32 max_x = max(point_i.x + radius_i, point_j.x + radius_j);
            i32 max_y = max(point_i.y + radius_i, point_j.y + radius_j);

            bounds[bounds_i++] = { (float)min_x, (float)min_y, (float)stroke_z };
            bounds[bounds_i++] = { (float)min_x, (float)max_y, (float)stroke_z };
            bounds[bounds_i++] = { (float)max_x, (float)max_y, (float)stroke_z };
            bounds[bounds_i++] = { (float)max_x, (float)min_y, (float)stroke_z };
        } else {
            v2f d = normalized(v2i_to_v2f(point_j - point_i));
            auto basis_change = [&d](v2f v) {
                v2f res = {
                    v.x * d.x + v.y * d.y,
                    v.x * d.y - v.y * d.x,
                };
                return res;
            };
            v2f a = basis_change(v2i_to_v2f(point_i));
            v2f b = basis_change(v2i_to_v2f(point_j));

            f32 rad = max(radius_i, radius_j);

            f32 min_x = min(a.x, b.x) - rad;
            f32 min_y = min(a.y, b.y) - rad;
            f32 max_x = max(a.x, b.x) + rad;
            f32 max_y = max(a.y, b.y) + rad;

            v2f A = basis_change(v2f{ min_x, min_y });
            v2f B = basis_change(v2f{ min_x, max_y });
            v2f C = basis_change(v2f{ max_x, max_y });
            v2f D = basis_change(v2f{ max_x, min_y });

            bounds[bounds_i++] = { A.x, A.y, (float)stroke_z };
            bounds[bounds_i++] = { B.x, B.y, (float)stroke_z };
            bounds[bounds_i++] = { C.x, C.y, (float)stroke_z };
            bounds[bounds_i++] = { D.x, D.y, (float)stroke_z };
        }

        indices[indices_i++] = (u16)(idx + 0);
        indices[indices_i++] = (u16)(idx + 1);
        indices[indices_i++] = (u16)(idx + 2);
        indices[indices_i++] = (u16)(idx + 2);
        indices[indices_i++] = (u16)(idx + 0);
        indices[indices_i++] = (u16)(idx + 3);

        float pressure_a = stroke->pressures[i];
        float pressure_b = stroke->pressures[i + 1];

        for (int repeat = 0; repeat < 4; ++repeat) {
            apoints[apoints_i++] = { (float)point_i.x, (float)point_i.y, pressure_a };
            bpoints[bpoints_i++] = { (float)point_j.x, (float)point_j.y, pressure_b };
        }
    }

    vk::destroy_buffer(&render_element->vbo_stroke);
    vk::destroy_buffer(&render_element->vbo_pointa);
    vk::destroy_buffer(&render_element->vbo_pointb);
    vk::destroy_buffer(&render_element->indices);

    vk::create_buffer(bounds_i * sizeof(decltype(*bounds)),
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &render_element->vbo_stroke);
    vk::create_buffer(apoints_i * sizeof(decltype(*apoints)),
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &render_element->vbo_pointa);
    vk::create_buffer(bpoints_i * sizeof(decltype(*bpoints)),
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &render_element->vbo_pointb);
    vk::create_buffer(indices_i * sizeof(decltype(*indices)),
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &render_element->indices);

    void* mapped = nullptr;
    vkMapMemory(vk::g_vk_context.device, render_element->vbo_stroke.memory, 0,
                render_element->vbo_stroke.size, 0, &mapped);
    std::memcpy(mapped, bounds, bounds_i * sizeof(decltype(*bounds)));
    vkUnmapMemory(vk::g_vk_context.device, render_element->vbo_stroke.memory);

    vkMapMemory(vk::g_vk_context.device, render_element->vbo_pointa.memory, 0,
                render_element->vbo_pointa.size, 0, &mapped);
    std::memcpy(mapped, apoints, apoints_i * sizeof(decltype(*apoints)));
    vkUnmapMemory(vk::g_vk_context.device, render_element->vbo_pointa.memory);

    vkMapMemory(vk::g_vk_context.device, render_element->vbo_pointb.memory, 0,
                render_element->vbo_pointb.size, 0, &mapped);
    std::memcpy(mapped, bpoints, bpoints_i * sizeof(decltype(*bpoints)));
    vkUnmapMemory(vk::g_vk_context.device, render_element->vbo_pointb.memory);

    vkMapMemory(vk::g_vk_context.device, render_element->indices.memory, 0,
                render_element->indices.size, 0, &mapped);
    std::memcpy(mapped, indices, indices_i * sizeof(decltype(*indices)));
    vkUnmapMemory(vk::g_vk_context.device, render_element->indices.memory);

    render_element->index_count = (uint32_t)indices_i;
    render_element->color = stroke->brush.color;
    render_element->radius = stroke->brush.radius;
    render_element->min_opacity = stroke->brush.pressure_opacity_min;
    render_element->hardness = stroke->brush.hardness;
    render_element->flags = 0;
    if (stroke->flags & StrokeFlag_ERASER) {
        render_element->flags |= RenderElementFlags_ERASER;
    }
    if (stroke->flags & StrokeFlag_PRESSURE_TO_OPACITY) {
        render_element->flags |= RenderElementFlags_PRESSURE_TO_OPACITY;
    }
    if (stroke->flags & StrokeFlag_DISTANCE_TO_OPACITY) {
        render_element->flags |= RenderElementFlags_DISTANCE_TO_OPACITY;
    }

    arena_pop(&scratch_arena);
}

void gpu_free_strokes(RenderBackend* renderer, CanvasState* canvas)
{
    if (canvas->root_layer != NULL) {
        for (Layer* l = canvas->root_layer; l != NULL; l = l->next) {
            StrokeList* sl = &l->strokes;
            StrokeBucket* bucket = &sl->root;
            i64 count = sl->count;
            while (bucket) {
                i64 bucket_count = (count >= STROKELIST_BUCKET_COUNT) ? STROKELIST_BUCKET_COUNT : count;
                for (i64 i = 0; i < bucket_count; ++i) {
                    gpu_reset_stroke(renderer, bucket->data[i].render_handle);
                }
                if (count <= STROKELIST_BUCKET_COUNT) {
                    break;
                }
                count -= STROKELIST_BUCKET_COUNT;
                bucket = bucket->next;
            }
        }
    }
}

void gpu_clip_strokes_and_update(Arena* arena, RenderBackend* renderer,
                                CanvasView* view, i64 render_scale,
                                Layer* root_layer, Stroke* working_stroke,
                                i32 x, i32 y, i32 w, i32 h, ClipFlags flags)
{
    reset(&renderer->clip_array);

    for (Layer* l = root_layer; l != NULL; l = l->next) {
        if (!(l->flags & LayerFlags_VISIBLE)) {
            continue;
        }

        StrokeBucket* bucket = &l->strokes.root;
        i64 count = l->strokes.count;
        while (bucket) {
            i64 bucket_count = (count >= STROKELIST_BUCKET_COUNT) ? STROKELIST_BUCKET_COUNT : count;
            for (i64 i = 0; i < bucket_count; ++i) {
                Stroke* s = &bucket->data[i];
                gpu_cook_stroke(arena, renderer, s, CookStroke_NEW);
                RenderElement* re = reinterpret_cast<RenderElement*>(s->render_handle);
                if (re && re->vbo_stroke.buffer != VK_NULL_HANDLE) {
                    push(&renderer->clip_array, re);
                }
            }
            if (count <= STROKELIST_BUCKET_COUNT) {
                break;
            }
            count -= STROKELIST_BUCKET_COUNT;
            bucket = bucket->next;
        }
    }

    if (working_stroke && working_stroke->num_points > 0) {
        gpu_cook_stroke(arena, renderer, working_stroke, CookStroke_UPDATE_WORKING_STROKE);
        RenderElement* re = reinterpret_cast<RenderElement*>(working_stroke->render_handle);
        if (re && re->vbo_stroke.buffer != VK_NULL_HANDLE) {
            push(&renderer->clip_array, re);
        }
    }
}

void gpu_reset_render_flags(RenderBackend* renderer, int flags)
{
    renderer->flags = flags;
}

void gpu_render(RenderBackend* renderer, i32 view_x, i32 view_y, i32 view_width, i32 view_height)
{
    if (!renderer->initialized) {
        return;
    }
    
    // Wait for previous frame
    vkWaitForFences(vk::g_vk_context.device, 1, &vk::g_vk_context.in_flight_fence, VK_TRUE, UINT64_MAX);
    
    // Acquire next image
    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(vk::g_vk_context.device, vk::g_vk_context.swapchain,
                                           UINT64_MAX, vk::g_vk_context.image_available_semaphore,
                                           VK_NULL_HANDLE, &image_index);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Swapchain needs to be recreated
        milton_log("Swapchain out of date\n");
        return;
    }
    
    vkResetFences(vk::g_vk_context.device, 1, &vk::g_vk_context.in_flight_fence);
    
    // Record command buffer
    VkCommandBuffer cmd = renderer->command_buffers[image_index];
    
    vkResetCommandBuffer(cmd, 0);
    
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = 0;
    
    vkBeginCommandBuffer(cmd, &begin_info);
    
    VkImageLayout prev_layout = renderer->canvas_layout;
    VkImageLayout next_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (prev_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
        prev_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VkImageMemoryBarrier to_color = {};
    to_color.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_color.oldLayout = prev_layout;
    to_color.newLayout = next_layout;
    to_color.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_color.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_color.image = renderer->canvas_image.image;
    to_color.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_color.subresourceRange.baseMipLevel = 0;
    to_color.subresourceRange.levelCount = 1;
    to_color.subresourceRange.baseArrayLayer = 0;
    to_color.subresourceRange.layerCount = 1;
    to_color.srcAccessMask = 0;
    to_color.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &to_color);
    renderer->canvas_layout = next_layout;

    VkRenderPassBeginInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = renderer->canvas_render_pass;
    render_pass_info.framebuffer = renderer->canvas_framebuffer;
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = vk::g_vk_context.swapchain_extent;

    VkClearValue clear_color;
    clear_color.color = {{renderer->background_color.r, renderer->background_color.g,
                         renderer->background_color.b, 1.0f}};
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)vk::g_vk_context.swapchain_extent.width;
    viewport.height = (float)vk::g_vk_context.swapchain_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = vk::g_vk_context.swapchain_extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkDeviceSize offsets[] = {0};
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->quad_pipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, &renderer->quad_vertex_buffer.buffer, offsets);
    vkCmdDraw(cmd, 4, 1, 0, 0);

    if (renderer->stroke_pipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->stroke_pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                renderer->stroke_pipeline_layout, 0, 1,
                                &renderer->stroke_set, 0, nullptr);

        for (i64 i = 0; i < renderer->clip_array.count; ++i) {
            RenderElement* re = renderer->clip_array.data[i];
            if (!re || re->index_count == 0) {
                continue;
            }

            renderer->view_ubo_data.radius = re->radius;
            upload_view_ubo(renderer);

            renderer->brush_ubo_data.color[0] = re->color.r;
            renderer->brush_ubo_data.color[1] = re->color.g;
            renderer->brush_ubo_data.color[2] = re->color.b;
            renderer->brush_ubo_data.color[3] = re->color.a;
            renderer->brush_ubo_data.opacity_min = re->min_opacity;
            renderer->brush_ubo_data.hardness = re->hardness;
            upload_brush_ubo(renderer);

            VkDeviceSize stroke_offsets[] = {0, 0, 0};
            VkBuffer stroke_buffers[] = {
                re->vbo_stroke.buffer,
                re->vbo_pointa.buffer,
                re->vbo_pointb.buffer,
            };
            vkCmdBindVertexBuffers(cmd, 0, 3, stroke_buffers, stroke_offsets);
            vkCmdBindIndexBuffer(cmd, re->indices.buffer, 0, VK_INDEX_TYPE_UINT16);
            vkCmdDrawIndexed(cmd, re->index_count, 1, 0, 0, 0);
        }
    }
    
    vkCmdEndRenderPass(cmd);

    VkImageMemoryBarrier to_sample = {};
    to_sample.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_sample.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    to_sample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_sample.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_sample.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_sample.image = renderer->canvas_image.image;
    to_sample.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_sample.subresourceRange.baseMipLevel = 0;
    to_sample.subresourceRange.levelCount = 1;
    to_sample.subresourceRange.baseArrayLayer = 0;
    to_sample.subresourceRange.layerCount = 1;
    to_sample.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    to_sample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &to_sample);
    renderer->canvas_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkRenderPassBeginInfo swap_pass_info = {};
    swap_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    swap_pass_info.renderPass = vk::g_vk_context.render_pass;
    swap_pass_info.framebuffer = renderer->framebuffers[image_index];
    swap_pass_info.renderArea.offset = {0, 0};
    swap_pass_info.renderArea.extent = vk::g_vk_context.swapchain_extent;
    swap_pass_info.clearValueCount = 1;
    swap_pass_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(cmd, &swap_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    if (renderer->blend_pipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->blend_pipeline);
        VkDescriptorSet sets[] = {renderer->stroke_set, renderer->blend_set};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                renderer->blend_pipeline_layout, 0, 2, sets, 0, nullptr);
        VkDeviceSize quad_offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &renderer->quad_vertex_buffer.buffer, &quad_offset);
        vkCmdDraw(cmd, 4, 1, 0, 0);
    }

    if (renderer->flags & RenderBackendFlags_GUI_VISIBLE) {
        if (renderer->picker_pipeline != VK_NULL_HANDLE &&
            renderer->picker_vertex_buffer.buffer != VK_NULL_HANDLE &&
            renderer->picker_norm_buffer.buffer != VK_NULL_HANDLE) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->picker_pipeline);
            VkDescriptorSet sets[] = {renderer->stroke_set, renderer->picker_set};
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    renderer->picker_pipeline_layout, 0, 2, sets, 0, nullptr);
            VkBuffer picker_buffers[] = {renderer->picker_vertex_buffer.buffer,
                                        renderer->picker_norm_buffer.buffer};
            VkDeviceSize picker_offsets[] = {0, 0};
            vkCmdBindVertexBuffers(cmd, 0, 2, picker_buffers, picker_offsets);
            vkCmdDraw(cmd, 4, 1, 0, 0);
        }
    }

    if (renderer->outline_pipeline != VK_NULL_HANDLE &&
        renderer->outline_vertex_buffer.buffer != VK_NULL_HANDLE &&
        renderer->outline_sizes_buffer.buffer != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->outline_pipeline);
        VkDescriptorSet sets[] = {renderer->stroke_set, renderer->outline_set};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                renderer->outline_pipeline_layout, 0, 2, sets, 0, nullptr);
        VkBuffer outline_buffers[] = {renderer->outline_vertex_buffer.buffer,
                                      renderer->outline_sizes_buffer.buffer};
        VkDeviceSize outline_offsets[] = {0, 0};
        vkCmdBindVertexBuffers(cmd, 0, 2, outline_buffers, outline_offsets);
        vkCmdDraw(cmd, 4, 1, 0, 0);
    }

    // Render ImGui on top
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRenderPass(cmd);
    
    vkEndCommandBuffer(cmd);
    
    // Submit command buffer
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore wait_semaphores[] = {vk::g_vk_context.image_available_semaphore};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    
    VkSemaphore signal_semaphores[] = {vk::g_vk_context.render_finished_semaphore};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;
    
    if (vkQueueSubmit(vk::g_vk_context.graphics_queue, 1, &submit_info, 
                     vk::g_vk_context.in_flight_fence) != VK_SUCCESS) {
        milton_log("Failed to submit draw command buffer\n");
        return;
    }
    
    // Present
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &vk::g_vk_context.swapchain;
    present_info.pImageIndices = &image_index;
    
    vkQueuePresentKHR(vk::g_vk_context.present_queue, &present_info);
}

void gpu_render_to_buffer(Milton* milton, u8* buffer, i32 scale, i32 x, i32 y, i32 w, i32 h, f32 background_alpha)
{
    RenderBackend* renderer = milton->renderer;
    if (!renderer || !renderer->initialized) {
        return;
    }

    VkDeviceSize readback_size = (VkDeviceSize)(w * h * 4);
    if (!ensure_readback_buffer(renderer, readback_size)) {
        milton_log("Failed to allocate readback buffer\n");
        return;
    }

    vkDeviceWaitIdle(vk::g_vk_context.device);

    VkCommandBuffer cmd = vk::begin_single_time_commands();

    VkImageMemoryBarrier to_transfer = {};
    to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_transfer.oldLayout = renderer->canvas_layout;
    to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.image = renderer->canvas_image.image;
    to_transfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_transfer.subresourceRange.baseMipLevel = 0;
    to_transfer.subresourceRange.levelCount = 1;
    to_transfer.subresourceRange.baseArrayLayer = 0;
    to_transfer.subresourceRange.layerCount = 1;
    to_transfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &to_transfer);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {x, y, 0};
    region.imageExtent = { (uint32_t)w, (uint32_t)h, 1 };

    vkCmdCopyImageToBuffer(cmd, renderer->canvas_image.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           renderer->readback_buffer.buffer, 1, &region);

    VkImageMemoryBarrier to_shader = {};
    to_shader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_shader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_shader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader.image = renderer->canvas_image.image;
    to_shader.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_shader.subresourceRange.baseMipLevel = 0;
    to_shader.subresourceRange.levelCount = 1;
    to_shader.subresourceRange.baseArrayLayer = 0;
    to_shader.subresourceRange.layerCount = 1;
    to_shader.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    to_shader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &to_shader);

    renderer->canvas_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vk::end_single_time_commands(cmd);

    void* mapped = nullptr;
    vkMapMemory(vk::g_vk_context.device, renderer->readback_buffer.memory, 0,
                readback_size, 0, &mapped);
    std::memcpy(buffer, mapped, (size_t)readback_size);
    vkUnmapMemory(vk::g_vk_context.device, renderer->readback_buffer.memory);
}

void gpu_release_data(RenderBackend* renderer)
{
    if (!renderer->initialized) {
        return;
    }
    
    vkDeviceWaitIdle(vk::g_vk_context.device);

    if (renderer->quad_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(vk::g_vk_context.device, renderer->quad_pipeline, nullptr);
    }
    if (renderer->quad_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(vk::g_vk_context.device, renderer->quad_pipeline_layout, nullptr);
    }
    vk::destroy_buffer(&renderer->quad_vertex_buffer);

    if (renderer->blend_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(vk::g_vk_context.device, renderer->blend_pipeline, nullptr);
    }
    if (renderer->blend_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(vk::g_vk_context.device, renderer->blend_pipeline_layout, nullptr);
    }
    if (renderer->blend_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vk::g_vk_context.device, renderer->blend_set_layout, nullptr);
    }
    if (renderer->blend_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(vk::g_vk_context.device, renderer->blend_sampler, nullptr);
    }
    if (renderer->canvas_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(vk::g_vk_context.device, renderer->canvas_framebuffer, nullptr);
    }
    if (renderer->canvas_render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(vk::g_vk_context.device, renderer->canvas_render_pass, nullptr);
    }
    vk::destroy_image(&renderer->canvas_image);

    if (renderer->stroke_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(vk::g_vk_context.device, renderer->stroke_pipeline, nullptr);
    }
    if (renderer->stroke_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(vk::g_vk_context.device, renderer->stroke_pipeline_layout, nullptr);
    }
    if (renderer->stroke_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vk::g_vk_context.device, renderer->stroke_set_layout, nullptr);
    }
    vk::destroy_buffer(&renderer->view_ubo);
    vk::destroy_buffer(&renderer->brush_ubo);
    release(&renderer->clip_array);

    if (renderer->picker_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(vk::g_vk_context.device, renderer->picker_pipeline, nullptr);
    }
    if (renderer->picker_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(vk::g_vk_context.device, renderer->picker_pipeline_layout, nullptr);
    }
    if (renderer->picker_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vk::g_vk_context.device, renderer->picker_set_layout, nullptr);
    }
    vk::destroy_buffer(&renderer->picker_ubo);
    vk::destroy_buffer(&renderer->picker_vertex_buffer);
    vk::destroy_buffer(&renderer->picker_norm_buffer);

    if (renderer->outline_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(vk::g_vk_context.device, renderer->outline_pipeline, nullptr);
    }
    if (renderer->outline_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(vk::g_vk_context.device, renderer->outline_pipeline_layout, nullptr);
    }
    if (renderer->outline_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vk::g_vk_context.device, renderer->outline_set_layout, nullptr);
    }
    vk::destroy_buffer(&renderer->outline_ubo);
    vk::destroy_buffer(&renderer->outline_vertex_buffer);
    vk::destroy_buffer(&renderer->outline_sizes_buffer);

    vk::destroy_buffer(&renderer->readback_buffer);

    // Free command buffers
    vkFreeCommandBuffers(vk::g_vk_context.device, vk::g_vk_context.command_pool,
                        vk::g_vk_context.swapchain_image_count, renderer->command_buffers);
    
    // Destroy framebuffers
    for (uint32_t i = 0; i < vk::g_vk_context.swapchain_image_count; i++) {
        vkDestroyFramebuffer(vk::g_vk_context.device, renderer->framebuffers[i], nullptr);
    }
    
    renderer->initialized = false;
}
