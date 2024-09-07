
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#endif

#include <vulkan/vulkan.h>
#include "stretchy_buffer.h"
#include "math/basic_math.h"

PFN_vkCmdBeginRenderingKHR pfn_vk_begin_rendering = NULL;
PFN_vkCmdEndRenderingKHR pfn_vk_end_rendering = NULL;

typedef struct Gpu2VulkanPhysicalDeviceData
{
    VkPhysicalDevice physical_device;
    i32 graphics_family_index;
    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;
} Gpu2VulkanPhysicalDeviceData;

typedef struct Gpu2MemoryType
{
    sbuffer(struct Gpu2MemoryBlock) memory_blocks;
} Gpu2MemoryType;

typedef struct Gpu2MemoryBlock
{
    u64 size;
    sbuffer(struct Gpu2MemoryRegion) free_list;
    sbuffer(struct Gpu2MemoryRegion) used_list;
    struct Gpu2MemoryType* owning_type;
    VkDeviceMemory vk_memory;

	i32 mapped_ref_count;
	void* mapped_ptr;
} Gpu2MemoryBlock;

typedef struct Gpu2MemoryRegion
{
    u64 padding; // Padding on front of region (for alignment requirement)
    u64 offset;
    u64 size;
    struct Gpu2MemoryBlock* owning_block;
    struct Gpu2Memory* alloc_ref; // Null in free list, non-null in used_list
} Gpu2MemoryRegion;

typedef struct Gpu2Memory
{
    Gpu2MemoryRegion* memory_region;
    VkMemoryPropertyFlagBits memory_properties;
	bool is_mapped;
} Gpu2Memory;

struct Gpu2Device
{
	// Main Vulkan Objects
    VkInstance vk_instance;
    VkPhysicalDevice vk_physical_device;
    VkDevice vk_device;

	// Queue
    VkQueue graphics_queue;
    VkCommandPool graphics_command_pool;

    // Debug Functionality
    VkDebugUtilsMessengerEXT debug_messenger;
    PFN_vkSetDebugUtilsObjectNameEXT pfn_set_object_name;

	// Surface + Swapchain
    VkSurfaceKHR surface;
    VkPresentModeKHR present_mode;
    VkSurfaceFormatKHR surface_format;
    VkSwapchainKHR swapchain;
	VkExtent2D swapchain_extent;

	// Swapchain Image Data
	u32 current_image_index;
    u32 swapchain_image_count;
	Gpu2Texture* swapchain_images;

    // Memory
    int num_memory_types;
    Gpu2MemoryType* memory_types;
    VkPhysicalDeviceMemoryProperties vk_memory_properties;

	// Pending Present
	bool has_pending_present_info;
	VkPresentInfoKHR pending_vk_present_info;

};

struct Gpu2Shader
{
	VkShaderModule vk_shader_module;
};

struct Gpu2BindGroupLayout
{
	u32 index;
	u32 num_bindings;
	Gpu2ResourceBinding bindings[GPU2_BIND_GROUP_MAX_BINDINGS];

	VkDescriptorSetLayout vk_descriptor_set_layout;
};

struct Gpu2BindGroup
{
	Gpu2BindGroupLayout layout;
	VkDescriptorPool vk_descriptor_pool;
	VkDescriptorSet vk_descriptor_set;
};

struct Gpu2RenderPipeline
{
	VkPipeline vk_pipeline;
	VkPipelineLayout vk_pipeline_layout;
};

struct Gpu2Buffer
{
	VkBuffer vk_buffer;	
	Gpu2Memory* memory;
};

struct Gpu2Texture
{
	VkImage vk_image;
	VkImageView vk_image_view;
	VkFormat vk_format;

	// Null memory implies externally managed
	Gpu2Memory* memory;
};

struct Gpu2CommandBuffer
{
	VkCommandBuffer vk_command_buffer;
};

struct Gpu2RenderPass
{
	VkCommandBuffer vk_command_buffer;
};

struct Gpu2Drawable
{
	Gpu2Texture texture;
};

#define VK_CHECK(f)                                                                 \
{                                                                                   \
	VkResult res = (f);                                                             \
	if (res != VK_SUCCESS)                                                          \
	{                                                                               \
		printf("VULKAN ERROR: %s RETURNED: %i LINE: %i\n", #f, res, __LINE__);      \
		exit(1);                                                                    \
	}                                                                               \
}

VkBool32 gpu2_vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                               		VkDebugUtilsMessageTypeFlagsEXT messageType,
                               		const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, 
									void *pUserData)
{
    if (!(messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT))
    {
        printf("\nValidation Layer: %s\n", pCallbackData->pMessage);
    }
    return VK_FALSE;
}

Gpu2VulkanPhysicalDeviceData gpu2_vulkan_choose_physical_device(VkInstance vk_instance, VkSurfaceKHR vk_surface)
{
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    i32 graphics_family_index = -1;
    VkSurfaceFormatKHR surface_format = {};
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

    u32 physical_device_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, NULL));
    if (physical_device_count > 0)
    {
        VkPhysicalDevice physical_devices[physical_device_count];
        VK_CHECK(vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, physical_devices));
        for (i32 i = 0; i < physical_device_count; ++i)
        {
            u32 format_count;
            VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_devices[i], vk_surface, &format_count, NULL));
            bool found_format = false;
            if (format_count > 0)
            {
                VkSurfaceFormatKHR surface_formats[format_count];

                VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_devices[i], vk_surface, &format_count, surface_formats));
                for (i32 i = 0; i < format_count; ++i)
                {
                    printf("format: %i\n", surface_formats[i].format);
                    if (surface_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
                    {
                        surface_format = surface_formats[i];
                        found_format = true;
                        break;
                    }
                }
            }

            assert(found_format);

            u32 present_mode_count;
            VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_devices[i], vk_surface, &present_mode_count, NULL));
            if (present_mode_count > 0)
            {
                VkPresentModeKHR present_modes[present_mode_count];
                VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_devices[i], vk_surface, &present_mode_count,
                                                                   present_modes));

                for (i32 i = 0; i < present_mode_count; ++i)
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

                for (i32 i = 0; i < queue_family_count; ++i)
                {
                    VkBool32 surface_supported = VK_FALSE;
                    vkGetPhysicalDeviceSurfaceSupportKHR(physical_devices[i], i, vk_surface, &surface_supported);

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

    return (Gpu2VulkanPhysicalDeviceData){
        .physical_device = physical_device,
        .graphics_family_index = graphics_family_index,
        .surface_format = surface_format,
        .present_mode = present_mode,
    };
}

typedef struct Gpu2VkImageBarrier
{
    VkImage vk_image;
    VkPipelineStageFlags src_stage;
    VkPipelineStageFlags dst_stage;
    VkImageLayout old_layout;
    VkImageLayout new_layout;
	VkImageAspectFlags aspect_mask;

} Gpu2VkImageBarrier;

