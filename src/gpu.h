#pragma once

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#endif

#include <vulkan/vulkan.h>
#include "window/window.h"
#include "types.h"

typedef enum GpuFormat {
    GPU_FORMAT_RGBA8_UNORM = VK_FORMAT_R8G8B8A8_UNORM,
    GPU_FORMAT_BGRA8_UNORM = VK_FORMAT_B8G8R8A8_UNORM,
    GPU_FORMAT_RGBA8_SRGB = VK_FORMAT_R8G8B8_SRGB,
    GPU_FORMAT_BGRA8_SRGB = VK_FORMAT_B8G8R8A8_SRGB,
    GPU_FORMAT_RG32_SFLOAT = VK_FORMAT_R32G32_SFLOAT,
    GPU_FORMAT_RGB32_SFLOAT = VK_FORMAT_R32G32B32_SFLOAT,
    GPU_FORMAT_RGBA32_SFLOAT = VK_FORMAT_R32G32B32A32_SFLOAT,
    GPU_FORMAT_D32_SFLOAT = VK_FORMAT_D32_SFLOAT,
	GPU_FORMAT_R32_SINT = VK_FORMAT_R32_SINT,
    //TODO: Add Formats
} GpuFormat;

typedef enum GpuLoadOp {
    GPU_LOAD_OP_LOAD = VK_ATTACHMENT_LOAD_OP_LOAD,
    GPU_LOAD_OP_CLEAR = VK_ATTACHMENT_LOAD_OP_CLEAR,
    GPU_LOAD_OP_DONT_CARE = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
} GpuLoadOp;

typedef enum GpuStoreOp {
    GPU_STORE_OP_STORE = VK_ATTACHMENT_STORE_OP_STORE,
    GPU_STORE_OP_DONT_CARE = VK_ATTACHMENT_STORE_OP_DONT_CARE,
} GpuStoreOp;

typedef enum GpuImageLayout {
    GPU_IMAGE_LAYOUT_UNDEFINED = VK_IMAGE_LAYOUT_UNDEFINED,
    GPU_IMAGE_LAYOUT_GENERAL = VK_IMAGE_LAYOUT_GENERAL,
    GPU_IMAGE_LAYOUT_COLOR_ATACHMENT = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    GPU_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    GPU_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
    GPU_IMAGE_LAYOUT_SHADER_READ_ONLY = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    GPU_IMAGE_LAYOUT_TRANSFER_SRC = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    GPU_IMAGE_LAYOUT_TRANSFER_DST = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    GPU_IMAGE_LAYOUT_PREINITIALIZED = VK_IMAGE_LAYOUT_PREINITIALIZED,
    GPU_IMAGE_LAYOUT_DEPTH_ATTACHMENT = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    GPU_IMAGE_LAYOUT_PRESENT_SRC = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
} GpuImageLayout;

typedef enum {
    GPU_MEMORY_PROPERTY_DEVICE_LOCAL = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    GPU_MEMORY_PROPERTY_HOST_VISIBLE = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
    GPU_MEMORY_PROPERTY_HOST_COHERENT = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
} GpuMemoryPropertyFlagsBits;
typedef u32 GpuMemoryPropertyFlags;

typedef enum GpuBufferUsageFlagBits {
    GPU_BUFFER_USAGE_TRANSFER_SRC = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    GPU_BUFFER_USAGE_TRANSFER_DST = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    GPU_BUFFER_USAGE_UNIFORM_BUFFER = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    GPU_BUFFER_USAGE_STORAGE_BUFFER = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    GPU_BUFFER_USAGE_INDEX_BUFFER = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    GPU_BUFFER_USAGE_VERTEX_BUFFER = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
} GpuBufferUsageFlagBits;
typedef u32 GpuBufferUsageFlags;

typedef enum GpuImageUsageFlagBits {
    GPU_IMAGE_USAGE_TRANSFER_SRC = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    GPU_IMAGE_USAGE_TRANSFER_DST = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    GPU_IMAGE_USAGE_SAMPLED = VK_IMAGE_USAGE_SAMPLED_BIT,
    GPU_IMAGE_USAGE_STORAGE = VK_IMAGE_USAGE_STORAGE_BIT,
    GPU_IMAGE_USAGE_COLOR_ATTACHMENT = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    GPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
} GpuImageUsageFlagBits;
typedef u32 GpuImageUsageFlags;

