// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include "vk.h"
#include "memory.h"
#include "platform.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <cstring>

namespace vk {

Context g_vk_context = {};

static PFN_vkCreateDebugUtilsMessengerEXT g_vkCreateDebugUtilsMessengerEXT = nullptr;
static PFN_vkDestroyDebugUtilsMessengerEXT g_vkDestroyDebugUtilsMessengerEXT = nullptr;

void log(const char* message) {
    milton_log("%s", message);
}

static const char* validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"
};

static const char* device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef MILTON_DEBUG
static const bool enable_validation_layers = true;
#else
static const bool enable_validation_layers = false;
#endif

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
    
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "Vulkan validation: %s\n", callback_data->pMessage);
        vk::log(buffer);
    }
    return VK_FALSE;
}

static bool check_validation_layer_support() {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
    
    for (const char* layer_name : validation_layers) {
        bool found = false;
        for (const auto& layer : available_layers) {
            if (strcmp(layer_name, layer.layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

static bool create_render_pass();

static bool create_instance(SDL_Window* window) {
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Milton";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 10, 1);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;
    
    // Get SDL required extensions
    uint32_t sdl_extension_count = 0;
    const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);
    
    std::vector<const char*> extensions(sdl_extensions, sdl_extensions + sdl_extension_count);
    
    if (enable_validation_layers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();
    
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {};
    if (enable_validation_layers) {
        if (!check_validation_layer_support()) {
            vk::log("Validation layers requested but not available\n");
            return false;
        }
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = validation_layers;
        
        debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_create_info.pfnUserCallback = debug_callback;
        create_info.pNext = &debug_create_info;
    }
    
    if (vkCreateInstance(&create_info, nullptr, &g_vk_context.instance) != VK_SUCCESS) {
        vk::log("Failed to create Vulkan instance\n");
        return false;
    }

    g_vkCreateDebugUtilsMessengerEXT =
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(g_vk_context.instance, "vkCreateDebugUtilsMessengerEXT"));
    g_vkDestroyDebugUtilsMessengerEXT =
        reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(g_vk_context.instance, "vkDestroyDebugUtilsMessengerEXT"));
    
    // Create debug messenger
    if (enable_validation_layers) {
        if (!g_vkCreateDebugUtilsMessengerEXT ||
            g_vkCreateDebugUtilsMessengerEXT(g_vk_context.instance, &debug_create_info,
                                             nullptr, &g_vk_context.debug_messenger) != VK_SUCCESS) {
            vk::log("Failed to set up debug messenger\n");
        }
    }
    
    vk::log("Created Vulkan instance\n");
    return true;
}

static bool create_surface(SDL_Window* window) {
    if (!SDL_Vulkan_CreateSurface(window, g_vk_context.instance, nullptr, &g_vk_context.surface)) {
        vk::log("Failed to create Vulkan surface\n");
        return false;
    }
    return true;
}

static bool pick_physical_device() {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_vk_context.instance, &device_count, nullptr);
    
    if (device_count == 0) {
        vk::log("Failed to find GPUs with Vulkan support\n");
        return false;
    }
    
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(g_vk_context.instance, &device_count, devices.data());
    
    // Pick first discrete GPU, or first GPU if no discrete GPU found
    VkPhysicalDevice selected_device = VK_NULL_HANDLE;
    for (const auto& device : devices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);
        
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            selected_device = device;
            break;
        }
        if (selected_device == VK_NULL_HANDLE) {
            selected_device = device;
        }
    }
    
    if (selected_device == VK_NULL_HANDLE) {
        vk::log("Failed to find suitable GPU\n");
        return false;
    }
    
    g_vk_context.physical_device = selected_device;
    vkGetPhysicalDeviceProperties(g_vk_context.physical_device, &g_vk_context.device_properties);
    vkGetPhysicalDeviceFeatures(g_vk_context.physical_device, &g_vk_context.device_features);
    vkGetPhysicalDeviceMemoryProperties(g_vk_context.physical_device, &g_vk_context.memory_properties);
    
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "Selected GPU: %s\n", g_vk_context.device_properties.deviceName);
    vk::log(buffer);
    
    return true;
}

