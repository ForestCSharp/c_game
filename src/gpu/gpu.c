#include "gpu.h"
#include "math/basic_math.h"
#include "stretchy_buffer.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// FCS TODO: MoltenVK testing
PFN_vkCmdBeginRenderingKHR pfn_begin_rendering = NULL;
PFN_vkCmdEndRenderingKHR pfn_end_rendering = NULL;

#define VK_CHECK(f)                                                                 \
{                                                                                   \
	VkResult res = (f);                                                             \
	if (res != VK_SUCCESS)                                                          \
	{                                                                               \
		printf("VULKAN ERROR: %s RETURNED: %i LINE: %i\n", #f, res, __LINE__);      \
		exit(1);                                                                    \
	}                                                                               \
}

VkBool32 vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                               VkDebugUtilsMessageTypeFlagsEXT messageType,
                               const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
    if (!(messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT))
    {
        printf("Validation Layer: %s\n", pCallbackData->pMessage);
    }
    return VK_FALSE;
}

u32 gpu_format_stride(GpuFormat format)
{
    switch (format)
    {
    case GPU_FORMAT_R32_SINT:
    case GPU_FORMAT_RGBA8_UNORM:
    case GPU_FORMAT_BGRA8_UNORM:
    case GPU_FORMAT_RGBA8_SRGB:
    case GPU_FORMAT_BGRA8_SRGB:
        return 4;
    case GPU_FORMAT_RG32_SFLOAT:
        return sizeof(float) * 2;
    case GPU_FORMAT_RGB32_SFLOAT:
        return sizeof(float) * 3;
    case GPU_FORMAT_RGBA32_SFLOAT:
        return sizeof(float) * 4;
    default:
        printf("Error: Unhandled Format in gpu_format_stride\n");
        exit(0);
        return 0;
    }
}

typedef struct
{
    VkPhysicalDevice physical_device;
    int graphics_family_index;
    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;
} VulkanPhysicalDeviceData;

VulkanPhysicalDeviceData vulkan_choose_physical_device(VkInstance instance, VkSurfaceKHR surface)
{

    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    int graphics_family_index = -1;
    VkSurfaceFormatKHR surface_format = {};
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

    u32 physical_device_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, NULL));
    if (physical_device_count > 0)
    {
        VkPhysicalDevice physical_devices[physical_device_count];
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices));
        for (int i = 0; i < physical_device_count; ++i)
        {
            u32 format_count;
            VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_devices[i], surface, &format_count, NULL));
            bool found_format = false;
            if (format_count > 0)
            {
                VkSurfaceFormatKHR surface_formats[format_count];

                VK_CHECK(
                    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_devices[i], surface, &format_count, surface_formats));
                for (int i = 0; i < format_count; ++i)
                {
                    printf("format: %i\n", surface_formats[i].format);
                    if (surface_formats[i].format == GPU_FORMAT_BGRA8_UNORM)
                    { // FIXME: more robust format choosing
                        surface_format = surface_formats[i];
                        found_format = true;
                        break;
                    }
                }
            }

            assert(found_format);

            u32 present_mode_count;
            VK_CHECK(
                vkGetPhysicalDeviceSurfacePresentModesKHR(physical_devices[i], surface, &present_mode_count, NULL));
            if (present_mode_count > 0)
            {
                VkPresentModeKHR present_modes[present_mode_count];
                VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_devices[i], surface, &present_mode_count,
                                                                   present_modes));

                for (int i = 0; i < present_mode_count; ++i)
                {
                    if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
                    {
                        present_mode = present_modes[i];
                        break;
                    }

                    if (present_modes[i] == VK_PRESENT_MODE_FIFO_KHR)
                    {
                        present_mode = present_modes[i];
                        break;
                    }
                }
            }

            // Check for correct queue families
            u32 queue_family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_count, NULL);
            if (queue_family_count > 0)
            {
                VkQueueFamilyProperties queue_families[queue_family_count];
                vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_count, queue_families);

                for (int i = 0; i < queue_family_count; ++i)
                {
                    VkBool32 surface_supported = VK_FALSE;
                    vkGetPhysicalDeviceSurfaceSupportKHR(physical_devices[i], i, surface, &surface_supported);

                    if (surface_supported == VK_TRUE && queue_families[i].queueCount > 0 &&
                        queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                    {
                        physical_device = physical_devices[i];
                        graphics_family_index = i;
                        break;
                    }
                }
            }
        }
    }

    VkPhysicalDeviceProperties physical_device_properties = {};
    vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);
    printf("Device Name: %s\n", physical_device_properties.deviceName);

    return (VulkanPhysicalDeviceData){
        .physical_device = physical_device,
        .graphics_family_index = graphics_family_index,
        .surface_format = surface_format,
        .present_mode = present_mode,
    };
}

GpuContext gpu_create_context(const Window *const window)
{
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "C Game",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "C Game",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    u32 enumerated_layer_count;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&enumerated_layer_count, NULL));
    if (enumerated_layer_count > 0)
    {
        VkLayerProperties enumerated_layer_properties[enumerated_layer_count];
        VK_CHECK(vkEnumerateInstanceLayerProperties(&enumerated_layer_count, enumerated_layer_properties));

        for (u32 i = 0; i < enumerated_layer_count; ++i)
        {
            printf("Instance Layer: %s\n", enumerated_layer_properties[i].layerName);
        }
    }

    const char *validation_layers[] = {
        "VK_LAYER_KHRONOS_validation",
        // "VK_LAYER_LUNARG_api_dump"
    };
    u32 validation_layer_count = sizeof(validation_layers) / sizeof(validation_layers[0]);

    // FCS TODO: Clean up defines. query surface extension string from window system?
    const char *extensions[] = {
        "VK_KHR_surface",
#if defined(_WIN32)
        "VK_KHR_win32_surface",
#elif defined(__APPLE__)
        VK_EXT_METAL_SURFACE_EXTENSION_NAME,
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    };
    u32 extension_count = sizeof(extensions) / sizeof(extensions[0]);
    printf("Extensions (%u)\n", extension_count);
    for (i32 i = 0; i < extension_count; ++i)
    {
        printf("\t%s\n", extensions[i]);
    }

    VkInstanceCreateInfo instance_create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR, // FCS TODO: Mac Only
        .pApplicationInfo = &app_info,
        .enabledLayerCount = validation_layer_count,
        .ppEnabledLayerNames = validation_layers,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions,
    };

    VkInstance instance = NULL;
    VK_CHECK(vkCreateInstance(&instance_create_info, NULL, &instance));

    VkDebugUtilsMessengerCreateInfoEXT debug_utils_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = 0,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = vulkan_debug_callback,
        .pUserData = NULL};

    PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessenger =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (CreateDebugUtilsMessenger == NULL)
        printf("ERROR: PFN_vkCreateDebugUtilsMessengerEXT\n");

    VkDebugUtilsMessengerEXT debug_messenger;
    VK_CHECK(CreateDebugUtilsMessenger(instance, &debug_utils_create_info, NULL, &debug_messenger));

    VkSurfaceKHR surface;

// FCS TODO: Clean up defines. request surface creation from Window system?
#if defined(_WIN32)
    VkWin32SurfaceCreateInfoKHR surface_create_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .hinstance = window->hinstance,
        .hwnd = window->hwnd,
    };
    VK_CHECK(vkCreateWin32SurfaceKHR(instance, &surface_create_info, NULL, &surface));
#elif defined(__APPLE__)
    VkMetalSurfaceCreateInfoEXT surface_create_info = {
        .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = 0,
        .pLayer = window->metal_layer,
    };
    VK_CHECK(vkCreateMetalSurfaceEXT(instance, &surface_create_info, NULL, &surface));
#endif

    VulkanPhysicalDeviceData physical_device_data = vulkan_choose_physical_device(instance, surface);

    float graphics_queue_priority = 1.0f;
    VkDeviceQueueCreateInfo graphics_queue_create_info = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                          .pNext = NULL,
                                                          .flags = 0,
                                                          .queueFamilyIndex =
                                                              physical_device_data.graphics_family_index,
                                                          .queueCount = 1,
                                                          .pQueuePriorities = &graphics_queue_priority};

    VkPhysicalDeviceFeatures physical_device_features = {
        .samplerAnisotropy = VK_TRUE,
    };

    const char *device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, // FCS TODO: MOLTEN_VK is on Vulkan 1.2, so request extension