void gpu2_cmd_vk_image_barrier(Gpu2CommandBuffer* in_command_buffer, Gpu2VkImageBarrier* in_barrier)
{
    VkImageMemoryBarrier vk_image_memory_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = 0, 
        .dstAccessMask = 0,
        .oldLayout = in_barrier->old_layout,
        .newLayout = in_barrier->new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = in_barrier->vk_image,
        .subresourceRange =
		{
			.aspectMask = in_barrier->aspect_mask,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
    };

    vkCmdPipelineBarrier(
		in_command_buffer->vk_command_buffer, 
		in_barrier->src_stage, 
		in_barrier->dst_stage,
		 0,    // dependencyFlags
		 0,    // memoryBarrierCount
		 NULL, // pMemoryBarriers
		 0,    // bufferMemoryBarrierCount
		 NULL, // pBufferMemoryBarriers
		 1,    // imageMemoryBarrierCount
		 &vk_image_memory_barrier
	);
}

void gpu2_vk_resize_swapchain(Gpu2Device* in_device, const Window* const window);

bool gpu2_create_device(Window* in_window, Gpu2Device* out_device)
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
    u32 validation_layer_count = ARRAY_COUNT(validation_layers);

    // FCS TODO: Clean up defines. query surface extension string from window system?
    const char *extensions[] = {
        "VK_KHR_surface",
		#if defined(_WIN32)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
		#elif defined(__APPLE__)
        VK_EXT_METAL_SURFACE_EXTENSION_NAME,
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
		#endif
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    };
    u32 extension_count = ARRAY_COUNT(extensions); 

	VkInstanceCreateFlags instance_create_flags = 0;
	#if defined(__APPLE__)
	instance_create_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	#endif

    VkInstanceCreateInfo instance_create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = instance_create_flags, 
        .pApplicationInfo = &app_info,
        .enabledLayerCount = validation_layer_count,
        .ppEnabledLayerNames = validation_layers,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions,
    };

    VkInstance vk_instance = NULL;
    VK_CHECK(vkCreateInstance(&instance_create_info, NULL, &vk_instance));

    VkDebugUtilsMessengerCreateInfoEXT debug_utils_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = 0,
        .messageSeverity = 	VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                        	VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           	VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = 	VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
						VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       	VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = gpu2_vulkan_debug_callback,
        .pUserData = NULL};

    PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessenger = 
		(PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
			vk_instance, 
			"vkCreateDebugUtilsMessengerEXT"
	);
	assert(CreateDebugUtilsMessenger);

    VkDebugUtilsMessengerEXT debug_messenger;
    VK_CHECK(CreateDebugUtilsMessenger(vk_instance, &debug_utils_create_info, NULL, &debug_messenger));


    VkSurfaceKHR surface;
	#if defined(_WIN32)
	VkWin32SurfaceCreateInfoKHR surface_create_info = {
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.pNext = NULL,
		.flags = 0,
		.hinstance = in_window->hinstance,
		.hwnd = in_window->hwnd,
	};
	VK_CHECK(vkCreateWin32SurfaceKHR(vk_instance, &surface_create_info, NULL, &surface));
	#elif defined(__APPLE__)
	VkMetalSurfaceCreateInfoEXT surface_create_info = {
		.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
		.pNext = NULL,
		.flags = 0,
		.pLayer = in_window->metal_layer,
	};
	VK_CHECK(vkCreateMetalSurfaceEXT(vk_instance, &surface_create_info, NULL, &surface));
	#endif

    Gpu2VulkanPhysicalDeviceData physical_device_data = gpu2_vulkan_choose_physical_device(vk_instance, surface);

    float graphics_queue_priority = 1.0f;
    VkDeviceQueueCreateInfo graphics_queue_create_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.queueFamilyIndex = physical_device_data.graphics_family_index,
		.queueCount = 1,
		.pQueuePriorities = &graphics_queue_priority
	};

    const char *device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, // FCS NOTE: MOLTEN_VK is on Vulkan 1.2, so request extension for now
		#if defined(__APPLE__)
        "VK_KHR_portability_subset",
		#endif
    };
    u32 device_extension_count = ARRAY_COUNT(device_extensions);

	VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
		.pNext = NULL,
	};

    VkPhysicalDeviceSynchronization2Features sync_2_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
        .pNext = &descriptor_indexing_features,
    };

    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .pNext = &sync_2_features,
    };

    VkPhysicalDeviceFeatures2 features_2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &dynamic_rendering_features,
        .features = {},
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

	assert(descriptor_indexing_features.descriptorBindingSampledImageUpdateAfterBind);
	assert(descriptor_indexing_features.descriptorBindingUniformBufferUpdateAfterBind);
	assert(descriptor_indexing_features.descriptorBindingStorageBufferUpdateAfterBind);
	assert(descriptor_indexing_features.shaderSampledImageArrayNonUniformIndexing);
	assert(descriptor_indexing_features.shaderStorageBufferArrayNonUniformIndexing);
	assert(descriptor_indexing_features.shaderUniformBufferArrayNonUniformIndexing);
	assert(features_2.features.shaderStorageBufferArrayDynamicIndexing);
	assert(features_2.features.shaderUniformBufferArrayDynamicIndexing);

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

    VkDevice vk_device = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(physical_device_data.physical_device, &device_create_info, NULL, &vk_device));

	// FCS TODO: use 1.3 core functions once MoltenVK Supports them
    pfn_vk_begin_rendering = (PFN_vkCmdBeginRenderingKHR)vkGetDeviceProcAddr(vk_device, "vkCmdBeginRenderingKHR");
    pfn_vk_end_rendering = (PFN_vkCmdEndRenderingKHR)vkGetDeviceProcAddr(vk_device, "vkCmdEndRenderingKHR");
    assert(pfn_vk_begin_rendering && pfn_vk_end_rendering);

    VkQueue graphics_queue;
    vkGetDeviceQueue(vk_device, physical_device_data.graphics_family_index, 0, &graphics_queue);

    VkCommandPoolCreateInfo graphics_command_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = physical_device_data.graphics_family_index,
    };
    VkCommandPool graphics_command_pool;
    vkCreateCommandPool(vk_device, &graphics_command_pool_create_info, NULL, &graphics_command_pool);

    // Set up Memory Types
    VkPhysicalDeviceMemoryProperties2 vk_memory_properties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
    };
    vkGetPhysicalDeviceMemoryProperties2(physical_device_data.physical_device, &vk_memory_properties);

	*out_device = (Gpu2Device) {
		// Main Vulkan Objects
		.vk_instance = vk_instance,
		.vk_physical_device = physical_device_data.physical_device,
		.vk_device = vk_device,
		// Queue
		.graphics_queue = graphics_queue,
		.graphics_command_pool = graphics_command_pool,
		// Debug Functionality
		.debug_messenger = debug_messenger,
		.pfn_set_object_name = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(vk_instance, "vkSetDebugUtilsObjectNameEXT"),
		// Surface + Swapchain
		.surface = surface,
		.present_mode = physical_device_data.present_mode,
        .surface_format = physical_device_data.surface_format,
        .swapchain = VK_NULL_HANDLE,
		// Memory
		.num_memory_types = vk_memory_properties.memoryProperties.memoryTypeCount,
        .memory_types = calloc(vk_memory_properties.memoryProperties.memoryTypeCount, sizeof(Gpu2MemoryType)),
        .vk_memory_properties = vk_memory_properties.memoryProperties,
		// Pending Present
		.has_pending_present_info = false,
		.pending_vk_present_info = {},
	};

    gpu2_vk_resize_swapchain(out_device, in_window);
	
	return true;
}