typedef enum GpuImageViewType {
    GPU_IMAGE_VIEW_1D = VK_IMAGE_VIEW_TYPE_1D,
    GPU_IMAGE_VIEW_2D = VK_IMAGE_VIEW_TYPE_2D,
    GPU_IMAGE_VIEW_3D = VK_IMAGE_VIEW_TYPE_3D,
    GPU_IMAGE_VIEW_CUBE = VK_IMAGE_VIEW_TYPE_CUBE,
} GpuImageViewType;

typedef enum GpuImageAspect {
    GPU_IMAGE_ASPECT_COLOR = VK_IMAGE_ASPECT_COLOR_BIT,
    GPU_IMAGE_ASPECT_DEPTH = VK_IMAGE_ASPECT_DEPTH_BIT,
    GPU_IMAGE_ASPECT_STENCIL = VK_IMAGE_ASPECT_STENCIL_BIT,
} GpuImageAspect;

typedef enum GpuFilter {
    GPU_FILTER_NEAREST = VK_FILTER_NEAREST,
    GPU_FILTER_LINEAR = VK_FILTER_LINEAR,
    GPU_FILTER_CUBIC_IMG = VK_FILTER_CUBIC_IMG,
} GpuFilter;

typedef enum GpuDescriptorType {
    GPU_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    GPU_DESCRIPTOR_TYPE_UNIFORM_BUFFER = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
} GpuDescriptorType;

typedef enum GpuShaderStageFlagBits {
    GPU_SHADER_STAGE_VERTEX = VK_SHADER_STAGE_VERTEX_BIT,
    GPU_SHADER_STAGE_FRAGMENT = VK_SHADER_STAGE_FRAGMENT_BIT,
    GPU_SHADER_STAGE_COMPUTE = VK_SHADER_STAGE_COMPUTE_BIT,
    GPU_SHADER_STAGE_ALL_GRAPHICS = VK_SHADER_STAGE_ALL_GRAPHICS,
} GpuShaderStageFlagBits;
typedef u32 GpuShaderStageFlags;

typedef enum GpuPipelineStageFlagBits {
    GPU_PIPELINE_STAGE_TOP_OF_PIPE = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    GPU_PIPELINE_STAGE_DRAW_INDIRECT = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
    GPU_PIPELINE_STAGE_VERTEX_INPUT = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
    GPU_PIPELINE_STAGE_VERTEX_SHADER = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
    GPU_PIPELINE_STAGE_FRAGMENT_SHADER = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    GPU_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    GPU_PIPELINE_STAGE_LATE_FRAGMENT_TESTS = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
    GPU_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    GPU_PIPELINE_STAGE_COMPUTE_SHADER = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    GPU_PIPELINE_STAGE_TRANSFER = VK_PIPELINE_STAGE_TRANSFER_BIT,
    GPU_PIPELINE_STAGE_BOTTOM_OF_PIPE = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
    GPU_PIPELINE_STAGE_HOST = VK_PIPELINE_STAGE_HOST_BIT,
    GPU_PIPELINE_STAGE_ALL_GRAPHICS = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    GPU_PIPELINE_STAGE_ALL_COMPUTE = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
} GpuPipelineStage;
typedef u32 GpuPipelineStageFlags;

typedef struct GpuMemoryType {
    struct GpuMemoryBlock* memory_blocks;
} GpuMemoryType;

typedef struct GpuMemoryBlock {
    u64 size;
    struct GpuMemoryRegion* free_list;
    struct GpuMemoryRegion* used_list;
    struct GpuMemoryType* owning_type;
    VkDeviceMemory vk_memory;
} GpuMemoryBlock;

typedef struct GpuMemoryRegion {
    u64 padding; //Padding on front of region (for alignment requirement)
    u64 offset;
    u64 size;
    struct GpuMemoryBlock* owning_block;
    struct GpuMemory* alloc_ref; //Null in free list, non-null in used_list
} GpuMemoryRegion;

typedef struct GpuMemory {
    GpuMemoryRegion* memory_region; 
    //FIXME: if we make above a reference, reallocing can break. Instead our external handles can be 3 indices: mem_type, mem_block, used_list_index
    GpuMemoryPropertyFlags memory_properties;
} GpuMemory;

typedef struct GpuBuffer {
    VkBuffer  vk_buffer;
    GpuMemory* memory;
} GpuBuffer;

typedef struct GpuImageCreateInfo {
    u32 dimensions[3]; //determines VkImageType
    GpuFormat format;
    u32 mip_levels;
    GpuImageUsageFlags usage;
    GpuMemoryPropertyFlags memory_properties;
} GpuImageCreateInfo;