#if defined(__APPLE__)
        "VK_KHR_portability_subset",
#endif
    };
    u32 device_extension_count = sizeof(device_extensions) / sizeof(device_extensions[0]);

    VkPhysicalDeviceSynchronization2Features sync_2_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
        .pNext = NULL,
    };

    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .pNext = &sync_2_features,
    };

    VkPhysicalDeviceFeatures2 features_2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &dynamic_rendering_features,
        .features = physical_device_features,
    };
    vkGetPhysicalDeviceFeatures2(physical_device_data.physical_device, &features_2);

#if defined(ENABLE_VULKAN_SYNC2)
    if (!sync_2_features.synchronization2)
    {
        printf("Error: Synchronization2 Is Required\n");
        exit(0);
    }
#endif

    if (!dynamic_rendering_features.dynamicRendering)
    {
        printf("Error: Dynamic Rendering Is Required\n");
        exit(0);
    }

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features_2,
        .flags = 0,
        .pQueueCreateInfos = &graphics_queue_create_info,
        .queueCreateInfoCount = 1,
        .pEnabledFeatures = NULL,
        .enabledLayerCount = validation_layer_count,
        .ppEnabledLayerNames = validation_layers,
        .enabledExtensionCount = device_extension_count,
        .ppEnabledExtensionNames = device_extensions,
    };

    VkDevice device = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(physical_device_data.physical_device, &device_create_info, NULL, &device));

    // FCS TODO: Testing MoltenVK
    pfn_begin_rendering = (PFN_vkCmdBeginRenderingKHR)vkGetDeviceProcAddr(device, "vkCmdBeginRenderingKHR");
    pfn_end_rendering = (PFN_vkCmdEndRenderingKHR)vkGetDeviceProcAddr(device, "vkCmdEndRenderingKHR");
    assert(pfn_begin_rendering && pfn_end_rendering);

    VkQueue graphics_queue;
    vkGetDeviceQueue(device, physical_device_data.graphics_family_index, 0, &graphics_queue);

    VkCommandPoolCreateInfo graphics_command_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = physical_device_data.graphics_family_index,
    };
    VkCommandPool graphics_command_pool;
    vkCreateCommandPool(device, &graphics_command_pool_create_info, NULL, &graphics_command_pool);

    // Set up Memory Types in Context
    VkPhysicalDeviceMemoryProperties2 vk_memory_properties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
    };
    vkGetPhysicalDeviceMemoryProperties2(physical_device_data.physical_device, &vk_memory_properties);

    // FIXME: get VkPhysicalDeviceMemoryBudgetPropertiesEXT above and store it so we have heap budgets
    // FIXME: get every second (other applications are also consuming GPU memory)? that seems to be what
    // VK_EXT_memory_budget suggests?

    GpuContext context = {
        .instance = instance,
        .physical_device = physical_device_data.physical_device,
        .device = device,
        .graphics_queue = graphics_queue,
        .graphics_command_pool = graphics_command_pool,
        .surface = surface,
        .present_mode = physical_device_data.present_mode,
        .surface_format = physical_device_data.surface_format,
        .swapchain = VK_NULL_HANDLE,
        .debug_messenger = debug_messenger,
        .pfn_set_object_name =
            (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"),
        .num_memory_types = vk_memory_properties.memoryProperties.memoryTypeCount,
        .memory_types = calloc(vk_memory_properties.memoryProperties.memoryTypeCount, sizeof(GpuMemoryType)),
        .vk_memory_properties = vk_memory_properties.memoryProperties,
    };

    // initial swapchain setup
    gpu_resize_swapchain(&context, window);

    return context;
}

void gpu_destroy_context(GpuContext *context)
{

    for (int i = 0; i < context->swapchain_image_count; ++i)
    {
        gpu_destroy_image_view(context, &context->swapchain_image_views[i]);
    }
    vkDestroySwapchainKHR(context->device, context->swapchain, NULL);

    vkDestroySurfaceKHR(context->instance, context->surface, NULL);

    vkDestroyCommandPool(context->device, context->graphics_command_pool, NULL);

    vkDestroyDevice(context->device, NULL);

    PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessenger =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context->instance,
                                                                   "vkDestroyDebugUtilsMessengerEXT");
    if (DestroyDebugUtilsMessenger != NULL)
        DestroyDebugUtilsMessenger(context->instance, context->debug_messenger, NULL);

    vkDestroyInstance(context->instance, NULL);

    free(context->memory_types);
}
void gpu_wait_idle(GpuContext *context)
{
    VK_CHECK(vkDeviceWaitIdle(context->device));
}

void gpu_resize_swapchain(GpuContext *context, const Window *const window)
{
    gpu_wait_idle(context);

    if (context->swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
    }

    int swapchain_width = 0;
    int swapchain_height = 0;
    window_get_dimensions(window, &swapchain_width, &swapchain_height);

    VkSurfaceCapabilitiesKHR surface_capabilities;
    VK_CHECK(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->physical_device, context->surface, &surface_capabilities));
    VkExtent2D swapchain_extent = {
        .width = CLAMP(swapchain_width, surface_capabilities.minImageExtent.width,
                       surface_capabilities.maxImageExtent.width),
        .height = CLAMP(swapchain_height, surface_capabilities.minImageExtent.height,
                        surface_capabilities.maxImageExtent.height),
    };

    u32 swapchain_image_count = surface_capabilities.minImageCount + 1;
    if (swapchain_image_count > surface_capabilities.maxImageCount)
    {
        swapchain_image_count = surface_capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = context->surface,
        .minImageCount = swapchain_image_count,
        .imageFormat = context->surface_format.format,
        .imageColorSpace = context->surface_format.colorSpace,
        .imageExtent = swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = context->present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE, // Could be used for dyanmic res?
    };

    VK_CHECK(vkCreateSwapchainKHR(context->device, &swapchain_create_info, NULL, &context->swapchain));

    VK_CHECK(vkGetSwapchainImagesKHR(context->device, context->swapchain, &swapchain_image_count, NULL));
    VkImage swapchain_images[swapchain_image_count];
    VK_CHECK(vkGetSwapchainImagesKHR(context->device, context->swapchain, &swapchain_image_count, swapchain_images));

    VkImageView swapchain_image_views[swapchain_image_count];
    for (int i = 0; i < swapchain_image_count; ++i)
    {
        VkImageViewCreateInfo image_view_create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .flags = 0,
            .pNext = NULL,
            .image = swapchain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = context->surface_format.format,
            .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1,
        };
        VK_CHECK(vkCreateImageView(context->device, &image_view_create_info, NULL, &swapchain_image_views[i]));
    }

    if (context->swapchain_images != NULL)
    {
        free(context->swapchain_images);
    }
    context->swapchain_images = malloc(swapchain_image_count * sizeof(GpuImage));
    for (i32 i = 0; i < swapchain_image_count; ++i)
    {
        context->swapchain_images[i] = (GpuImage){
            .vk_image = swapchain_images[i],
            .memory = NULL, // Memory managed externally
            .format = (GpuFormat)context->surface_format.format,
        };
    }

    if (context->swapchain_image_views != NULL)
    {
        // use context->swapchain_image_count as thats the previous image count (likely the same)
        for (int i = 0; i < context->swapchain_image_count; ++i)
        {
            gpu_destroy_image_view(context, &context->swapchain_image_views[i]);
        }
        free(context->swapchain_image_views);
    }
    context->swapchain_image_views = malloc(swapchain_image_count * sizeof(VkImageView));
    memcpy(context->swapchain_image_views, swapchain_image_views, swapchain_image_count * sizeof(VkImageView));

    context->swapchain_image_count = swapchain_image_count;
}

// FIXME: From VulkanTutorial: We'll add error handling for both vkAcquireNextImageKHR and
// vkQueuePresentKHR in the next chapter, because their failure does not necessarily mean
// that the program should terminate, unlike the functions we've seen so far.

u32 gpu_acquire_next_image(GpuContext *context, GpuSemaphore *semaphore)
{
    u32 image_index;
    vkAcquireNextImageKHR(context->device, context->swapchain, UINT64_MAX, semaphore->vk_semaphore, VK_NULL_HANDLE,
                          &image_index);
    return image_index;
}