void gpu2_vk_resize_swapchain(Gpu2Device* in_device, const Window* const in_window)
{
    vkDeviceWaitIdle(in_device->vk_device);

    if (in_device->swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(in_device->vk_device, in_device->swapchain, NULL);
    }

    int swapchain_width = 0;
    int swapchain_height = 0;
    window_get_dimensions(in_window, &swapchain_width, &swapchain_height);

    VkSurfaceCapabilitiesKHR surface_capabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(in_device->vk_physical_device, in_device->surface, &surface_capabilities));
    in_device->swapchain_extent = (VkExtent2D) {
        .width = CLAMP(swapchain_width, surface_capabilities.minImageExtent.width,surface_capabilities.maxImageExtent.width),
        .height = CLAMP(swapchain_height, surface_capabilities.minImageExtent.height,surface_capabilities.maxImageExtent.height),
    };

    u32 swapchain_image_count = surface_capabilities.minImageCount + 1;
    if (swapchain_image_count > surface_capabilities.maxImageCount)
    {
        swapchain_image_count = surface_capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = in_device->surface,
        .minImageCount = swapchain_image_count,
        .imageFormat = in_device->surface_format.format,
        .imageColorSpace = in_device->surface_format.colorSpace,
        .imageExtent = in_device->swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = in_device->present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    VK_CHECK(vkCreateSwapchainKHR(in_device->vk_device, &swapchain_create_info, NULL, &in_device->swapchain));

    VK_CHECK(vkGetSwapchainImagesKHR(in_device->vk_device, in_device->swapchain, &swapchain_image_count, NULL));
    VkImage swapchain_images[swapchain_image_count];
    VK_CHECK(vkGetSwapchainImagesKHR(in_device->vk_device, in_device->swapchain, &swapchain_image_count, swapchain_images));
	
	// Clean up old image views here before freeing swapchain_images
	u32 old_swapchain_image_count = in_device->swapchain_image_count;
    for (i32 i = 0; i < old_swapchain_image_count; ++i)
    {
		vkDestroyImageView(in_device->vk_device, in_device->swapchain_images[i].vk_image_view, NULL);
	}

    if (in_device->swapchain_images != NULL)
    {
        free(in_device->swapchain_images);
    }
    in_device->swapchain_images = malloc(swapchain_image_count * sizeof(Gpu2Texture));
    for (i32 i = 0; i < swapchain_image_count; ++i)
    {
        in_device->swapchain_images[i] = (Gpu2Texture){
            .vk_image = swapchain_images[i],
			.vk_image_view = VK_NULL_HANDLE,
            .vk_format = in_device->surface_format.format,
            .memory = NULL, // Memory managed externally
        };

		// Create our image view
		VkImageViewCreateInfo image_view_create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .flags = 0,
            .pNext = NULL,
            .image = swapchain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = in_device->surface_format.format,
            .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, //FCS TODO:
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1,
        };
        VK_CHECK(vkCreateImageView(in_device->vk_device, &image_view_create_info, NULL, &in_device->swapchain_images[i].vk_image_view));
    }

	in_device->current_image_index = 0;
    in_device->swapchain_image_count = swapchain_image_count;
}

bool gpu2_create_shader(Gpu2Device* in_device, Gpu2ShaderCreateInfo* in_create_info, Gpu2Shader* out_shader)
{
	String filename_string = string_new(in_create_info->filename);
	string_append(&filename_string, ".spv");
	
	size_t file_size;	
	char* shader_source;
	read_binary_file(&filename_string, &file_size, (void**)&shader_source);
	string_free(&filename_string);

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .codeSize = file_size,
        .pCode = (u32*) shader_source,
    };

    VkShaderModule vk_shader_module;
    VK_CHECK(vkCreateShaderModule(in_device->vk_device, &create_info, NULL, &vk_shader_module));
    *out_shader = (Gpu2Shader){
        .vk_shader_module = vk_shader_module,
    };

	return true;
}

VkShaderStageFlags gpu2_shader_stages_to_vk_shader_stages(Gpu2ShaderStageFlags in_flags)
{	
	VkShaderStageFlags vk_flags = 0;
	if (BIT_COMPARE(in_flags, GPU2_SHADER_STAGE_VERTEX))
	{
		vk_flags |= VK_SHADER_STAGE_VERTEX_BIT;
	}
	if (BIT_COMPARE(in_flags, GPU2_SHADER_STAGE_FRAGMENT))
	{
		vk_flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	if (BIT_COMPARE(in_flags, GPU2_SHADER_STAGE_COMPUTE))
	{
		vk_flags |= VK_SHADER_STAGE_COMPUTE_BIT;
	}
	return vk_flags;
}

VkDescriptorType gpu2_binding_type_to_vk_descriptor_type(Gpu2BindingType in_type)
{
	switch (in_type)
	{
		case GPU2_BINDING_TYPE_BUFFER:
		{
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			break;
		}
		case GPU2_BINDING_TYPE_TEXTURE:
		{
			return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			break;
		}
		default:
		{
			assert(false);
			return 0;
		}
	}
}

bool gpu2_create_bind_group_layout(Gpu2Device* in_device, Gpu2BindGroupLayoutCreateInfo* in_create_info, Gpu2BindGroupLayout* out_bind_group_layout)
{
	VkDescriptorSetLayoutBinding vk_bindings[in_create_info->num_bindings];
	for (i32 binding_idx = 0; binding_idx < in_create_info->num_bindings; ++binding_idx)
	{
		Gpu2ResourceBinding* binding = &in_create_info->bindings[binding_idx];
		vk_bindings[binding_idx] = (VkDescriptorSetLayoutBinding) {
			.binding = binding_idx,	
			.descriptorType = gpu2_binding_type_to_vk_descriptor_type(binding->type),
			.descriptorCount = 1,
			.stageFlags = gpu2_shader_stages_to_vk_shader_stages(binding->shader_stages),
			.pImmutableSamplers = NULL,
		};	
	}

	VkDescriptorSetLayoutCreateInfo vk_desc_layout_create_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.bindingCount = in_create_info->num_bindings,
		.pBindings = vk_bindings,
	};
	VkDescriptorSetLayout vk_descriptor_set_layout = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDescriptorSetLayout(in_device->vk_device, &vk_desc_layout_create_info, NULL, &vk_descriptor_set_layout));

	out_bind_group_layout->vk_descriptor_set_layout = vk_descriptor_set_layout;
	out_bind_group_layout->index = in_create_info->index;
	out_bind_group_layout->num_bindings = in_create_info->num_bindings;	
	memcpy(out_bind_group_layout->bindings, in_create_info->bindings, sizeof(Gpu2ResourceBinding) * in_create_info->num_bindings);

	return true;
}