typedef struct GpuImage {
    VkImage   vk_image;
    GpuMemory* memory; //If null, memory is managed elsewhere (such as swapchains)
    GpuFormat format;
} GpuImage;

typedef struct GpuImageViewCreateInfo {
    GpuImage* image;
    GpuImageViewType type;
    GpuFormat format;
    GpuImageAspect aspect;
} GpuImageViewCreateInfo;

typedef struct GpuImageView {
    VkImageView vk_image_view;
} GpuImageView;

typedef struct GpuSamplerCreateInfo {
    GpuFilter mag_filter;
    GpuFilter min_filter;
    u32  max_anisotropy;
} GpuSamplerCreateInfo;

typedef struct GpuSampler {
    VkSampler vk_sampler;
} GpuSampler;

typedef struct GpuDescriptorSet {
    VkDescriptorPool vk_descriptor_pool; //One pool per set for now
    VkDescriptorSet vk_descriptor_set;
} GpuDescriptorSet;

typedef struct GpuDescriptorWriteImage {
    GpuImageView* image_view;
    GpuSampler* sampler;
    GpuImageLayout layout;
} GpuDescriptorWriteImage;

typedef struct GpuDescriptorWriteBuffer {
    GpuBuffer* buffer;
    u64 offset;
    u64 range;
} GpuDescriptorWriteBuffer;

typedef struct GpuDescriptorBinding {
    u32 binding;
    GpuDescriptorType type;
    GpuShaderStageFlags stage_flags;
} GpuDescriptorBinding;

typedef struct GpuDescriptorWrite{
    GpuDescriptorBinding* binding_desc;
    GpuDescriptorWriteImage* image_write;
    GpuDescriptorWriteBuffer* buffer_write;
} GpuDescriptorWrite;

typedef struct GpuAttachmentDesc {
    GpuFormat format;
    GpuLoadOp load_op;
    GpuStoreOp store_op;
    GpuImageLayout initial_layout;
    GpuImageLayout final_layout;
} GpuAttachmentDesc;

typedef struct GpuRenderPass {
    VkRenderPass vk_render_pass;
} GpuRenderPass;

typedef struct GpuShaderModule {
    VkShaderModule vk_shader_module;
} GpuShaderModule;

typedef struct GpuDescriptorLayout {
    u32 binding_count;
    GpuDescriptorBinding* bindings;
} GpuDescriptorLayout;

typedef struct GpuPipelineLayout {
    VkPipelineLayout vk_pipeline_layout;
    VkDescriptorSetLayout vk_descriptor_set_layout;
    GpuDescriptorLayout descriptor_layout;
} GpuPipelineLayout;

typedef struct GpuPipelineDepthStencilState {
    bool depth_test;
    bool depth_write;
} GpuPipelineDepthStencilState;


/*  For usage with Dynamic Rendering. Either this OR a render pass 
    should be referenced in GpuGraphicsPipelineCreateInfo when 
    creating a pipeline */
typedef struct GpuPipelineRenderingCreateInfo {
    u32 color_attachment_count;
    GpuFormat* color_formats;
    GpuFormat depth_format;
    GpuFormat stencil_format;

} GpuPipelineRenderingCreateInfo;

typedef struct GpuGraphicsPipelineCreateInfo {
    GpuShaderModule* vertex_module;
    GpuShaderModule* fragment_module;
	GpuRenderPass* render_pass;
    GpuPipelineRenderingCreateInfo* rendering_info;
	GpuPipelineLayout* layout;
    u32 num_attributes;
    GpuFormat* attribute_formats;
    GpuPipelineDepthStencilState depth_stencil;
    bool enable_color_blending; //FCS TODO: need to pass in per-color attachment for this
	// TODO: More FixedFunction State
} GpuGraphicsPipelineCreateInfo;

typedef struct GpuPipeline {
    VkPipeline vk_pipeline;
} GpuPipeline;

typedef struct GpuFramebufferCreateInfo {
    GpuRenderPass* render_pass;
    u32 width;
    u32 height;
    u32 attachment_count;
    GpuImageView* attachments;
} GpuFramebufferCreateInfo;

typedef struct GpuFramebuffer {
    VkFramebuffer vk_framebuffer;
    u32 width;
    u32 height;
} GpuFramebuffer;

typedef struct GpuCommandBuffer {
    VkCommandBuffer vk_command_buffer;
} GpuCommandBuffer;