u32 vulkan_find_memory_type(VkPhysicalDevice *physical_device, u32 type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(*physical_device, &memory_properties);

    for (u32 i = 0; i < memory_properties.memoryTypeCount; ++i)
    {
        if ((type_filter & (1 << i)) && ((memory_properties.memoryTypes[i].propertyFlags & properties) == properties))
        {
            return i;
        }
    }

    return UINT_MAX;
}

// TODO: memory allocator
//  - need way to track frees amongst memory blocks of a given memory type
//  - basically a "free list", but where each element of the list provides info on which memory_block, offset, size
//  - also need to free up VkDeviceMemory blocks when a free makes them unused (see if sum of free list elements for
//  that block is its size?)

#define MB *1024 * 1024ULL
#define GPU_BLOCK_SIZE 256 MB

/*TODO: bufferImageGranularity: If the previous block contains a non-linear resource while the current one is linear or
   vice versa then the alignment requirement is the max of the VkMemoryRequirements.alignment and the device's
   bufferImageGranularity. */

VkDeviceSize gpu_memory_used = 0;

GpuMemory *gpu_allocate_memory(GpuContext *context, u32 type_filter, GpuMemoryPropertyFlags memory_properties,
                               u64 alloc_size, u64 alignment)
{

    u32 memory_type_index = vulkan_find_memory_type(&context->physical_device, type_filter, memory_properties);
    assert(memory_type_index < context->num_memory_types);

    GpuMemoryType *memory_type = &context->memory_types[memory_type_index];
    assert(memory_type);

    // Search Blocks of Memory Type for free list entry of sufficient size
    for (u32 block_index = 0; block_index < sb_count(memory_type->memory_blocks); ++block_index)
    {
        GpuMemoryBlock *block = &memory_type->memory_blocks[block_index];
        assert(block);

        for (u32 free_list_index = 0; free_list_index < sb_count(block->free_list); ++free_list_index)
        {
            GpuMemoryRegion *free_list_region = &block->free_list[free_list_index];
            assert(free_list_region);

            u64 padding = alignment - free_list_region->offset % alignment;
            u64 new_region_size = alloc_size + padding;

            if (free_list_region->size >= new_region_size)
            {
                sb_push(
					block->used_list, 
					((GpuMemoryRegion){
					  .padding = padding,
					  .offset = free_list_region->offset + padding,
					  .size = alloc_size,
					  .owning_block = block,
					  .alloc_ref = calloc(1, sizeof(GpuMemory)),
					})
				);

                GpuMemoryRegion *out_region = &block->used_list[sb_count(block->used_list) - 1];
                out_region->alloc_ref->memory_properties = memory_properties;

                // Fix backwards reference in used_list elements
                for (u32 i = 0; i < sb_count(block->used_list); ++i)
                {
                    block->used_list[i].alloc_ref->memory_region = &block->used_list[i];
                }

                if (free_list_region->size == alloc_size)
                {
                    sb_del(block->free_list, free_list_index);
                }
                else
                {
                    // Modify Region In Place
                    free_list_region->size = free_list_region->size - new_region_size;
                    free_list_region->offset += new_region_size;
                }

                // return memory region we allocated
                return out_region->alloc_ref;
            }
        }
    }

    // If we've reached here, we need a new allocation

    u32 heap_index = context->vk_memory_properties.memoryTypes[memory_type_index].heapIndex;
    VkDeviceSize heap_size = context->vk_memory_properties.memoryHeaps[heap_index].size;
    VkDeviceSize allocation_size =
        MIN(heap_size / 2, GPU_BLOCK_SIZE); // Don't try to allocate entire heap if below the size of GPU_BLOCK_SIZE

    // FIXME: Need to get heap budget information, which is what we should use for heap_size above
    //           this is why our GPU_MEMORY_PROPERTY_DEVICE_LOCAL | GPU_MEMORY_PROPERTY_HOST_VISIBLE |
    //           GPU_MEMORY_PROPERTY_HOST_COHERENT allocation was failing

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = allocation_size,
        .memoryTypeIndex = memory_type_index,
    };

    VkDeviceMemory vk_memory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(context->device, &alloc_info, NULL, &vk_memory));

    gpu_memory_used += allocation_size;

    // Create a new block

    sb_push(memory_type->memory_blocks, ((GpuMemoryBlock){
                                            .size = alloc_info.allocationSize,
                                            .free_list = NULL,
                                            .used_list = NULL,
                                            .owning_type = memory_type,
                                            .vk_memory = vk_memory,
                                        }));

    // fix backwards-references in free/used lists of old blocks
    for (u32 block_idx = 0; block_idx < sb_count(memory_type->memory_blocks) - 1; ++block_idx)
    {
        GpuMemoryBlock *block = &memory_type->memory_blocks[block_idx];
        for (u32 free_idx = 0; free_idx < sb_count(block->free_list); ++free_idx)
        {
            block->free_list[free_idx].owning_block = block;
        }
        for (u32 used_idx = 0; used_idx < sb_count(block->used_list); ++used_idx)
        {
            block->used_list[used_idx].owning_block = block;
        }
    }

    GpuMemoryBlock *new_block = &memory_type->memory_blocks[sb_count(memory_type->memory_blocks) - 1];

    // FIXME: need to check if remaining size is zero (free_list region)
    sb_push(new_block->free_list, ((GpuMemoryRegion){
                                      .padding = 0,
                                      .offset = alloc_size,
                                      .size = alloc_info.allocationSize - alloc_size,
                                      .owning_block = new_block,
                                  }));

    sb_push(new_block->used_list, ((GpuMemoryRegion){
                                      .padding = 0,
                                      .offset = 0,
                                      .size = alloc_size,
                                      .owning_block = new_block,
                                      .alloc_ref = calloc(1, sizeof(GpuMemory)),
                                  }));

    *new_block->used_list[0].alloc_ref = (GpuMemory){
        .memory_region = &new_block->used_list[0],
        .memory_properties = memory_properties,
    };

    return new_block->used_list[0].alloc_ref;
}

void gpu_free_memory(GpuContext *context, GpuMemory *gpu_memory)
{
    // Add this memory's region as a new free list entry
    assert(gpu_memory);
    GpuMemoryRegion *memory_region = gpu_memory->memory_region;
    assert(memory_region);
    GpuMemoryBlock *owning_block = memory_region->owning_block;
    assert(owning_block);

	if (gpu_memory->is_mapped)
	{
		gpu_unmap_memory(context, gpu_memory);
	}

    // 1. Find where our element lies in used_list
    u32 used_list_index = memory_region - owning_block->used_list;
    assert(used_list_index < sb_count(owning_block->used_list));

    // 2. Find insertion point in free list (used_region->offset > current_free_region->offset)
    for (u32 free_list_index = 0; free_list_index < sb_count(owning_block->free_list); ++free_list_index)
    {
        GpuMemoryRegion *free_entry = &owning_block->free_list[free_list_index];
        if (memory_region->offset < free_entry->offset)
        {
            // Padding info isn't used once freed
            memory_region->offset -= memory_region->padding;
            memory_region->size += memory_region->padding;
            memory_region->padding = 0;
            // TODO: Check this padding logic

            const i32 previous_index = free_list_index - 1;
            GpuMemoryRegion *preceding_entry = previous_index >= 0 ? &owning_block->free_list[previous_index] : NULL;

            const bool merge_with_current = memory_region->offset + memory_region->size == free_entry->offset;
            const bool merge_with_preceding =
                preceding_entry && memory_region->offset == preceding_entry->offset + preceding_entry->size;
            const bool merge_with_both = merge_with_current && merge_with_preceding;

            // FIXME: TEST THESE CASES

            if (merge_with_both)
            {
                // Our newly freed region fits perfectly between two existing regions
                // Resize preceding entry's size and remove next free entry
                preceding_entry->size += memory_region->size + free_entry->size;
                sb_del(owning_block->free_list, free_list_index);
            }
            else if (merge_with_preceding)
            {
                // We can merge with the preceding entry, just add to size
                preceding_entry->size += memory_region->size;
            }
            else if (merge_with_current)
            {
                // We can merge with the next entry, change next region's offset and size
                free_entry->offset = memory_region->offset;
                free_entry->size += memory_region->size;
            }
            else
            {
                // We aren't directly adjacent to any existing free entries, create a new one
                sb_ins(owning_block->free_list, free_list_index, *memory_region);
            }

            // Finally, remove from used_list
            sb_del(owning_block->used_list, used_list_index);

            // Fix backwards reference in used_list elements
            for (u32 i = 0; i < sb_count(owning_block->used_list); ++i)
            {
                owning_block->used_list[i].alloc_ref->memory_region = &owning_block->used_list[i];
            }

            break;
        }
    }

    // If we freed the whole block, actually free the memory and remove the block
    if (sb_count(owning_block->used_list) == 0)
    {

        gpu_memory_used -= owning_block->size;

        vkFreeMemory(context->device, owning_block->vk_memory, NULL);
        GpuMemoryType *owning_type = owning_block->owning_type;
        assert(owning_type);
        u32 owning_block_index = owning_block - owning_type->memory_blocks;
        sb_del(owning_type->memory_blocks, owning_block_index);
    }
}