bool gpu2_create_bind_group(Gpu2Device* in_device, Gpu2BindGroupCreateInfo* in_create_info, Gpu2BindGroup* out_bind_group)
{
	Gpu2BindGroupLayout* layout = in_create_info->layout;
	VkDescriptorPoolSize vk_pool_sizes[layout->num_bindings];
	for (i32 binding_idx = 0; binding_idx < layout->num_bindings; ++binding_idx)
	{
		vk_pool_sizes[binding_idx] = (VkDescriptorPoolSize) {
			.type = gpu2_binding_type_to_vk_descriptor_type(layout->bindings[binding_idx].type),
			.descriptorCount = 1,
		};
	}

	VkDescriptorPoolCreateInfo vk_descriptor_pool_create_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.maxSets = 1,
		.poolSizeCount = in_create_info->layout->num_bindings,
		.pPoolSizes = vk_pool_sizes,
	};

	VkDescriptorPool vk_descriptor_pool = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDescriptorPool(in_device->vk_device, &vk_descriptor_pool_create_info, NULL, &vk_descriptor_pool));

	VkDescriptorSetAllocateInfo vk_descriptor_set_alloc_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = NULL,
		.descriptorPool = vk_descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout->vk_descriptor_set_layout,
	};
	VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;
	VK_CHECK(vkAllocateDescriptorSets(in_device->vk_device, &vk_descriptor_set_alloc_info, &vk_descriptor_set));

	// Go Ahead and write descriptor set as well
	VkDescriptorBufferInfo vk_descriptor_buffer_infos[in_create_info->num_writes];
	VkDescriptorImageInfo vk_descriptor_image_infos[in_create_info->num_writes];
    VkWriteDescriptorSet vk_descriptor_writes[in_create_info->num_writes];
	for (i32 write_idx = 0; write_idx < in_create_info->num_writes; ++write_idx)
	{
		Gpu2ResourceWrite* resource_write = &in_create_info->writes[write_idx];

		VkDescriptorType vk_descriptor_type;
		switch (resource_write->type)
		{
			case GPU2_BINDING_TYPE_BUFFER:
			{
				vk_descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				vk_descriptor_buffer_infos[write_idx] = (VkDescriptorBufferInfo) {
					.buffer = resource_write->buffer_binding.buffer->vk_buffer,
					.offset = 0,
					.range  = VK_WHOLE_SIZE,
				};
				break;
			}
			case GPU2_BINDING_TYPE_TEXTURE:
			{
				//vk_descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				//vk_descriptor_image_infos[write_idx] = (VkDescriptorImageInfo) {
				//	.sampler = TODO,
				//	.imageView = TODO,
				//	.imageLayout = TODO,
				//};
				assert(false);	
				break;
			}
			default:
			{
				assert(false);
			}
		}

		vk_descriptor_writes[write_idx] = (VkWriteDescriptorSet) {	
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet = vk_descriptor_set,
            .dstBinding = write_idx, // Write binding is assumed to be our write index (must write all resources...)
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk_descriptor_type,
            .pImageInfo = resource_write->type == GPU2_BINDING_TYPE_TEXTURE ? &vk_descriptor_image_infos[write_idx] : NULL,
            .pBufferInfo = resource_write->type == GPU2_BINDING_TYPE_BUFFER ? &vk_descriptor_buffer_infos[write_idx] : NULL,
            .pTexelBufferView = NULL,
		};
	}
	vkUpdateDescriptorSets(in_device->vk_device, in_create_info->num_writes, vk_descriptor_writes, 0, NULL);

	*out_bind_group = (Gpu2BindGroup) {
		.layout = *layout, 
		.vk_descriptor_pool = vk_descriptor_pool,
		.vk_descriptor_set = vk_descriptor_set,
	};

	return true;
}