typedef struct GpuClearDepthStencil {
    float depth;
    u32 stencil;
} GpuClearDepthStencil;

typedef union GpuClearValue {
    float clear_color[4];
    GpuClearDepthStencil depth_stencil;
} GpuClearValue;

typedef struct GpuRenderingAttachmentInfo {
    GpuImageView* image_view;
    GpuImageLayout image_layout;
    GpuLoadOp load_op;
    GpuStoreOp store_op;
    GpuClearValue clear_value;
} GpuRenderingAttachmentInfo;

typedef struct GpuRenderingInfo {
    u32 render_width;
    u32 render_height;
    u32 color_attachment_count;
    GpuRenderingAttachmentInfo* color_attachments;
    GpuRenderingAttachmentInfo* depth_attachment;
    GpuRenderingAttachmentInfo* stencil_attachment;
} GpuRenderingInfo;

typedef struct GpuRenderPassBeginInfo {
    GpuRenderPass* render_pass;
    GpuFramebuffer* framebuffer; 
    u32 num_clear_values;
    const GpuClearValue* clear_values;
} GpuRenderPassBeginInfo;

typedef struct GpuFence {
    VkFence vk_fence;
} GpuFence;

typedef struct GpuSemaphore {
    VkSemaphore vk_semaphore;
} GpuSemaphore;

typedef struct GpuViewport {
    float x, y, width, height, min_depth, max_depth;
} GpuViewport;

typedef struct GpuImageBarrier
{
    GpuImage* image;
    GpuPipelineStageFlags src_stage;
    GpuPipelineStageFlags dst_stage;
    GpuImageLayout old_layout;
    GpuImageLayout new_layout;
} GpuImageBarrier;

typedef struct {
    //Main Vulkan Objects
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;

    VkQueue graphics_queue;
    VkCommandPool graphics_command_pool;

    //Debug Functionality
    VkDebugUtilsMessengerEXT debug_messenger;
    PFN_vkSetDebugUtilsObjectNameEXT pfn_set_object_name;

    //Surface/Swapchain Data
    VkSurfaceKHR surface;
    VkPresentModeKHR present_mode;
    VkSurfaceFormatKHR surface_format;
    VkSwapchainKHR swapchain;
    u32 swapchain_image_count;
    GpuImage* swapchain_images;
    GpuImageView* swapchain_image_views;

    //Memory
    int num_memory_types;
    GpuMemoryType* memory_types;
    VkPhysicalDeviceMemoryProperties vk_memory_properties;

} GpuContext;

GpuContext gpu_create_context(const Window* const window);
void       gpu_destroy_context(GpuContext* context);
void       gpu_wait_idle(GpuContext* context);

void       gpu_resize_swapchain(GpuContext* context, const Window* const window);
u32   gpu_acquire_next_image(GpuContext* context, GpuSemaphore* semaphore);

GpuBuffer gpu_create_buffer(GpuContext* context, GpuBufferUsageFlags buffer_usage, GpuMemoryPropertyFlags memory_properties, u64 buffer_size, const char* debug_name);
void      gpu_destroy_buffer(GpuContext* context, GpuBuffer* buffer);
void      gpu_upload_buffer(GpuContext* context, GpuBuffer* buffer, u64 upload_size, void* upload_data);

GpuImage gpu_create_image(GpuContext* context, GpuImageCreateInfo* create_info, const char* debug_name);
void     gpu_destroy_image(GpuContext* context, GpuImage* image);
void     gpu_upload_image(GpuContext* context, GpuImage* image, u64 upload_width, u64 upload_height, void* upload_data);

GpuImageView gpu_create_image_view(GpuContext* context, GpuImageViewCreateInfo* create_info);
void         gpu_destroy_image_view(GpuContext* context, GpuImageView* image_view);

GpuSampler gpu_create_sampler(GpuContext* context, GpuSamplerCreateInfo* create_info);
void       gpu_destroy_sampler(GpuContext* context, GpuSampler* sampler);

//FIXME: consolidate final two args somehow
GpuDescriptorSet gpu_create_descriptor_set(GpuContext* context, GpuPipelineLayout* pipeline_layout);
void             gpu_destroy_descriptor_set(GpuContext* context, GpuDescriptorSet* descriptor_set);
void             gpu_write_descriptor_set(GpuContext* context, GpuDescriptorSet* descriptor_set, u32 write_count, GpuDescriptorWrite* descriptor_writes);

void       gpu_map_memory(GpuContext* context, GpuMemory* memory, u64 offset, u64 size, void** ppData);
void       gpu_unmap_memory(GpuContext* context, GpuMemory* memory);