static bool find_queue_families() {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_vk_context.physical_device, &queue_family_count, nullptr);
    
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(g_vk_context.physical_device, &queue_family_count, queue_families.data());
    
    g_vk_context.graphics_queue_family = UINT32_MAX;
    g_vk_context.present_queue_family = UINT32_MAX;
    
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            g_vk_context.graphics_queue_family = i;
        }
        
        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(g_vk_context.physical_device, i, g_vk_context.surface, &present_support);
        if (present_support) {
            g_vk_context.present_queue_family = i;
        }
        
        if (g_vk_context.graphics_queue_family != UINT32_MAX && 
            g_vk_context.present_queue_family != UINT32_MAX) {
            break;
        }
    }
    
    if (g_vk_context.graphics_queue_family == UINT32_MAX || 
        g_vk_context.present_queue_family == UINT32_MAX) {
        vk::log("Failed to find suitable queue families\n");
        return false;
    }
    
    return true;
}

static bool create_logical_device() {
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::vector<uint32_t> unique_queue_families;
    unique_queue_families.push_back(g_vk_context.graphics_queue_family);
    if (g_vk_context.present_queue_family != g_vk_context.graphics_queue_family) {
        unique_queue_families.push_back(g_vk_context.present_queue_family);
    }
    
    float queue_priority = 1.0f;
    for (uint32_t queue_family : unique_queue_families) {
        VkDeviceQueueCreateInfo queue_create_info = {};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }
    
    VkPhysicalDeviceFeatures device_features = {};
    device_features.samplerAnisotropy = VK_TRUE;
    device_features.fillModeNonSolid = VK_TRUE;
    
    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = 1;
    create_info.ppEnabledExtensionNames = device_extensions;
    
    if (enable_validation_layers) {
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = validation_layers;
    }
    
    if (vkCreateDevice(g_vk_context.physical_device, &create_info, nullptr, &g_vk_context.device) != VK_SUCCESS) {
        vk::log("Failed to create logical device\n");
        return false;
    }
    
    vkGetDeviceQueue(g_vk_context.device, g_vk_context.graphics_queue_family, 0, &g_vk_context.graphics_queue);
    vkGetDeviceQueue(g_vk_context.device, g_vk_context.present_queue_family, 0, &g_vk_context.present_queue);

    return true;
}

static bool create_swapchain(SDL_Window* window) {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vk_context.physical_device, g_vk_context.surface, &capabilities);
    
    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_vk_context.physical_device, g_vk_context.surface, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_vk_context.physical_device, g_vk_context.surface, &format_count, formats.data());
    
    // Pick format
    VkSurfaceFormatKHR surface_format = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = format;
            break;
        }
    }
    
    // Get window size
    int width, height;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    
    VkExtent2D extent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };
    
    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = g_vk_context.surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    uint32_t queue_family_indices[] = {g_vk_context.graphics_queue_family, g_vk_context.present_queue_family};
    if (g_vk_context.graphics_queue_family != g_vk_context.present_queue_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    
    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;  // VSync
    create_info.clipped = VK_TRUE;
    
    if (vkCreateSwapchainKHR(g_vk_context.device, &create_info, nullptr, &g_vk_context.swapchain) != VK_SUCCESS) {
        vk::log("Failed to create swapchain\n");
        return false;
    }
    
    g_vk_context.swapchain_format = surface_format.format;
    g_vk_context.swapchain_extent = extent;
    
    // Get swapchain images
    vkGetSwapchainImagesKHR(g_vk_context.device, g_vk_context.swapchain, &g_vk_context.swapchain_image_count, nullptr);
    vkGetSwapchainImagesKHR(g_vk_context.device, g_vk_context.swapchain, &g_vk_context.swapchain_image_count, g_vk_context.swapchain_images);
    
    // Create image views
    for (uint32_t i = 0; i < g_vk_context.swapchain_image_count; i++) {
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = g_vk_context.swapchain_images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = g_vk_context.swapchain_format;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(g_vk_context.device, &view_info, nullptr, &g_vk_context.swapchain_image_views[i]) != VK_SUCCESS) {
            vk::log("Failed to create image view\n");
            return false;
        }
    }
    
    return true;
}