GpuBuffer gpu_create_buffer(GpuContext* context, GpuBufferCreateInfo* create_info)
{
	VkBufferCreateInfo buffer_create_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = create_info->size,
		.usage = create_info->usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VkBuffer vk_buffer;
	VK_CHECK(vkCreateBuffer(context->device, &buffer_create_info, NULL, &vk_buffer));

	VkMemoryRequirements memory_reqs;
	vkGetBufferMemoryRequirements(context->device, vk_buffer, &memory_reqs);
	GpuMemory *memory = gpu_allocate_memory(context, memory_reqs.memoryTypeBits, create_info->memory_properties, memory_reqs.size,
	memory_reqs.alignment);

	VK_CHECK(vkBindBufferMemory(context->device, vk_buffer, memory->memory_region->owning_block->vk_memory,
	memory->memory_region->offset));

	// set the name
	if (context->pfn_set_object_name != NULL)
	{
		VkDebugUtilsObjectNameInfoEXT vk_name_info = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = VK_OBJECT_TYPE_BUFFER,
			.objectHandle = (u64)vk_buffer, // this cast may vary by platform/compiler
			.pObjectName = create_info->debug_name,
		};
		context->pfn_set_object_name(context->device, &vk_name_info);
	}

	return (GpuBuffer){
		.vk_buffer = vk_buffer,
		.memory = memory,
	};

}

void gpu_destroy_buffer(GpuContext *context, GpuBuffer *buffer)
{
    if (buffer->vk_buffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(context->device, buffer->vk_buffer, NULL);
        gpu_free_memory(context, buffer->memory);
    }
}

void* gpu_map_buffer(GpuContext* context, GpuBuffer* buffer)
{
	void *pData;
	gpu_map_memory(context, buffer->memory, 0, buffer->memory->memory_region->size, &pData);
	return pData;
}

void gpu_unmap_buffer(GpuContext* context, GpuBuffer* buffer)
{
	gpu_unmap_memory(context, buffer->memory);
}

void gpu_memcpy(GpuContext *context, GpuMemory *memory, u64 upload_size, void *upload_data)
{
    void *pData;
    gpu_map_memory(context, memory, 0, upload_size, &pData);
    memcpy(pData, upload_data, upload_size);
    gpu_unmap_memory(context, memory);
}

void gpu_upload_buffer(GpuContext *context, GpuBuffer *buffer, u64 upload_size, void *upload_data)
{
    assert(upload_size <= buffer->memory->memory_region->size);
    if (buffer->memory->memory_properties & GPU_MEMORY_PROPERTY_HOST_VISIBLE)
    {
        gpu_memcpy(context, buffer->memory, upload_size, upload_data);
    }
    else
    {
		GpuBufferCreateInfo staging_buffer_create_info = {
			.size = upload_size,
			.usage = GPU_BUFFER_USAGE_TRANSFER_SRC,
			.memory_properties = GPU_MEMORY_PROPERTY_HOST_VISIBLE | GPU_MEMORY_PROPERTY_HOST_COHERENT,
			.debug_name = "gpu_upload_buffer staging buffer",
		};

		//TODO: reuse staging buffer, command buffer
        // Need staging buffer 
        GpuBuffer staging_buffer = gpu_create_buffer(context, &staging_buffer_create_info);
        gpu_memcpy(context, staging_buffer.memory, upload_size, upload_data);
        GpuCommandBuffer command_buffer = gpu_create_command_buffer(context);
        gpu_begin_command_buffer(&command_buffer);
        gpu_cmd_copy_buffer(&command_buffer, &staging_buffer, buffer, upload_size);
        gpu_end_command_buffer(&command_buffer);
        gpu_queue_submit(context, &command_buffer, NULL, NULL, NULL);
        gpu_wait_idle(context); // TODO: pass back complete semaphore
        gpu_destroy_buffer(context, &staging_buffer);
    }
}

void gpu_cmd_image_barrier(GpuCommandBuffer *command_buffer, GpuImageBarrier *image_barrier)
{
#if defined(ENABLE_VULKAN_SYNC2)
    VkImageMemoryBarrier2 vk_image_memory_barrier_2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = NULL,
        .srcStageMask = image_barrier->src_stage,
        .srcAccessMask = 0, // FCS TODO:
        .dstStageMask = image_barrier->dst_stage,
        .dstAccessMask = 0, // FCS TODO:
        .oldLayout = (VkImageLayout)image_barrier->old_layout,
        .newLayout = (VkImageLayout)image_barrier->new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image_barrier->image->vk_image,
        .subresourceRange =
            {
                // FCS TODO:
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VkDependencyInfo vk_dependency_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = NULL,
        .dependencyFlags = 0, // TODO:
        .memoryBarrierCount = 0,
        .pMemoryBarriers = NULL,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = NULL,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &vk_image_memory_barrier_2,
    };

    vkCmdPipelineBarrier2(command_buffer->vk_command_buffer, &vk_dependency_info);
#else
    // FCS TODO: vanilla vkCmdPipelineBarrier
    VkImageMemoryBarrier vk_image_memory_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = 0, // FCS TODO:
        .dstAccessMask = 0, // FCS TODO:
        .oldLayout = (VkImageLayout)image_barrier->old_layout,
        .newLayout = (VkImageLayout)image_barrier->new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image_barrier->image->vk_image,
        .subresourceRange =
            {
                // FCS TODO:
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vkCmdPipelineBarrier(command_buffer->vk_command_buffer, image_barrier->src_stage, image_barrier->dst_stage,
                         0,    // dependencyFlags
                         0,    // memoryBarrierCount
                         NULL, // pMemoryBarriers
                         0,    // bufferMemoryBarrierCount
                         NULL, // pBufferMemoryBarriers
                         1,    // imageMemoryBarrierCount
                         &vk_image_memory_barrier);
#endif
}

GpuImage gpu_create_image(GpuContext *context, GpuImageCreateInfo *create_info)
{

    VkImageType vk_image_type = VK_IMAGE_TYPE_1D;
    if (create_info->dimensions[2] > 1)
    {
        vk_image_type = VK_IMAGE_TYPE_3D;
    }
    else if (create_info->dimensions[1] > 1)
    {
        vk_image_type = VK_IMAGE_TYPE_2D;
    }

    VkImageCreateInfo vk_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .imageType = vk_image_type,
        .format = (VkFormat)create_info->format,
        .extent =
            {
                .width = create_info->dimensions[0],
                .height = create_info->dimensions[1],
                .depth = create_info->dimensions[2],
            },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = create_info->usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkImage vk_image = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(context->device, &vk_create_info, NULL, &vk_image));

    VkMemoryRequirements memory_reqs;
    vkGetImageMemoryRequirements(context->device, vk_image, &memory_reqs);
    GpuMemory *memory = gpu_allocate_memory(context, memory_reqs.memoryTypeBits, create_info->memory_properties,
                                            memory_reqs.size, memory_reqs.alignment);

    VK_CHECK(vkBindImageMemory(context->device, vk_image, memory->memory_region->owning_block->vk_memory,
                               memory->memory_region->offset));

    // set the name
    if (context->pfn_set_object_name != NULL)
    {
        VkDebugUtilsObjectNameInfoEXT vk_name_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = VK_OBJECT_TYPE_IMAGE,
            .objectHandle = (u64)vk_image, // this cast may vary by platform/compiler
            .pObjectName = create_info->debug_name,
        };
        context->pfn_set_object_name(context->device, &vk_name_info);
    }

    return (GpuImage){
        .vk_image = vk_image,
        .memory = memory,
        .format = create_info->format,
    };
}