GpuShaderModule gpu_create_shader_module(GpuContext* context, u64 code_size, const u32* code);
void            gpu_destroy_shader_module(GpuContext* context, GpuShaderModule* shader_module);

GpuRenderPass gpu_create_render_pass(GpuContext* context, u32 color_attachment_count, GpuAttachmentDesc* color_attachments, GpuAttachmentDesc* depth_stencil_attachment);
void          gpu_destroy_render_pass(GpuContext* context, GpuRenderPass* render_pass);

GpuPipelineLayout gpu_create_pipeline_layout(GpuContext* context, GpuDescriptorLayout* descriptor_layout);
void              gpu_destroy_pipeline_layout(GpuContext* context, GpuPipelineLayout* layout);

GpuPipeline gpu_create_graphics_pipeline(GpuContext* context, GpuGraphicsPipelineCreateInfo* create_info);
void        gpu_destroy_pipeline(GpuContext* context, GpuPipeline* pipeline);

GpuFramebuffer gpu_create_framebuffer(GpuContext* context, GpuFramebufferCreateInfo* create_info);
void           gpu_destroy_framebuffer(GpuContext* context, GpuFramebuffer* framebuffer);

GpuCommandBuffer gpu_create_command_buffer(GpuContext* context);
void             gpu_free_command_buffer(GpuContext* context, GpuCommandBuffer* command_buffer);

void gpu_begin_command_buffer(GpuCommandBuffer* command_buffer);
void gpu_end_command_buffer(GpuCommandBuffer* command_buffer);

void gpu_cmd_begin_rendering(GpuCommandBuffer* command_buffer, GpuRenderingInfo* rendering_info);
void gpu_cmd_end_rendering(GpuCommandBuffer* command_buffer);

void gpu_cmd_begin_render_pass(GpuCommandBuffer* command_buffer, GpuRenderPassBeginInfo* begin_info);
void gpu_cmd_end_render_pass(GpuCommandBuffer* command_buffer);

void gpu_cmd_bind_pipeline(GpuCommandBuffer* command_buffer, GpuPipeline* pipeline);

void gpu_cmd_bind_index_buffer(GpuCommandBuffer* command_buffer, GpuBuffer* index_buffer);
//TODO: multiple vertex buffer bindings
void gpu_cmd_bind_vertex_buffer(GpuCommandBuffer* command_buffer, GpuBuffer* vertex_buffer);
void gpu_cmd_bind_descriptor_set(GpuCommandBuffer* command_buffer, GpuPipelineLayout* layout, GpuDescriptorSet* descriptor_set);

//TODO: non-indexed-draw, instance_count for both draw types
void gpu_cmd_draw_indexed(GpuCommandBuffer* command_buffer, u32 index_count);
void gpu_cmd_draw(GpuCommandBuffer* command_buffer, u32 vertex_count);

void gpu_cmd_set_viewport(GpuCommandBuffer* command_buffer, GpuViewport* viewport);

void gpu_cmd_copy_buffer(GpuCommandBuffer* command_buffer, GpuBuffer* src_buffer, GpuBuffer* dst_buffer, u64 size);
void gpu_cmd_copy_buffer_to_image(GpuCommandBuffer* command_buffer, GpuBuffer* src_buffer, GpuImage* dst_image, GpuImageLayout image_layout, u64 width, u64 height);

void gpu_cmd_image_barrier(GpuCommandBuffer* command_buffer, GpuImageBarrier* image_barrier);
//FCS TODO: gpu_cmd_buffer_barrier
//FCS TODO: gpu_cmd_memory_barrier

//TODO: queue argument
void gpu_queue_submit(GpuContext* context, GpuCommandBuffer* command_buffer, GpuSemaphore* wait_semaphore, GpuSemaphore* signal_semaphore, GpuFence* signal_fence);
void gpu_queue_present(GpuContext* context, u32 image_index, GpuSemaphore* wait_semaphore);

GpuFence gpu_create_fence(GpuContext* context, bool signaled);
void     gpu_destroy_fence(GpuContext* context, GpuFence* fence);
void     gpu_wait_for_fence(GpuContext* context, GpuFence* fence);
void     gpu_reset_fence(GpuContext* context, GpuFence* fence);

GpuSemaphore gpu_create_semaphore(GpuContext* context);
void         gpu_destroy_semaphore(GpuContext* context, GpuSemaphore* semaphore);