bool gpu2_create_render_pipeline(Gpu2Device* in_device, Gpu2RenderPipelineCreateInfo* in_create_info, Gpu2RenderPipeline* out_render_pipeline)
{
	VkPipelineShaderStageCreateInfo vertex_shader_stage_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = in_create_info->vertex_shader->vk_shader_module,
		.pName = "main",
	};

	VkPipelineShaderStageCreateInfo fragment_shader_stage_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = in_create_info->fragment_shader->vk_shader_module,
		.pName = "main",
	};

	VkPipelineShaderStageCreateInfo shader_stages[] = {
		vertex_shader_stage_create_info,
		fragment_shader_stage_create_info,
	};

	VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo dynamic_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = ARRAY_COUNT(dynamic_states),
		.pDynamicStates = dynamic_states,
	};

	VkPipelineVertexInputStateCreateInfo vertex_input = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = NULL,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = NULL, 
	};

	VkPipelineInputAssemblyStateCreateInfo input_assembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};

	VkPipelineViewportStateCreateInfo viewport = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,	// Only need count because of dynamic state
		.scissorCount = 1,	// Only need count because of dynamic state
	};

	VkPipelineRasterizationStateCreateInfo rasterization = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
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
		.depthTestEnable = in_create_info->depth_test_enabled ? VK_TRUE : VK_FALSE, 
		.depthWriteEnable = in_create_info->depth_test_enabled ? VK_TRUE : VK_FALSE,
		.depthCompareOp = VK_COMPARE_OP_LESS, 
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
		.front = {},
		.back = {},
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f,
	};

	VkPipelineColorBlendAttachmentState color_blending_attachments[] = {{
		.blendEnable = VK_TRUE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT 
						| VK_COLOR_COMPONENT_G_BIT
						| VK_COLOR_COMPONENT_B_BIT
						| VK_COLOR_COMPONENT_A_BIT,
	}};

	VkPipelineColorBlendStateCreateInfo color_blending = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1, // TODO: based on num color attachments
		.pAttachments = color_blending_attachments,
		.blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
	};

	VkDescriptorSetLayout set_layouts[in_create_info->num_bind_groups];
	for (u32 bind_group_idx = 0; bind_group_idx < in_create_info->num_bind_groups; ++bind_group_idx)
	{
		set_layouts[bind_group_idx] = in_create_info->bind_groups[bind_group_idx]->layout.vk_descriptor_set_layout;
	}
	const u32 num_set_layouts = ARRAY_COUNT(set_layouts); 

	VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = num_set_layouts, 
		.pSetLayouts = set_layouts, 
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = NULL,
	};

	VkPipelineLayout vk_pipeline_layout;
	VK_CHECK(vkCreatePipelineLayout(in_device->vk_device, &pipeline_layout_create_info, NULL, &vk_pipeline_layout));

	const VkFormat vk_color_attachment_formats[] = {
		in_device->surface_format.format,
	};

	//FCS TODO: Pass attachment formats in create_info
	VkPipelineRenderingCreateInfo vk_pipeline_rendering_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext = NULL,
		.viewMask = 0,
		.colorAttachmentCount = ARRAY_COUNT(vk_color_attachment_formats),
		.pColorAttachmentFormats = vk_color_attachment_formats,
		.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT, // FCS TODO
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED, //FCS TODO
	};

	VkGraphicsPipelineCreateInfo pipeline_create_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &vk_pipeline_rendering_create_info,
		.flags = 0,
		.stageCount = 2,
		.pStages = shader_stages,
		.pVertexInputState = &vertex_input,
		.pInputAssemblyState = &input_assembly,
		.pTessellationState = NULL,
		.pViewportState = &viewport,
		.pRasterizationState = &rasterization,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = &color_blending,
		.pDynamicState = &dynamic_state,
		.layout = vk_pipeline_layout,
		.renderPass = VK_NULL_HANDLE, //render_pass, //FCS TODO: Will use dynamic rendering
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = 0,
	};

	VkPipeline vk_pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(in_device->vk_device, VK_NULL_HANDLE, 1, &pipeline_create_info, NULL, &vk_pipeline));

	*out_render_pipeline = (Gpu2RenderPipeline) {
		.vk_pipeline = vk_pipeline,
		.vk_pipeline_layout = vk_pipeline_layout,
	};

	return true;
}

#define MB * 1024 * 1024ULL
#define GPU_BLOCK_SIZE 256 MB

/*TODO: bufferImageGranularity: If the previous block contains a non-linear resource while the current one is linear or
   vice versa then the alignment requirement is the max of the VkMemoryRequirements.alignment and the device's
   bufferImageGranularity. */

u32 gpu2_vk_find_memory_type(VkPhysicalDevice physical_device, u32 type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

    for (u32 i = 0; i < memory_properties.memoryTypeCount; ++i)
    {
        if ((type_filter & (1 << i)) && ((memory_properties.memoryTypes[i].propertyFlags & properties) == properties))
        {
            return i;
        }
    }

    return UINT_MAX;
}