void gpu_destroy_image(GpuContext *context, GpuImage *image)
{
    if (image->vk_image != VK_NULL_HANDLE)
    {
        vkDestroyImage(context->device, image->vk_image, NULL);
        gpu_free_memory(context, image->memory);
    }
}

void gpu_upload_image(GpuContext *context, GpuImage *image, u64 upload_width, u64 upload_height, void *upload_data)
{
    u64 upload_size = upload_width * upload_height * gpu_format_stride(image->format);
    if (image->memory->memory_properties & GPU_MEMORY_PROPERTY_HOST_VISIBLE)
    {
        gpu_memcpy(context, image->memory, upload_size, upload_data);
    }
    else
    {
		GpuBufferCreateInfo staging_buffer_create_info = {
			.size = upload_size,
			.usage = GPU_BUFFER_USAGE_TRANSFER_SRC,
			.memory_properties = GPU_MEMORY_PROPERTY_HOST_VISIBLE | GPU_MEMORY_PROPERTY_HOST_COHERENT,
			.debug_name = "gpu_upload_image staging buffer",
		};

        // Need staging buffer
        GpuBuffer staging_buffer = gpu_create_buffer(context, &staging_buffer_create_info);
        gpu_memcpy(context, staging_buffer.memory, upload_size, upload_data);
        GpuCommandBuffer command_buffer = gpu_create_command_buffer(context);
        gpu_begin_command_buffer(&command_buffer);
        gpu_cmd_image_barrier(&command_buffer, &(GpuImageBarrier){
                                                   .image = image,
                                                   .src_stage = GPU_PIPELINE_STAGE_TOP_OF_PIPE,
                                                   .dst_stage = GPU_PIPELINE_STAGE_BOTTOM_OF_PIPE,
                                                   .old_layout = GPU_IMAGE_LAYOUT_UNDEFINED,
                                                   .new_layout = GPU_IMAGE_LAYOUT_TRANSFER_DST,
                                               });
        gpu_cmd_copy_buffer_to_image(&command_buffer, &staging_buffer, image,
                                     GPU_IMAGE_LAYOUT_TRANSFER_DST /*FCS TODO: */, upload_width, upload_height);
        gpu_cmd_image_barrier(&command_buffer, &(GpuImageBarrier){
                                                   .image = image,
                                                   .src_stage = GPU_PIPELINE_STAGE_TOP_OF_PIPE,
                                                   .dst_stage = GPU_PIPELINE_STAGE_BOTTOM_OF_PIPE,
                                                   .old_layout = GPU_IMAGE_LAYOUT_TRANSFER_DST,
                                                   .new_layout = GPU_IMAGE_LAYOUT_SHADER_READ_ONLY,
                                               });
        gpu_end_command_buffer(&command_buffer);
        gpu_queue_submit(context, &command_buffer, NULL, NULL, NULL);
        gpu_wait_idle(context); // TODO: pass back complete semaphore
        gpu_destroy_buffer(context, &staging_buffer);
    }
}

GpuImageView gpu_create_image_view(GpuContext *context, GpuImageViewCreateInfo *create_info)
{
    VkImageViewCreateInfo vk_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .flags = 0,
        .pNext = NULL,
        .image = create_info->image->vk_image,
        .viewType = (VkImageViewType)create_info->type,
        .format = (VkFormat)create_info->format,
        .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
        .subresourceRange.aspectMask = create_info->aspect,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1,
    };

    VkImageView vk_image_view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(context->device, &vk_create_info, NULL, &vk_image_view));

    return (GpuImageView){
        .vk_image_view = vk_image_view,
    };
}

void gpu_destroy_image_view(GpuContext *context, GpuImageView *image_view)
{
    vkDestroyImageView(context->device, image_view->vk_image_view, NULL);
}

GpuSampler gpu_create_sampler(GpuContext *context, GpuSamplerCreateInfo *create_info)
{
    VkSamplerCreateInfo vk_create_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .magFilter = (VkFilter)create_info->mag_filter,
        .minFilter = (VkFilter)create_info->min_filter,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .mipLodBias = 0.0f,
        .anisotropyEnable = create_info->max_anisotropy != 0,
        .maxAnisotropy = (float)create_info->max_anisotropy,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_NEVER,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    VkSampler vk_sampler = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSampler(context->device, &vk_create_info, NULL, &vk_sampler));

    return (GpuSampler){
        .vk_sampler = vk_sampler,
    };
}

void gpu_destroy_sampler(GpuContext *context, GpuSampler *sampler)
{
    vkDestroySampler(context->device, sampler->vk_sampler, NULL);
}

GpuDescriptorSet gpu_create_descriptor_set(GpuContext* context, const GpuDescriptorLayout* descriptor_layout)
{
	VkDescriptorPoolSize vk_pool_sizes[descriptor_layout->binding_count];
	for (u32 i = 0; i < descriptor_layout->binding_count; ++i)
	{
		vk_pool_sizes[i].type = (VkDescriptorType)descriptor_layout->bindings[i].type;
		vk_pool_sizes[i].descriptorCount = 1;
	}

	VkDescriptorPoolCreateInfo vk_descriptor_pool_create_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.maxSets = 1,
		.poolSizeCount = descriptor_layout->binding_count,
		.pPoolSizes = vk_pool_sizes,
	};

	VkDescriptorPool vk_descriptor_pool = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDescriptorPool(context->device, &vk_descriptor_pool_create_info, NULL, &vk_descriptor_pool));

	VkDescriptorSetAllocateInfo vk_descriptor_set_alloc_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = NULL,
		.descriptorPool = vk_descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &descriptor_layout->vk_descriptor_set_layout,
	};
	VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;
	VK_CHECK(vkAllocateDescriptorSets(context->device, &vk_descriptor_set_alloc_info, &vk_descriptor_set));

	return (GpuDescriptorSet){
		.vk_descriptor_pool = vk_descriptor_pool,
		.vk_descriptor_set = vk_descriptor_set,
		.set_number = descriptor_layout->set_number,
	};
}

void gpu_destroy_descriptor_set(GpuContext *context, GpuDescriptorSet *descriptor_set)
{
    VK_CHECK(vkResetDescriptorPool(context->device, descriptor_set->vk_descriptor_pool, 0));
    vkDestroyDescriptorPool(context->device, descriptor_set->vk_descriptor_pool, NULL);
}

void gpu_write_descriptor_set(GpuContext *context, GpuDescriptorSet *descriptor_set, u32 write_count,
                              GpuDescriptorWrite *descriptor_writes)
{
	VkDescriptorImageInfo vk_descriptor_image_infos[write_count];
	VkDescriptorBufferInfo vk_descriptor_buffer_infos[write_count];
    VkWriteDescriptorSet vk_descriptor_writes[write_count];
    
	for (u32 i = 0; i < write_count; ++i)
    {
		if (descriptor_writes[i].image_write)
		{
			vk_descriptor_image_infos[i] = (VkDescriptorImageInfo) {
				.sampler = descriptor_writes[i].image_write->sampler->vk_sampler,
			    .imageView = descriptor_writes[i].image_write->image_view->vk_image_view,
			    .imageLayout = (VkImageLayout) descriptor_writes[i].image_write->layout,
			};
		}

		if (descriptor_writes[i].buffer_write)
		{
			vk_descriptor_buffer_infos[i] = (VkDescriptorBufferInfo) {
				.buffer = descriptor_writes[i].buffer_write->buffer->vk_buffer,
				.offset = descriptor_writes[i].buffer_write->offset,
				.range  = descriptor_writes[i].buffer_write->range,
			};
		}

        vk_descriptor_writes[i] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet = descriptor_set->vk_descriptor_set,
            .dstBinding = descriptor_writes[i].binding_desc->binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = (VkDescriptorType)descriptor_writes[i].binding_desc->type,
            .pImageInfo = descriptor_writes[i].image_write ? &vk_descriptor_image_infos[i] : NULL,
            .pBufferInfo = descriptor_writes[i].buffer_write ? &vk_descriptor_buffer_infos[i] : NULL,
            .pTexelBufferView = NULL,
        };
    }

    vkUpdateDescriptorSets(context->device, write_count, vk_descriptor_writes, 0, NULL);
}

