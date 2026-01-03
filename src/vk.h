#pragma once

// Vulkan wrapper header for Milton
// Replaces gl.h with Vulkan equivalents

#include <vulkan/vulkan.h>

// Vulkan handles and structures used throughout Milton
namespace vk {

struct Context {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;
    VkSurfaceKHR surface;
    void* window_handle;
    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    
    uint32_t graphics_queue_family;
    uint32_t present_queue_family;
    
    VkCommandPool command_pool;
    VkDescriptorPool descriptor_pool;
    
    // Swapchain images
    uint32_t swapchain_image_count;
    VkImage swapchain_images[8];  // Max 8 swapchain images
    VkImageView swapchain_image_views[8];
    VkFramebuffer swapchain_framebuffers[8];
    
    // Render pass
    VkRenderPass render_pass;

    VkPipelineCache pipeline_cache;
    
    // Synchronization
    static const uint32_t kMaxFramesInFlight = 2;
    VkSemaphore image_available_semaphores[kMaxFramesInFlight];
    VkFence in_flight_fences[kMaxFramesInFlight];
    VkSemaphore render_finished_semaphores[8];
    VkFence images_in_flight[8];
    
    uint32_t current_frame;
    
    // Debug messenger for validation layers
    VkDebugUtilsMessengerEXT debug_messenger;
    
    // Physical device properties
    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceFeatures device_features;
    VkPhysicalDeviceMemoryProperties memory_properties;
};

// Global Vulkan context
extern Context g_vk_context;

// Initialization and cleanup
bool init(void* window_handle);
void cleanup();
bool recreate_swapchain(void* window_handle);

// Helper functions
uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties);
VkCommandBuffer begin_single_time_commands();
void end_single_time_commands(VkCommandBuffer command_buffer);

// Buffer management
struct Buffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
};

bool create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                   VkMemoryPropertyFlags properties, Buffer* out_buffer);
void destroy_buffer(Buffer* buffer);

// Image management
struct Image {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkFormat format;
    uint32_t width;
    uint32_t height;
};

bool create_image(uint32_t width, uint32_t height, VkFormat format,
                  VkImageTiling tiling, VkImageUsageFlags usage,
                  VkMemoryPropertyFlags properties, Image* out_image);
void destroy_image(Image* image);

// Shader management
VkShaderModule create_shader_module(const uint32_t* code, size_t code_size);
VkPipelineShaderStageCreateInfo shader_stage_info(VkShaderModule module,
                                                  VkShaderStageFlagBits stage,
                                                  const char* entry_point = "main");
VkPipelineLayout create_pipeline_layout(const VkDescriptorSetLayout* set_layouts,
                                        uint32_t set_layout_count,
                                        const VkPushConstantRange* push_ranges,
                                        uint32_t push_range_count);
VkPipeline create_graphics_pipeline(const VkGraphicsPipelineCreateInfo* create_info);

// Logging
void log(const char* message);

}  // namespace vk