static void destroy_swapchain_resources() {
    for (uint32_t i = 0; i < g_vk_context.swapchain_image_count; i++) {
        if (g_vk_context.swapchain_image_views[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(g_vk_context.device, g_vk_context.swapchain_image_views[i], nullptr);
            g_vk_context.swapchain_image_views[i] = VK_NULL_HANDLE;
        }
    }
    g_vk_context.swapchain_image_count = 0;
    for (uint32_t i = 0; i < 8; ++i) {
        g_vk_context.swapchain_images[i] = VK_NULL_HANDLE;
        g_vk_context.swapchain_image_views[i] = VK_NULL_HANDLE;
    }

    if (g_vk_context.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(g_vk_context.device, g_vk_context.swapchain, nullptr);
        g_vk_context.swapchain = VK_NULL_HANDLE;
    }

    if (g_vk_context.render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(g_vk_context.device, g_vk_context.render_pass, nullptr);
        g_vk_context.render_pass = VK_NULL_HANDLE;
    }
}

bool recreate_swapchain(void* window_handle) {
    SDL_Window* window = static_cast<SDL_Window*>(window_handle);
    if (!window) {
        return false;
    }
    vkDeviceWaitIdle(g_vk_context.device);
    destroy_swapchain_resources();
    if (!create_swapchain(window)) return false;
    if (!create_render_pass()) return false;
    return true;
}

static bool create_render_pass() {
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = g_vk_context.swapchain_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    
    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;
    
    if (vkCreateRenderPass(g_vk_context.device, &render_pass_info, nullptr, &g_vk_context.render_pass) != VK_SUCCESS) {
        vk::log("Failed to create render pass\n");
        return false;
    }
    
    return true;
}

static bool create_command_pool() {
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = g_vk_context.graphics_queue_family;
    
    if (vkCreateCommandPool(g_vk_context.device, &pool_info, nullptr, &g_vk_context.command_pool) != VK_SUCCESS) {
        vk::log("Failed to create command pool\n");
        return false;
    }
    
    return true;
}

static bool create_descriptor_pool() {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * 11;
    pool_info.poolSizeCount = 11;
    pool_info.pPoolSizes = pool_sizes;
    
    if (vkCreateDescriptorPool(g_vk_context.device, &pool_info, nullptr, &g_vk_context.descriptor_pool) != VK_SUCCESS) {
        vk::log("Failed to create descriptor pool\n");
        return false;
    }
    
    return true;
}

static bool create_pipeline_cache() {
    VkPipelineCacheCreateInfo cache_info = {};
    cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    if (vkCreatePipelineCache(g_vk_context.device, &cache_info, nullptr, &g_vk_context.pipeline_cache) != VK_SUCCESS) {
        vk::log("Failed to create pipeline cache\n");
        return false;
    }

    return true;
}

static bool create_sync_objects() {
    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    if (vkCreateSemaphore(g_vk_context.device, &semaphore_info, nullptr, &g_vk_context.image_available_semaphore) != VK_SUCCESS ||
        vkCreateSemaphore(g_vk_context.device, &semaphore_info, nullptr, &g_vk_context.render_finished_semaphore) != VK_SUCCESS ||
        vkCreateFence(g_vk_context.device, &fence_info, nullptr, &g_vk_context.in_flight_fence) != VK_SUCCESS) {
        vk::log("Failed to create synchronization objects\n");
        return false;
    }
    
    return true;
}

bool init(void* window_handle) {
    SDL_Window* window = static_cast<SDL_Window*>(window_handle);
    g_vk_context.window_handle = window_handle;
    
    if (!create_instance(window)) return false;
    if (!create_surface(window)) return false;
    if (!pick_physical_device()) return false;
    if (!find_queue_families()) return false;
    if (!create_logical_device()) return false;
    if (!create_swapchain(window)) return false;
    if (!create_render_pass()) return false;
    if (!create_command_pool()) return false;
    if (!create_descriptor_pool()) return false;
    if (!create_pipeline_cache()) return false;
    if (!create_sync_objects()) return false;
    
    g_vk_context.current_frame = 0;
    
    vk::log("Vulkan initialization complete\n");
    return true;
}

void cleanup() {
    if (g_vk_context.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(g_vk_context.device);
        
        vkDestroySemaphore(g_vk_context.device, g_vk_context.image_available_semaphore, nullptr);
        vkDestroySemaphore(g_vk_context.device, g_vk_context.render_finished_semaphore, nullptr);
        vkDestroyFence(g_vk_context.device, g_vk_context.in_flight_fence, nullptr);

        if (g_vk_context.pipeline_cache != VK_NULL_HANDLE) {
            vkDestroyPipelineCache(g_vk_context.device, g_vk_context.pipeline_cache, nullptr);
        }

        vkDestroyDescriptorPool(g_vk_context.device, g_vk_context.descriptor_pool, nullptr);
        vkDestroyCommandPool(g_vk_context.device, g_vk_context.command_pool, nullptr);
        
        destroy_swapchain_resources();
        vkDestroyDevice(g_vk_context.device, nullptr);
    }
    
    if (g_vk_context.debug_messenger != VK_NULL_HANDLE) {
        if (g_vkDestroyDebugUtilsMessengerEXT) {
            g_vkDestroyDebugUtilsMessengerEXT(g_vk_context.instance, g_vk_context.debug_messenger, nullptr);
        }
    }
    
    if (g_vk_context.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_vk_context.instance, g_vk_context.surface, nullptr);
    }
    
    if (g_vk_context.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_vk_context.instance, nullptr);
    }
}

uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < g_vk_context.memory_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && 
            (g_vk_context.memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

VkCommandBuffer begin_single_time_commands() {
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = g_vk_context.command_pool;
    alloc_info.commandBufferCount = 1;
    
    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(g_vk_context.device, &alloc_info, &command_buffer);
    
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(command_buffer, &begin_info);
    return command_buffer;
}

void end_single_time_commands(VkCommandBuffer command_buffer) {
    vkEndCommandBuffer(command_buffer);
    
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    
    vkQueueSubmit(g_vk_context.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_vk_context.graphics_queue);
    
    vkFreeCommandBuffers(g_vk_context.device, g_vk_context.command_pool, 1, &command_buffer);
}

bool create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                   VkMemoryPropertyFlags properties, Buffer* out_buffer) {
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(g_vk_context.device, &buffer_info, nullptr, &out_buffer->buffer) != VK_SUCCESS) {
        return false;
    }
    
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(g_vk_context.device, out_buffer->buffer, &mem_requirements);
    
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(g_vk_context.device, &alloc_info, nullptr, &out_buffer->memory) != VK_SUCCESS) {
        vkDestroyBuffer(g_vk_context.device, out_buffer->buffer, nullptr);
        return false;
    }
    
    vkBindBufferMemory(g_vk_context.device, out_buffer->buffer, out_buffer->memory, 0);
    out_buffer->size = size;
    
    return true;
}

void destroy_buffer(Buffer* buffer) {
    if (buffer->buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_vk_context.device, buffer->buffer, nullptr);
    }
    if (buffer->memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_vk_context.device, buffer->memory, nullptr);
    }
    buffer->buffer = VK_NULL_HANDLE;
    buffer->memory = VK_NULL_HANDLE;
}

bool create_image(uint32_t width, uint32_t height, VkFormat format,
                  VkImageTiling tiling, VkImageUsageFlags usage,
                  VkMemoryPropertyFlags properties, Image* out_image) {
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateImage(g_vk_context.device, &image_info, nullptr, &out_image->image) != VK_SUCCESS) {
        return false;
    }
    
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(g_vk_context.device, out_image->image, &mem_requirements);
    
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(g_vk_context.device, &alloc_info, nullptr, &out_image->memory) != VK_SUCCESS) {
        vkDestroyImage(g_vk_context.device, out_image->image, nullptr);
        return false;
    }
    
    vkBindImageMemory(g_vk_context.device, out_image->image, out_image->memory, 0);
    
    out_image->format = format;
    out_image->width = width;
    out_image->height = height;
    
    return true;
}

void destroy_image(Image* image) {
    if (image->view != VK_NULL_HANDLE) {
        vkDestroyImageView(g_vk_context.device, image->view, nullptr);
    }
    if (image->image != VK_NULL_HANDLE) {
        vkDestroyImage(g_vk_context.device, image->image, nullptr);
    }
    if (image->memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_vk_context.device, image->memory, nullptr);
    }
    image->view = VK_NULL_HANDLE;
    image->image = VK_NULL_HANDLE;
    image->memory = VK_NULL_HANDLE;
}

VkShaderModule create_shader_module(const uint32_t* code, size_t code_size) {
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code_size;
    create_info.pCode = code;
    
    VkShaderModule shader_module;
    if (vkCreateShaderModule(g_vk_context.device, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    
    return shader_module;
}

VkPipelineShaderStageCreateInfo shader_stage_info(VkShaderModule module,
                                                  VkShaderStageFlagBits stage,
                                                  const char* entry_point) {
    VkPipelineShaderStageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage = stage;
    info.module = module;
    info.pName = entry_point;
    return info;
}

VkPipelineLayout create_pipeline_layout(const VkDescriptorSetLayout* set_layouts,
                                        uint32_t set_layout_count,
                                        const VkPushConstantRange* push_ranges,
                                        uint32_t push_range_count) {
    VkPipelineLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = set_layout_count;
    layout_info.pSetLayouts = set_layouts;
    layout_info.pushConstantRangeCount = push_range_count;
    layout_info.pPushConstantRanges = push_ranges;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(g_vk_context.device, &layout_info, nullptr, &layout) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return layout;
}

VkPipeline create_graphics_pipeline(const VkGraphicsPipelineCreateInfo* create_info) {
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(g_vk_context.device, g_vk_context.pipeline_cache, 1,
                                  create_info, nullptr, &pipeline) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return pipeline;
}

}  // namespace vk