void gpu_map_memory(GpuContext *context, GpuMemory *memory, u64 offset, u64 size, void **ppData)
{
	assert(!memory->is_mapped);

	const bool is_block_unmapped = memory->memory_region->owning_block->mapped_ref_count == 0;
	if (is_block_unmapped)
	{
		VK_CHECK(vkMapMemory(
			context->device, 
			memory->memory_region->owning_block->vk_memory,
			0, 
			memory->memory_region->owning_block->size, 
			0, 
			&memory->memory_region->owning_block->mapped_ptr
		));
	}

	memory->is_mapped = true;
	memory->memory_region->owning_block->mapped_ref_count++;
	*ppData = (char*) memory->memory_region->owning_block->mapped_ptr + memory->memory_region->offset + offset;
}

void gpu_unmap_memory(GpuContext *context, GpuMemory *memory)
{
	assert(memory->is_mapped);
	assert(memory->memory_region->owning_block->mapped_ref_count > 0);

	memory->is_mapped = false;
	memory->memory_region->owning_block->mapped_ref_count--;

	const bool needs_unmap = memory->memory_region->owning_block->mapped_ref_count == 0;
	if (needs_unmap)
	{
    	vkUnmapMemory(context->device, memory->memory_region->owning_block->vk_memory);
	}
}

GpuShaderModule gpu_create_shader_module(GpuContext *context, u64 code_size, const u32 *code)
{
    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .codeSize = code_size,
        .pCode = code,
    };

    VkShaderModule vk_shader_module;
    VK_CHECK(vkCreateShaderModule(context->device, &create_info, NULL, &vk_shader_module));
    return (GpuShaderModule){
        .vk_shader_module = vk_shader_module,
    };
}

void gpu_destroy_shader_module(GpuContext *context, GpuShaderModule *shader_module)
{
    vkDestroyShaderModule(context->device, shader_module->vk_shader_module, NULL);
}

GpuRenderPass gpu_create_render_pass(GpuContext *context, u32 color_attachment_count,
                                     GpuAttachmentDesc *color_attachments, GpuAttachmentDesc *depth_stencil_attachment)
{

    u32 depth_attachment_count = depth_stencil_attachment != NULL ? 1 : 0;

    VkAttachmentDescription vk_attachments[color_attachment_count + depth_attachment_count];
    VkAttachmentReference vk_attachment_refs[color_attachment_count + depth_attachment_count];

    // Order of attachments/refs: Color Attachments, then Depth Attachment

    for (u32 i = 0; i < color_attachment_count + depth_attachment_count; ++i)
    {

        // Whether we are currently building a color attachment or the depth-stencil attachment
        bool is_color_attachment = i < color_attachment_count;
        GpuAttachmentDesc *attachment = is_color_attachment ? &color_attachments[i] : depth_stencil_attachment;

        vk_attachments[i] = (VkAttachmentDescription){
            .flags = 0,
            .format = (VkFormat)attachment->format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = (VkAttachmentLoadOp)attachment->load_op,
            .storeOp = (VkAttachmentStoreOp)attachment->store_op,
            .stencilLoadOp = (VkAttachmentLoadOp)GPU_LOAD_OP_DONT_CARE,
            .stencilStoreOp = (VkAttachmentStoreOp)GPU_STORE_OP_DONT_CARE,
            .initialLayout = (VkImageLayout)attachment->initial_layout,
            .finalLayout = (VkImageLayout)attachment->final_layout,
        };

        vk_attachment_refs[i] = (VkAttachmentReference){
            .attachment = i,
            .layout = (VkImageLayout)(is_color_attachment ? GPU_IMAGE_LAYOUT_COLOR_ATACHMENT
                                                          : GPU_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT),
        };
    }

    VkSubpassDescription subpass = {
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = NULL,
        .colorAttachmentCount = color_attachment_count,
        .pColorAttachments = color_attachment_count > 0 ? vk_attachment_refs : NULL,
        .pResolveAttachments = NULL,
        .pDepthStencilAttachment = depth_attachment_count > 0 ? &vk_attachment_refs[color_attachment_count] : NULL,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = NULL,
    };

    VkRenderPassCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .attachmentCount = color_attachment_count + depth_attachment_count,
        .pAttachments = vk_attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 0,
        .pDependencies = NULL,
    };

    VkRenderPass vk_render_pass;
    VK_CHECK(vkCreateRenderPass(context->device, &create_info, NULL, &vk_render_pass));

    return (GpuRenderPass){
        .vk_render_pass = vk_render_pass,
    };
}

void gpu_destroy_render_pass(GpuContext *context, GpuRenderPass *render_pass)
{
    vkDestroyRenderPass(context->device, render_pass->vk_render_pass, NULL);
}

GpuDescriptorLayout gpu_create_descriptor_layout(GpuContext* context, const GpuDescriptorLayoutCreateInfo* create_info)
{
	VkDescriptorSetLayoutBinding vk_bindings[create_info->binding_count];
	for (int i = 0; i < create_info->binding_count; ++i)
	{
		vk_bindings[i].binding = create_info->bindings[i].binding;
		vk_bindings[i].descriptorType = (VkDescriptorType)create_info->bindings[i].type;
		vk_bindings[i].descriptorCount = 1;
		vk_bindings[i].stageFlags = create_info->bindings[i].stage_flags;
		vk_bindings[i].pImmutableSamplers = NULL;
	}

	VkDescriptorSetLayoutCreateInfo vk_desc_layout_create_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.bindingCount = create_info->binding_count,
		.pBindings = vk_bindings,
	};

	VkDescriptorSetLayout vk_descriptor_set_layout = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDescriptorSetLayout(context->device, &vk_desc_layout_create_info, NULL, &vk_descriptor_set_layout));

	u64 bindings_size = sizeof(GpuDescriptorBinding) * create_info->binding_count;
	GpuDescriptorBinding* bindings = malloc(bindings_size);	
	memcpy(bindings, create_info->bindings, bindings_size);

	return (GpuDescriptorLayout) {
		.set_number = create_info->set_number,
		.binding_count = create_info->binding_count,
		.bindings = bindings,
		.vk_descriptor_set_layout = vk_descriptor_set_layout,
	};
}

void gpu_destroy_descriptor_layout(GpuContext* context, const GpuDescriptorLayout* descriptor_layout)
{
	free(descriptor_layout->bindings);
	vkDestroyDescriptorSetLayout(context->device, descriptor_layout->vk_descriptor_set_layout, NULL);
}

GpuPipelineLayout gpu_create_pipeline_layout(GpuContext* context, const i32 num_descriptor_layouts, const GpuDescriptorLayout* descriptor_layouts)
{
	VkDescriptorSetLayout vk_descriptor_set_layouts[num_descriptor_layouts];
	for (i32 i = 0; i < num_descriptor_layouts; ++i)
	{
		vk_descriptor_set_layouts[i] = descriptor_layouts[i].vk_descriptor_set_layout;
	}

    VkPipelineLayoutCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = num_descriptor_layouts,
        .pSetLayouts = vk_descriptor_set_layouts,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL,
    };

    VkPipelineLayout vk_pipeline_layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(context->device, &create_info, NULL, &vk_pipeline_layout));

    return (GpuPipelineLayout){
		.vk_pipeline_layout = vk_pipeline_layout,
	};
}

void gpu_destroy_pipeline_layout(GpuContext *context, GpuPipelineLayout *layout)
{
    vkDestroyPipelineLayout(context->device, layout->vk_pipeline_layout, NULL);
}