void gpu2_vk_map_memory(Gpu2Device* in_device, Gpu2Memory *memory, u64 offset, u64 size, void **ppData)
{
	assert(!memory->is_mapped);

	const bool is_block_unmapped = memory->memory_region->owning_block->mapped_ref_count == 0;
	if (is_block_unmapped)
	{
		VK_CHECK(vkMapMemory(
			in_device->vk_device, 
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

void gpu2_vk_unmap_memory(Gpu2Device* in_device, Gpu2Memory *memory)
{
	assert(memory->is_mapped);
	assert(memory->memory_region->owning_block->mapped_ref_count > 0);

	memory->is_mapped = false;
	memory->memory_region->owning_block->mapped_ref_count--;

	const bool needs_unmap = memory->memory_region->owning_block->mapped_ref_count == 0;
	if (needs_unmap)
	{
    	vkUnmapMemory(in_device->vk_device, memory->memory_region->owning_block->vk_memory);
	}
}

Gpu2Memory* gpu2_vk_allocate_memory(Gpu2Device* in_device, u32 type_filter, VkMemoryPropertyFlags memory_properties, u64 alloc_size, u64 alignment)
{
	u32 memory_type_index = gpu2_vk_find_memory_type(in_device->vk_physical_device, type_filter, memory_properties);
    assert(memory_type_index < in_device->num_memory_types);

    Gpu2MemoryType *memory_type = &in_device->memory_types[memory_type_index];
    assert(memory_type);

    // Search Blocks of Memory Type for free list entry of sufficient size
    for (u32 block_index = 0; block_index < sb_count(memory_type->memory_blocks); ++block_index)
    {
        Gpu2MemoryBlock *block = &memory_type->memory_blocks[block_index];
        assert(block);

        for (u32 free_list_index = 0; free_list_index < sb_count(block->free_list); ++free_list_index)
        {
            Gpu2MemoryRegion *free_list_region = &block->free_list[free_list_index];
            assert(free_list_region);

            u64 padding = alignment - free_list_region->offset % alignment;
            u64 new_region_size = alloc_size + padding;

            if (free_list_region->size >= new_region_size)
            {
                sb_push(
					block->used_list, 
					((Gpu2MemoryRegion){
					  .padding = padding,
					  .offset = free_list_region->offset + padding,
					  .size = alloc_size,
					  .owning_block = block,
					  .alloc_ref = calloc(1, sizeof(Gpu2Memory)),
					})
				);

                Gpu2MemoryRegion *out_region = &block->used_list[sb_count(block->used_list) - 1];
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

    u32 heap_index = in_device->vk_memory_properties.memoryTypes[memory_type_index].heapIndex;
    VkDeviceSize heap_size = in_device->vk_memory_properties.memoryHeaps[heap_index].size;
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
    VK_CHECK(vkAllocateMemory(in_device->vk_device, &alloc_info, NULL, &vk_memory));

    // Create a new block

    sb_push(memory_type->memory_blocks, ((Gpu2MemoryBlock){
                                            .size = alloc_info.allocationSize,
                                            .free_list = NULL,
                                            .used_list = NULL,
                                            .owning_type = memory_type,
                                            .vk_memory = vk_memory,
                                        }));

    // fix backwards-references in free/used lists of old blocks
    for (u32 block_idx = 0; block_idx < sb_count(memory_type->memory_blocks) - 1; ++block_idx)
    {
        Gpu2MemoryBlock *block = &memory_type->memory_blocks[block_idx];
        for (u32 free_idx = 0; free_idx < sb_count(block->free_list); ++free_idx)
        {
            block->free_list[free_idx].owning_block = block;
        }
        for (u32 used_idx = 0; used_idx < sb_count(block->used_list); ++used_idx)
        {
            block->used_list[used_idx].owning_block = block;
        }
    }

    Gpu2MemoryBlock *new_block = &memory_type->memory_blocks[sb_count(memory_type->memory_blocks) - 1];

    // FIXME: need to check if remaining size is zero (free_list region)
    sb_push(
		new_block->free_list, 
		((Gpu2MemoryRegion){
			.padding = 0,
			.offset = alloc_size,
			.size = alloc_info.allocationSize - alloc_size,
			.owning_block = new_block,
		}
	));

    sb_push(new_block->used_list, ((Gpu2MemoryRegion){
                                      .padding = 0,
                                      .offset = 0,
                                      .size = alloc_size,
                                      .owning_block = new_block,
                                      .alloc_ref = calloc(1, sizeof(Gpu2Memory)),
                                  }));

    *new_block->used_list[0].alloc_ref = (Gpu2Memory){
        .memory_region = &new_block->used_list[0],
        .memory_properties = memory_properties,
    };

    return new_block->used_list[0].alloc_ref;
}

void gpu2_vk_free_memory(Gpu2Device* in_device, Gpu2Memory *gpu_memory)
{
    // Add this memory's region as a new free list entry
    assert(gpu_memory);
    Gpu2MemoryRegion *memory_region = gpu_memory->memory_region;
    assert(memory_region);
    Gpu2MemoryBlock *owning_block = memory_region->owning_block;
    assert(owning_block);

	//FCS TODO: gpu2_map_memory and gpu2_unmap_memory
	if (gpu_memory->is_mapped)
	{
		gpu2_vk_unmap_memory(in_device, gpu_memory);
	}

    // 1. Find where our element lies in used_list
    u32 used_list_index = memory_region - owning_block->used_list;
    assert(used_list_index < sb_count(owning_block->used_list));

    // 2. Find insertion point in free list (used_region->offset > current_free_region->offset)
    for (u32 free_list_index = 0; free_list_index < sb_count(owning_block->free_list); ++free_list_index)
    {
        Gpu2MemoryRegion *free_entry = &owning_block->free_list[free_list_index];
        if (memory_region->offset < free_entry->offset)
        {
            // Padding info isn't used once freed
            memory_region->offset -= memory_region->padding;
            memory_region->size += memory_region->padding;
            memory_region->padding = 0;
            // TODO: Check this padding logic

            const i32 previous_index = free_list_index - 1;
            Gpu2MemoryRegion *preceding_entry = previous_index >= 0 ? &owning_block->free_list[previous_index] : NULL;

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
        vkFreeMemory(in_device->vk_device, owning_block->vk_memory, NULL);
        Gpu2MemoryType *owning_type = owning_block->owning_type;
        assert(owning_type);
        u32 owning_block_index = owning_block - owning_type->memory_blocks;
        sb_del(owning_type->memory_blocks, owning_block_index);
    }
}

//FCS TODO: Buffer Usage flags arg?
static const VkBufferUsageFlags FIXME_VK_BUFFER_USAGE_FLAGS = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

bool gpu2_create_buffer(Gpu2Device* in_device, Gpu2BufferCreateInfo* in_create_info, Gpu2Buffer* out_buffer)
{
	VkBufferCreateInfo buffer_create_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = in_create_info->size,
		.usage = FIXME_VK_BUFFER_USAGE_FLAGS, 
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VkBuffer vk_buffer;
	VK_CHECK(vkCreateBuffer(in_device->vk_device, &buffer_create_info, NULL, &vk_buffer));


	VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	if (in_create_info->is_cpu_visible)
	{
		memory_properties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		memory_properties |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	}

	VkMemoryRequirements memory_reqs;
	vkGetBufferMemoryRequirements(in_device->vk_device, vk_buffer, &memory_reqs);
	Gpu2Memory* memory = gpu2_vk_allocate_memory(in_device, memory_reqs.memoryTypeBits, memory_properties, memory_reqs.size,
	memory_reqs.alignment);

	VK_CHECK(vkBindBufferMemory(in_device->vk_device, vk_buffer, memory->memory_region->owning_block->vk_memory,
	memory->memory_region->offset));

	//FCS TODO: Will need staging buffer for non-CPU addressable memory
	void* pBufferData;
	gpu2_vk_map_memory(in_device, memory, 0, memory->memory_region->size, &pBufferData);
	memcpy(pBufferData, in_create_info->data, in_create_info->size);
	gpu2_vk_unmap_memory(in_device, memory);

	*out_buffer = (Gpu2Buffer){
		.vk_buffer = vk_buffer,
		.memory = memory,
	};

	return true;
}

void gpu2_write_buffer(Gpu2Device* in_device, Gpu2Buffer* in_buffer, Gpu2BufferWriteInfo* in_write_info)
{
	Gpu2Memory* memory = in_buffer->memory;

	// Make Sure our memory is host-visible and host-coherent
	VkMemoryPropertyFlags flags_to_check = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 
										 | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	assert((memory->memory_properties & flags_to_check) != 0);
	
	void* pBufferData;
	gpu2_vk_map_memory(in_device, memory, 0, memory->memory_region->size, &pBufferData);
	memcpy(pBufferData, in_write_info->data, in_write_info->size);
	gpu2_vk_unmap_memory(in_device, memory);
}

VkFormat gpu2_format_to_vk_format(Gpu2Format in_format)
{
	switch(in_format)
	{
		case GPU2_FORMAT_D32_SFLOAT: return VK_FORMAT_D32_SFLOAT;
		default: 
			printf("gpu2_format_to_vk_format: Unimplemented Format\n");
			exit(0);
			return 0;
	}
}

VkImageUsageFlags gpu2_texture_usage_flags_to_vk_image_usage_flags(Gpu2TextureUsageFlags in_flags)
{
	VkImageUsageFlags out_flags = 0;
	
	if (BIT_COMPARE(in_flags, GPU2_TEXTURE_USAGE_TRANSFER_SRC))
	{
		out_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}

	if (BIT_COMPARE(in_flags, GPU2_TEXTURE_USAGE_TRANSFER_DST))
	{
		out_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	if (BIT_COMPARE(in_flags, GPU2_TEXTURE_USAGE_SAMPLED))
	{
		out_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}

	if (BIT_COMPARE(in_flags, GPU2_TEXTURE_USAGE_STORAGE))
	{
		out_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
	}

	if (BIT_COMPARE(in_flags, GPU2_TEXTURE_USAGE_COLOR_ATTACHMENT))
	{
		out_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}

	if (BIT_COMPARE(in_flags, GPU2_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT))
	{
		out_flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}

	return out_flags;
}

bool gpu2_create_texture(Gpu2Device* in_device, Gpu2TextureCreateInfo* in_create_info, Gpu2Texture* out_texture)
{
	const Gpu2TextureExtent extent = in_create_info->extent;
    VkImageType vk_image_type 	= extent.depth > 1 	? 	VK_IMAGE_TYPE_3D
								: extent.height > 1	? 	VK_IMAGE_TYPE_2D
								: 						VK_IMAGE_TYPE_1D;

	VkFormat vk_format = gpu2_format_to_vk_format(in_create_info->format);

    VkImageCreateInfo vk_image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .imageType = vk_image_type,
        .format = vk_format,
        .extent =
            {
                .width = extent.width,
                .height = extent.height,
                .depth = extent.depth,
            },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = gpu2_texture_usage_flags_to_vk_image_usage_flags(in_create_info->usage),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkImage vk_image = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(in_device->vk_device, &vk_image_create_info, NULL, &vk_image));

    VkMemoryRequirements memory_reqs;
    vkGetImageMemoryRequirements(in_device->vk_device, vk_image, &memory_reqs);

	VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	if (in_create_info->is_cpu_visible)
	{
		memory_properties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		memory_properties |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	}

    Gpu2Memory *memory = gpu2_vk_allocate_memory(
		in_device, 
		memory_reqs.memoryTypeBits, 
		memory_properties, 
		memory_reqs.size,
		memory_reqs.alignment
	);

    VK_CHECK(vkBindImageMemory(
		in_device->vk_device, 
		vk_image,
		memory->memory_region->owning_block->vk_memory,
        memory->memory_region->offset
	));

	VkImageViewType vk_image_view_type = VK_IMAGE_VIEW_TYPE_1D;
	switch(vk_image_type)
	{
		case VK_IMAGE_TYPE_1D: vk_image_view_type = VK_IMAGE_VIEW_TYPE_1D; break;
		case VK_IMAGE_TYPE_2D: vk_image_view_type = VK_IMAGE_VIEW_TYPE_2D; break;
		case VK_IMAGE_TYPE_3D: vk_image_view_type = VK_IMAGE_VIEW_TYPE_3D; break;
		default: break;
	}

	// FCS TODO: Set aspectMask based on format
    VkImageViewCreateInfo vk_image_view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .flags = 0,
        .pNext = NULL,
        .image = vk_image,
        .viewType = vk_image_view_type,
        .format = vk_format,
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

    VkImageView vk_image_view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(in_device->vk_device, &vk_image_view_create_info, NULL, &vk_image_view));

	*out_texture = (Gpu2Texture) {
		.vk_image = vk_image,
		.vk_image_view = vk_image_view,
		.vk_format = vk_format,
	};

	return true;
}

bool gpu2_create_command_buffer(Gpu2Device* in_device, Gpu2CommandBuffer* out_command_buffer)
{
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = in_device->graphics_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer vk_command_buffer;
    VK_CHECK(vkAllocateCommandBuffers(in_device->vk_device, &alloc_info, &vk_command_buffer));

    *out_command_buffer = (Gpu2CommandBuffer) {
        .vk_command_buffer = vk_command_buffer,
    };

	VK_CHECK(vkBeginCommandBuffer(out_command_buffer->vk_command_buffer,
	   &(VkCommandBufferBeginInfo){
		   .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		   .pNext = NULL,
		   .flags = 0,
		   .pInheritanceInfo = NULL,
	   }
	));

	return true;
}

bool gpu2_get_next_drawable(Gpu2Device* in_device, Gpu2CommandBuffer* in_command_buffer, Gpu2Drawable* out_drawable)
{
	//FCS TODO: Actual Synchronization... array of image acquired semaphores on device that we pass to queue submission later 

	VkFenceCreateInfo vk_fence_create_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
	};

	VkFence vk_fence;
	VK_CHECK(vkCreateFence(in_device->vk_device, &vk_fence_create_info, NULL, &vk_fence));

    vkAcquireNextImageKHR(
		in_device->vk_device, 
		in_device->swapchain, 
		UINT64_MAX, 
		VK_NULL_HANDLE,
		vk_fence, //FCS TODO: TEMP
		&in_device->current_image_index
	);

	//FCS TODO: TEMP
	vkWaitForFences(in_device->vk_device, 1, &vk_fence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(in_device->vk_device, vk_fence, NULL);

	*out_drawable = (Gpu2Drawable) {
		.texture = in_device->swapchain_images[in_device->current_image_index],
	};

	gpu2_cmd_vk_image_barrier(
		in_command_buffer, 
		&(Gpu2VkImageBarrier){
			.vk_image = out_drawable->texture.vk_image,
			.src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			.dst_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
			.new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
		}
	);

	return true;
}

bool gpu2_drawable_get_texture(Gpu2Drawable* in_drawable, Gpu2Texture* out_texture)
{
	*out_texture = in_drawable->texture;
	return true;
}

VkAttachmentLoadOp gpu2_load_action_to_vulkan_load_op(Gpu2LoadAction in_load_action)
{
	switch (in_load_action)
	{
		case GPU2_LOAD_ACTION_DONT_CARE:	return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		case GPU2_LOAD_ACTION_LOAD: 		return VK_ATTACHMENT_LOAD_OP_LOAD;
		case GPU2_LOAD_ACTION_CLEAR:		return VK_ATTACHMENT_LOAD_OP_CLEAR;
	}
	assert(false);
}

VkAttachmentStoreOp gpu2_store_action_to_vulkan_store_op(Gpu2StoreAction in_store_action)
{
	switch (in_store_action)
	{
		case GPU2_STORE_ACTION_DONT_CARE: 	return VK_ATTACHMENT_STORE_OP_DONT_CARE;
		case GPU2_STORE_ACTION_STORE:		return VK_ATTACHMENT_STORE_OP_STORE;
	}
	assert(false);
}

void gpu2_begin_render_pass(Gpu2Device* in_device, Gpu2RenderPassCreateInfo* in_create_info, Gpu2RenderPass* out_render_pass)
{
	VkExtent2D rendering_extent = in_device->swapchain_extent;

    VkRenderingAttachmentInfo vk_color_attachments[in_create_info->num_color_attachments];
    for (u32 color_attachment_idx = 0; color_attachment_idx < in_create_info->num_color_attachments; ++color_attachment_idx)
    {
		Gpu2ColorAttachmentDescriptor* color_attachment_desc = &in_create_info->color_attachments[color_attachment_idx];
		Gpu2Texture* color_attachment_texture = color_attachment_desc->texture;

		vk_color_attachments[color_attachment_idx] = (VkRenderingAttachmentInfo) {	
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = NULL,
			.imageView = color_attachment_texture->vk_image_view,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = 0,
			.loadOp = gpu2_load_action_to_vulkan_load_op(color_attachment_desc->load_action),
			.storeOp = gpu2_store_action_to_vulkan_store_op(color_attachment_desc->store_action),
			.clearValue = {
				.color = {
					.float32 = { 
						color_attachment_desc->clear_color[0], 
						color_attachment_desc->clear_color[1], 
						color_attachment_desc->clear_color[2], 
						color_attachment_desc->clear_color[3], 
					},
				},
			},
		};
    }

	VkRenderingAttachmentInfo vk_depth_attachment = {};
	Gpu2DepthAttachmentDescriptor* depth_attachment_desc = in_create_info->depth_attachment;
	if (depth_attachment_desc)
	{
		Gpu2Texture* depth_attachment_texture = depth_attachment_desc->texture;

		vk_depth_attachment = (VkRenderingAttachmentInfo) {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = NULL,
			.imageView = depth_attachment_texture->vk_image_view,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = 0,
			.loadOp = gpu2_load_action_to_vulkan_load_op(depth_attachment_desc->load_action),
			.storeOp = gpu2_store_action_to_vulkan_store_op(depth_attachment_desc->store_action),
			.clearValue = {
				.depthStencil = {
					.depth = depth_attachment_desc->clear_depth,
				},
			},
		};

		gpu2_cmd_vk_image_barrier(
			in_create_info->command_buffer, 
			&(Gpu2VkImageBarrier){
				.vk_image = depth_attachment_texture->vk_image,
				.src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				.dst_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
				.new_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT,
			}
		);
	}

    VkRenderingInfo vk_rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderArea = {
			.offset = {
				.x = 0,
				.y = 0,
			},
			.extent = rendering_extent, 
		},
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = in_create_info->num_color_attachments,
        .pColorAttachments = vk_color_attachments,
        .pDepthAttachment = depth_attachment_desc ? &vk_depth_attachment : NULL,
        .pStencilAttachment = NULL,
    };

	*out_render_pass = (Gpu2RenderPass) {
		.vk_command_buffer = in_create_info->command_buffer->vk_command_buffer,
	};

	// FCS TODO: use vkCmdBeginRendering once MoltenVK is at 1.3
    // vkCmdBeginRendering(command_buffer->vk_command_buffer, &vk_rendering_info);
    pfn_vk_begin_rendering(out_render_pass->vk_command_buffer, &vk_rendering_info);

	VkViewport vk_viewport = {
		.x = 0.0f,
		.y = (float) rendering_extent.height,
		.width = (float) rendering_extent.width,
		.height = -(float) rendering_extent.height,
		.minDepth = 0.0,
		.maxDepth = 1.0,
	};
    vkCmdSetViewport(
		out_render_pass->vk_command_buffer, 
		0, // First Viewport
		1, // Viewport Count
		&vk_viewport
	);
	
	VkRect2D vk_scissor_rect = {
		.offset = {
			.x = 0,
			.y = 0, 
		},
		.extent = {
			.width = rendering_extent.width,
			.height = rendering_extent.height,
		},
	};
    vkCmdSetScissor(
		out_render_pass->vk_command_buffer, 
		0, // First Scissor
		1, // Scissor Count
		&vk_scissor_rect
	);
}

void gpu2_end_render_pass(Gpu2RenderPass* in_render_pass)
{
    pfn_vk_end_rendering(in_render_pass->vk_command_buffer);
}

void gpu2_render_pass_set_render_pipeline(Gpu2RenderPass* in_render_pass, Gpu2RenderPipeline* in_render_pipeline)
{
    vkCmdBindPipeline(in_render_pass->vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, in_render_pipeline->vk_pipeline);
}

void gpu2_render_pass_set_bind_group(Gpu2RenderPass* in_render_pass, Gpu2RenderPipeline* in_render_pipeline, Gpu2BindGroup* in_bind_group)
{
	const u32 descriptor_set_count = 1;

    vkCmdBindDescriptorSets(
		in_render_pass->vk_command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		in_render_pipeline->vk_pipeline_layout, 
		in_bind_group->layout.index, 
		descriptor_set_count,
		&in_bind_group->vk_descriptor_set, 
		0,		// dynamic offset count
		NULL	// pDynamicOffsets
	);
}

void gpu2_render_pass_draw(Gpu2RenderPass* in_render_pass, u32 vertex_start, u32 vertex_count)
{
	const u32 instance_count = 1;
	const u32 first_instance = 0;
    vkCmdDraw(in_render_pass->vk_command_buffer, vertex_count, instance_count, vertex_start, first_instance);
}

void gpu2_present_drawable(Gpu2Device* in_device, Gpu2CommandBuffer* in_command_buffer, Gpu2Drawable* in_drawable)
{
	//FCS TODO: Proper Synchronization...

	in_device->has_pending_present_info = true;
    in_device->pending_vk_present_info = (VkPresentInfoKHR) {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = 0, //wait_semaphore ? 1 : 0,
        .pWaitSemaphores = NULL, //wait_semaphore ? &wait_semaphore->vk_semaphore : NULL,
        .swapchainCount = 1,
        .pSwapchains = &in_device->swapchain,
        .pImageIndices = &in_device->current_image_index,
        .pResults = NULL,
    };

	gpu2_cmd_vk_image_barrier(
		in_command_buffer, 
		&(Gpu2VkImageBarrier){
			.vk_image = in_drawable->texture.vk_image,
			.src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			.dst_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			.old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
		}
	);
}

bool gpu2_commit_command_buffer(Gpu2Device* in_device, Gpu2CommandBuffer* in_command_buffer)
{
    VK_CHECK(vkEndCommandBuffer(in_command_buffer->vk_command_buffer));

    //VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo vk_submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0, //wait_semaphore ? 1 : 0,
        .pWaitSemaphores = NULL, //wait_semaphore ? &wait_semaphore->vk_semaphore : NULL,
        .pWaitDstStageMask = NULL, //&wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &in_command_buffer->vk_command_buffer,
        .signalSemaphoreCount = 0, //signal_semaphore != NULL ? 1 : 0,
        .pSignalSemaphores = NULL, //signal_semaphore ? &signal_semaphore->vk_semaphore : NULL,
    };

    VK_CHECK(vkQueueSubmit(
		in_device->graphics_queue, 
		1, 
		&vk_submit_info,           
		VK_NULL_HANDLE //signal_fence ? signal_fence->vk_fence : VK_NULL_HANDLE
	));

	if (in_device->has_pending_present_info)
	{
		in_device->has_pending_present_info = false;
		// FCS TODO: MoltenVk returning VK_SUBOPTIMAL_KHR, likely due to invalid swapchain size (need to implement window functions)
		/*VK_CHECK*/(vkQueuePresentKHR(in_device->graphics_queue, &in_device->pending_vk_present_info));
	}
	
	//FCS TODO: Actually handle synchronization 
    vkDeviceWaitIdle(in_device->vk_device);

	return true;
}

// FCS TODO: Track previous layout (from previous image barrier call...)
// FCS TODO: Verify we need viewport flip to match Metal Backend
// FCS TODO: No triangle... 