GpuPipeline gpu_create_graphics_pipeline(GpuContext *context, GpuGraphicsPipelineCreateInfo *create_info)
{
    VkPipelineShaderStageCreateInfo shader_stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = create_info->vertex_module->vk_shader_module,
            .pName = "main",
            .pSpecializationInfo = NULL,
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = create_info->fragment_module->vk_shader_module,
            .pName = "main",
            .pSpecializationInfo = NULL,
        }};

    const u32 binding = 0; // Only using binding 0 for now
    u32 location = 0;      // Locations are provided in-order from attributes
    u32 total_stride = 0;

    VkVertexInputAttributeDescription attribute_descriptions[create_info->num_attributes];
    for (int i = 0; i < create_info->num_attributes; ++i)
    {
        attribute_descriptions[i].binding = binding;
        attribute_descriptions[i].location = location++;
        attribute_descriptions[i].format = (VkFormat)create_info->attribute_formats[i];
        attribute_descriptions[i].offset = total_stride;

        total_stride += gpu_format_stride(create_info->attribute_formats[i]);
    }

    VkVertexInputBindingDescription binding_descriptions[] = {
        {
            .binding = 0,
            .stride = total_stride,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
    };

    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = binding_descriptions,
        .vertexAttributeDescriptionCount = create_info->num_attributes,
        .pVertexAttributeDescriptions = attribute_descriptions,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembler = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = NULL, // Viewport is dynamic (gpu_cmd_set_viewport)
        .scissorCount = 1,
        .pScissors = NULL, // Scissor is dynamic (gpu_cmd_set_viewport)
    };

    VkPipelineRasterizationStateCreateInfo rasterization = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = (VkPolygonMode) create_info->rasterizer.polygon_mode,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .pSampleMask = NULL,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthTestEnable = create_info->depth_stencil.depth_test,
        .depthWriteEnable = create_info->depth_stencil.depth_write,
        .depthCompareOp = VK_COMPARE_OP_LESS, // TODO: add to create info
        .depthBoundsTestEnable = VK_FALSE,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
    };

    // FIXME: build up based on renderpass attachments
    VkPipelineColorBlendAttachmentState color_blending_attachments[] = {{
        .blendEnable = create_info->enable_color_blending ? VK_TRUE : VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    }};

    VkPipelineColorBlendStateCreateInfo color_blending = {.sType =
                                                              VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                          .pNext = NULL,
                                                          .flags = 0,
                                                          .logicOpEnable = VK_FALSE,
                                                          .logicOp = VK_LOGIC_OP_COPY,
                                                          .attachmentCount = 1, // TODO: based on num color attachments
                                                          .pAttachments = color_blending_attachments,
                                                          .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}};

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .dynamicStateCount = sizeof(dynamic_states) / sizeof(VkDynamicState),
        .pDynamicStates = dynamic_states,
    };

    VkPipelineRenderingCreateInfo vk_rendering_create_info = {};
    if (create_info->rendering_info)
    {
        vk_rendering_create_info = (VkPipelineRenderingCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .pNext = NULL,
            .viewMask = 0,
            .colorAttachmentCount = create_info->rendering_info->color_attachment_count,
            .pColorAttachmentFormats = (VkFormat *)create_info->rendering_info->color_formats,
            .depthAttachmentFormat = (VkFormat)create_info->rendering_info->depth_format,
            .stencilAttachmentFormat = (VkFormat)create_info->rendering_info->stencil_format,
        };
    }

    VkGraphicsPipelineCreateInfo vk_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = create_info->rendering_info ? &vk_rendering_create_info : NULL,
        .flags = 0,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembler,
        .pTessellationState = NULL,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state_create_info,
        .layout = create_info->layout->vk_pipeline_layout,
        .renderPass = create_info->render_pass ? create_info->render_pass->vk_render_pass : VK_NULL_HANDLE,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };

    VkPipeline vk_pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(context->device, VK_NULL_HANDLE, 1, &vk_create_info, NULL, &vk_pipeline));

    return (GpuPipeline){
        .vk_pipeline = vk_pipeline,
    };
}

void gpu_destroy_pipeline(GpuContext *context, GpuPipeline *pipeline)
{
    vkDestroyPipeline(context->device, pipeline->vk_pipeline, NULL);
}

GpuFramebuffer gpu_create_framebuffer(GpuContext *context, GpuFramebufferCreateInfo *create_info)
{

    VkFramebufferCreateInfo vk_create_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = create_info->render_pass->vk_render_pass,
        .attachmentCount = create_info->attachment_count,
        .pAttachments = (VkImageView *)create_info->attachments,
        .width = create_info->width,
        .height = create_info->height,
        .layers = 1,
    };

    VkFramebuffer vk_framebuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFramebuffer(context->device, &vk_create_info, NULL, &vk_framebuffer));

    return (GpuFramebuffer){
        .vk_framebuffer = vk_framebuffer,
        .width = create_info->width,
        .height = create_info->height,
    };
}

void gpu_destroy_framebuffer(GpuContext *context, GpuFramebuffer *framebuffer)
{
    vkDestroyFramebuffer(context->device, framebuffer->vk_framebuffer, NULL);
}

GpuCommandBuffer gpu_create_command_buffer(GpuContext *context)
{
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = context->graphics_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer vk_command_buffer;
    VK_CHECK(vkAllocateCommandBuffers(context->device, &alloc_info, &vk_command_buffer));

    return (GpuCommandBuffer){
        .vk_command_buffer = vk_command_buffer,
    };
}

void gpu_free_command_buffer(GpuContext *context, GpuCommandBuffer *command_buffer)
{
    vkFreeCommandBuffers(context->device, context->graphics_command_pool, 1, &command_buffer->vk_command_buffer);
}

void gpu_begin_command_buffer(GpuCommandBuffer *command_buffer)
{
    VK_CHECK(vkBeginCommandBuffer(command_buffer->vk_command_buffer,
                                  &(VkCommandBufferBeginInfo){
                                      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                      .pNext = NULL,
                                      .flags = 0,
                                      .pInheritanceInfo = NULL,
                                  }));
}

void gpu_end_command_buffer(GpuCommandBuffer *command_buffer)
{
    VK_CHECK(vkEndCommandBuffer(command_buffer->vk_command_buffer));
}

// helper to convert to VkRenderingAttachmentInfo
VkRenderingAttachmentInfo to_vk_attachment_info(GpuRenderingAttachmentInfo *attachment_info)
{
    assert(attachment_info);
    // TODO: Can we infer image_layout from just if its color/depth/stencil (remove from info struct?)

    VkRenderingAttachmentInfo vk_rendering_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = NULL,
        .imageView = attachment_info->image_view->vk_image_view,
        .imageLayout = (VkImageLayout)attachment_info->image_layout,
        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = 0,
        .loadOp = (VkAttachmentLoadOp)attachment_info->load_op,
        .storeOp = (VkAttachmentStoreOp)attachment_info->store_op,
        // .clearValue = (VkClearValue) attachment_info->clear_value, //Below
    };

    memcpy(&vk_rendering_attachment_info.clearValue, &attachment_info->clear_value, sizeof(VkClearValue));

    return vk_rendering_attachment_info;
}

void gpu_cmd_begin_rendering(GpuCommandBuffer *command_buffer, GpuRenderingInfo *rendering_info)
{

    VkRenderingAttachmentInfo vk_color_attachments[rendering_info->color_attachment_count];
    for (u32 i = 0; i < rendering_info->color_attachment_count; ++i)
    {
        vk_color_attachments[i] = to_vk_attachment_info(&rendering_info->color_attachments[i]);
    }

    VkRenderingAttachmentInfo vk_depth_attachment;
    if (rendering_info->depth_attachment)
    {
        vk_depth_attachment = to_vk_attachment_info(rendering_info->depth_attachment);
    }

    VkRenderingAttachmentInfo vk_stencil_attachment;
    if (rendering_info->stencil_attachment)
    {
        vk_stencil_attachment = to_vk_attachment_info(rendering_info->stencil_attachment);
    }

    VkRenderingInfo vk_rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderArea =
            {
                .offset =
                    {
                        .x = 0,
                        .y = 0,
                    },
                .extent =
                    {
                        .width = rendering_info->render_width,
                        .height = rendering_info->render_height,
                    },
            },
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = rendering_info->color_attachment_count,
        .pColorAttachments = vk_color_attachments,
        .pDepthAttachment = rendering_info->depth_attachment ? &vk_depth_attachment : NULL,
        .pStencilAttachment = rendering_info->stencil_attachment ? &vk_stencil_attachment : NULL,
    };

    // vkCmdBeginRendering(command_buffer->vk_command_buffer, &vk_rendering_info);
    pfn_begin_rendering(command_buffer->vk_command_buffer, &vk_rendering_info);
}

void gpu_cmd_end_rendering(GpuCommandBuffer *command_buffer)
{
    // vkCmdEndRendering(command_buffer->vk_command_buffer);
    pfn_end_rendering(command_buffer->vk_command_buffer);
}

void gpu_cmd_begin_render_pass(GpuCommandBuffer *command_buffer, GpuRenderPassBeginInfo *begin_info)
{
    VkRenderPassBeginInfo vk_begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .renderPass = begin_info->render_pass->vk_render_pass,
        .framebuffer = begin_info->framebuffer->vk_framebuffer,
        .renderArea =
            {
                .offset =
                    {
                        .x = 0,
                        .y = 0,
                    },
                .extent =
                    {
                        .width = begin_info->framebuffer->width,
                        .height = begin_info->framebuffer->height,
                    },
            },
        .clearValueCount = begin_info->num_clear_values,
        .pClearValues = (VkClearValue *)begin_info->clear_values,
    };

    vkCmdBeginRenderPass(command_buffer->vk_command_buffer, &vk_begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void gpu_cmd_end_render_pass(GpuCommandBuffer *command_buffer)
{
    vkCmdEndRenderPass(command_buffer->vk_command_buffer);
}

void gpu_cmd_bind_pipeline(GpuCommandBuffer *command_buffer, GpuPipeline *pipeline)
{
    // TODO: Also Allow binding of compute pipelines
    vkCmdBindPipeline(command_buffer->vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->vk_pipeline);
}

void gpu_cmd_bind_index_buffer(GpuCommandBuffer *command_buffer, GpuBuffer *index_buffer)
{
    vkCmdBindIndexBuffer(command_buffer->vk_command_buffer, index_buffer->vk_buffer, 0, VK_INDEX_TYPE_UINT32);
}

void gpu_cmd_bind_descriptor_set(GpuCommandBuffer *command_buffer, GpuPipelineLayout *layout, GpuDescriptorSet *descriptor_set)
{
    vkCmdBindDescriptorSets(
		command_buffer->vk_command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		layout->vk_pipeline_layout, 
		descriptor_set->set_number, 
		1, 
		&descriptor_set->vk_descriptor_set, 
		0,
		NULL
	);
}

void gpu_cmd_bind_vertex_buffer(GpuCommandBuffer *command_buffer, GpuBuffer *vertex_buffer)
{
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(command_buffer->vk_command_buffer, 0, 1, &vertex_buffer->vk_buffer, offsets);
}

void gpu_cmd_draw_indexed(GpuCommandBuffer *command_buffer, u32 index_count, u32 instance_count)
{
    vkCmdDrawIndexed(command_buffer->vk_command_buffer, index_count, instance_count, 0, 0, 0);
}

void gpu_cmd_draw(GpuCommandBuffer *command_buffer, u32 vertex_count, u32 instance_count)
{
    vkCmdDraw(command_buffer->vk_command_buffer, vertex_count, instance_count, 0, 0);
}

void gpu_cmd_set_viewport(GpuCommandBuffer *command_buffer, GpuViewport *viewport)
{
    vkCmdSetViewport(command_buffer->vk_command_buffer, 0, 1, (VkViewport *)viewport);
    vkCmdSetScissor(command_buffer->vk_command_buffer, 0, 1,
                    &(VkRect2D){
                        .offset =
                            {
                                .x = viewport->x,
                                .y = viewport->y,
                            },
                        .extent =
                            {
                                .width = (u32)viewport->width,
                                .height = (u32)viewport->height,
                            },
                    });
}

void gpu_cmd_copy_buffer(GpuCommandBuffer *command_buffer, GpuBuffer *src_buffer, GpuBuffer *dst_buffer, u64 size)
{
    VkBufferCopy vk_buffer_copy = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size,
    };
    vkCmdCopyBuffer(command_buffer->vk_command_buffer, src_buffer->vk_buffer, dst_buffer->vk_buffer, 1,
                    &vk_buffer_copy);
}

void gpu_cmd_copy_buffer_to_image(GpuCommandBuffer *command_buffer, GpuBuffer *src_buffer, GpuImage *dst_image,
                                  GpuImageLayout image_layout, u64 width, u64 height)
{
    VkBufferImageCopy vk_buffer_image_copy = {.bufferOffset = 0,
                                              .bufferRowLength = 0,
                                              .bufferImageHeight = 0,
                                              .imageSubresource =
                                                  (VkImageSubresourceLayers){
                                                      .aspectMask = GPU_IMAGE_ASPECT_COLOR, // TODO:
                                                      .mipLevel = 0,                        // TODO:
                                                      .baseArrayLayer = 0,
                                                      .layerCount = 1,
                                                  },
                                              .imageOffset = {.x = 0, .y = 0, .z = 0},
                                              .imageExtent = {
                                                  .width = width,
                                                  .height = height,
                                                  .depth = 1,
                                              }};

    // FCS TODO: dstImageLayout must specify the layout of the image subresources of dstImage specified in pRegions at
    // the time this command is executed on a VkDevice
    vkCmdCopyBufferToImage(command_buffer->vk_command_buffer, src_buffer->vk_buffer, dst_image->vk_image,
                           (VkImageLayout)image_layout, 1, &vk_buffer_image_copy);
}

void gpu_queue_submit(GpuContext *context, GpuCommandBuffer *command_buffer, GpuSemaphore *wait_semaphore,
                      GpuSemaphore *signal_semaphore, GpuFence *signal_fence)
{

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo vk_submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = wait_semaphore ? 1 : 0,
        .pWaitSemaphores = wait_semaphore ? &wait_semaphore->vk_semaphore : NULL,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer->vk_command_buffer,
        .signalSemaphoreCount = signal_semaphore != NULL ? 1 : 0,
        .pSignalSemaphores = signal_semaphore ? &signal_semaphore->vk_semaphore : NULL,
    };

    VK_CHECK(vkQueueSubmit(context->graphics_queue, 1, &vk_submit_info,
                           signal_fence ? signal_fence->vk_fence : VK_NULL_HANDLE));
}

void gpu_queue_present(GpuContext *context, u32 image_index, GpuSemaphore *wait_semaphore)
{
    VkPresentInfoKHR vk_present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = wait_semaphore ? 1 : 0,
        .pWaitSemaphores = wait_semaphore ? &wait_semaphore->vk_semaphore : NULL,
        .swapchainCount = 1,
        .pSwapchains = &context->swapchain,
        .pImageIndices = &image_index,
        .pResults = NULL,
    };

    // FCS TODO: MoltenVk returning VK_SUBOPTIMAL_KHR, likely due to invalid swapchain size (need to implement window
    // functions)
    /*VK_CHECK*/ (vkQueuePresentKHR(context->graphics_queue, &vk_present_info));
}

GpuFence gpu_create_fence(GpuContext *context, bool signaled)
{
    VkFenceCreateInfo vk_create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0,
    };

    VkFence vk_fence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(context->device, &vk_create_info, NULL, &vk_fence));

    return (GpuFence){
        .vk_fence = vk_fence,
    };
}

void gpu_destroy_fence(GpuContext *context, GpuFence *fence)
{
    vkDestroyFence(context->device, fence->vk_fence, NULL);
}

void gpu_wait_for_fence(GpuContext *context, GpuFence *fence)
{
    VK_CHECK(vkWaitForFences(context->device, 1, &fence->vk_fence, VK_TRUE, UINT64_MAX));
}

void gpu_reset_fence(GpuContext *context, GpuFence *fence)
{
    VK_CHECK(vkResetFences(context->device, 1, &fence->vk_fence));
}

GpuSemaphore gpu_create_semaphore(GpuContext *context)
{
    VkSemaphoreCreateInfo vk_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = NULL, .flags = 0};

    VkSemaphore vk_semaphore;
    VK_CHECK(vkCreateSemaphore(context->device, &vk_create_info, NULL, &vk_semaphore));

    return (GpuSemaphore){
        .vk_semaphore = vk_semaphore,
    };
}

void gpu_destroy_semaphore(GpuContext *context, GpuSemaphore *semaphore)
{
    vkDestroySemaphore(context->device, semaphore->vk_semaphore, NULL);
}

static bool gpu_read_file(const char *filename, size_t *out_file_size, u32 **out_data)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
	{
        return false;
	}
	
    fseek(file, 0L, SEEK_END);
    const size_t file_size = *out_file_size = ftell(file);
    rewind(file);

    *out_data = calloc(1, file_size + 1);
    if (!*out_data)
    {
        fclose(file);
        return false;
    }

    if (fread(*out_data, 1, file_size, file) != file_size)
    {
        fclose(file);
        free(*out_data);
        return false;
    }

    fclose(file);
    return true;
}

GpuShaderModule gpu_create_shader_module_from_file(GpuContext *gpu_context, const char *filename)
{
    size_t shader_size = 0;
    u32 *shader_code = NULL;
    if (!gpu_read_file(filename, &shader_size, &shader_code))
    {
        exit(1);
    }

    GpuShaderModule module = gpu_create_shader_module(gpu_context, shader_size, shader_code);
    free(shader_code);
    return module;
}
