#pragma once

#include "types.h"
#include "app/app.h"

typedef struct GpuDevice GpuDevice;
typedef struct GpuBuffer GpuBuffer;
typedef struct GpuTexture GpuTexture;
typedef struct GpuSampler GpuSampler;

typedef struct GpuShaderCreateInfo
{
	const char* filename;
} GpuShaderCreateInfo;
typedef struct GpuShader GpuShader;

typedef enum GpuBufferUsageFlagBits
{
	GPU_BUFFER_USAGE_TRANSFER_SRC		= 1 << 0,
	GPU_BUFFER_USAGE_TRANSFER_DST		= 1 << 1,
	GPU_BUFFER_USAGE_STORAGE_BUFFER	= 1 << 2,
} GpuBufferUsageFlagBits;
typedef u32 GpuBufferUsageFlags;

typedef struct GpuBufferCreateInfo
{
	GpuBufferUsageFlags usage;
	bool is_cpu_visible;
	u64 size;
	void* data;
} GpuBufferCreateInfo;

typedef struct GpuBufferWriteInfo
{
	GpuBuffer* buffer;
	u64 size;
	void* data;
} GpuBufferWriteInfo;

typedef struct GpuTextureExtent
{
	u32 width;
	u32 height;
	u32 depth;
} GpuTextureExtent;


typedef enum GpuFormat
{
    GPU_FORMAT_RGBA8_UNORM, 
    GPU_FORMAT_BGRA8_UNORM, 
    GPU_FORMAT_RGBA8_SRGB, 
    GPU_FORMAT_BGRA8_SRGB, 
    GPU_FORMAT_RG32_SFLOAT, 
    GPU_FORMAT_RGB32_SFLOAT, 
    GPU_FORMAT_RGBA32_SFLOAT, 
    GPU_FORMAT_D32_SFLOAT, 
    GPU_FORMAT_R32_SINT, 
} GpuFormat;

typedef enum GpuTextureUsageFlagBits
{
    GPU_TEXTURE_USAGE_TRANSFER_SRC				= 1 << 0, 
    GPU_TEXTURE_USAGE_TRANSFER_DST				= 1 << 1,
    GPU_TEXTURE_USAGE_SAMPLED					= 1 << 2, 
    GPU_TEXTURE_USAGE_STORAGE					= 1 << 3, 
    GPU_TEXTURE_USAGE_COLOR_ATTACHMENT			= 1 << 4, 
    GPU_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT = 1 << 5, 
} GpuTextureUsageFlagBits;
typedef u32 GpuTextureUsageFlags;

typedef enum GpuFilter
{
	GPU_FILTER_NEAREST,
	GPU_FILTER_LINEAR,
} GpuFilter;

typedef enum GpuSamplerAddressMode
{
	GPU_SAMPLER_ADDRESS_MODE_REPEAT,
	GPU_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
	GPU_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,	
	GPU_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
} GpuSamplerAddressMode;

typedef struct GpuSamplerCreateInfo
{
	struct {
		GpuFilter min;
		GpuFilter mag;
	} filters;
	
	struct {
		GpuSamplerAddressMode u;
		GpuSamplerAddressMode v;
		GpuSamplerAddressMode w;
	} address_modes;

    u32 max_anisotropy;
} GpuSamplerCreateInfo;

typedef struct GpuTextureCreateInfo
{
	GpuFormat format;
	GpuTextureExtent extent;
	GpuTextureUsageFlags usage;
	bool is_cpu_visible;
} GpuTextureCreateInfo;

typedef struct GpuTextureWriteInfo
{
	GpuTexture* texture;
	u64 width;
	u64 height;
	void* data;
} GpuTextureWriteInfo;

typedef enum GpuBindingType
{
	GPU_BINDING_TYPE_BUFFER, // Storage buffer
	GPU_BINDING_TYPE_TEXTURE,
	GPU_BINDING_TYPE_SAMPLER,
} GpuBindingType;

typedef enum GpuShaderStage
{
	GPU_SHADER_STAGE_VERTEX 	= 1 << 0,
	GPU_SHADER_STAGE_FRAGMENT 	= 1 << 1,
	GPU_SHADER_STAGE_COMPUTE 	= 1 << 2,
} GpuShaderStage;
typedef u32 GpuShaderStageFlags;

typedef struct GpuResourceBinding
{
	GpuBindingType type;
	GpuShaderStageFlags shader_stages;
} GpuResourceBinding; 

typedef struct GpuBindGroupLayoutCreateInfo
{
	u32 index;
	u32 num_bindings;
	GpuResourceBinding* bindings;
} GpuBindGroupLayoutCreateInfo;
typedef struct GpuBindGroupLayout GpuBindGroupLayout;

typedef struct GpuBufferBinding
{
	GpuBuffer* buffer;
} GpuBufferBinding;

typedef struct GpuTextureBinding
{
	GpuTexture* texture;
} GpuTextureBinding;

typedef struct GpuSamplerBinding
{
	GpuSampler* sampler;
} GpuSamplerBinding;

typedef struct GpuResourceWrite
{
	u32 binding_index;
	GpuBindingType type;
	union 
	{
		GpuBufferBinding buffer_binding;
		GpuTextureBinding texture_binding;
		GpuSamplerBinding sampler_binding;
	};
} GpuResourceWrite;

typedef struct GpuBindGroupCreateInfo 
{
	GpuBindGroupLayout* layout; 
} GpuBindGroupCreateInfo;

typedef struct GpuBindGroup GpuBindGroup;

typedef struct GpuBindGroupUpdateInfo
{
	GpuBindGroup* bind_group;
	u32 num_writes;
	GpuResourceWrite* writes;
} GpuBindGroupUpdateInfo;

static const u32 GPU_BIND_GROUP_MAX_BINDINGS = 16;

typedef enum GpuPolygonMode
{
	GPU_POLYGON_MODE_FILL	= 0,
	GPU_POLYGON_MODE_LINE	= 1,
} GpuPolygonMode;

typedef struct GpuRenderPipelineCreateInfo
{
	GpuShader* vertex_shader;
	GpuShader* fragment_shader;
	u32 num_bind_group_layouts;
	GpuBindGroupLayout** bind_group_layouts; 
	GpuPolygonMode polygon_mode;
	bool depth_test_enabled;
} GpuRenderPipelineCreateInfo;
typedef struct GpuRenderPipeline GpuRenderPipeline;

typedef struct GpuCommandBuffer GpuCommandBuffer;
typedef struct GpuDrawable GpuDrawable;

typedef enum GpuLoadAction
{
	GPU_LOAD_ACTION_DONT_CARE,
	GPU_LOAD_ACTION_LOAD,
	GPU_LOAD_ACTION_CLEAR,
} GpuLoadAction;

typedef enum GpuStoreAction
{
	GPU_STORE_ACTION_DONT_CARE,
	GPU_STORE_ACTION_STORE,
} GpuStoreAction;

typedef struct GpuColorAttachmentDescriptor
{
	GpuTexture* texture; 
	float clear_color[4];
	GpuLoadAction load_action;
	GpuStoreAction store_action;
} GpuColorAttachmentDescriptor;

typedef struct GpuDepthAttachmentDescriptor
{
	GpuTexture* texture;
	float clear_depth;
	GpuLoadAction load_action;
	GpuStoreAction store_action;
} GpuDepthAttachmentDescriptor;

typedef struct GpuRenderPassCreateInfo
{
	u32 num_color_attachments;
	GpuColorAttachmentDescriptor* color_attachments;
	GpuDepthAttachmentDescriptor* depth_attachment;
	GpuCommandBuffer* command_buffer;
} GpuRenderPassCreateInfo;
typedef struct GpuRenderPass GpuRenderPass;

void gpu_create_device(Window* in_window, GpuDevice* out_device);
void gpu_destroy_device(GpuDevice* in_device);

u32 gpu_get_swapchain_count(GpuDevice* in_device);

void gpu_create_shader(GpuDevice* in_device, GpuShaderCreateInfo* in_create_info, GpuShader* out_shader);
void gpu_destroy_shader(GpuDevice* in_device, GpuShader* in_shader);

bool gpu_create_bind_group_layout(GpuDevice* in_device, const GpuBindGroupLayoutCreateInfo* in_create_info, GpuBindGroupLayout* out_bind_group_layout);
bool gpu_create_bind_group(GpuDevice* in_device, const GpuBindGroupCreateInfo* in_create_info, GpuBindGroup* out_bind_group);
void gpu_update_bind_group(GpuDevice* in_device, const GpuBindGroupUpdateInfo* in_update_info);
void gpu_destroy_bind_group(GpuDevice* in_device, GpuBindGroup* in_bind_group);
void gpu_destroy_bind_group_layout(GpuDevice* in_device, GpuBindGroupLayout* in_bind_group_layout);

bool gpu_create_render_pipeline(GpuDevice* in_device, GpuRenderPipelineCreateInfo* in_create_info, GpuRenderPipeline* out_render_pipeline);

void gpu_create_buffer(GpuDevice* in_device, const GpuBufferCreateInfo* in_create_info, GpuBuffer* out_buffer);
void gpu_write_buffer(GpuDevice* in_device, const GpuBufferWriteInfo* in_write_info);
void* gpu_map_buffer(GpuDevice* in_device, GpuBuffer* in_buffer);
void gpu_unmap_buffer(GpuDevice* in_device, GpuBuffer* in_buffer);
void gpu_destroy_buffer(GpuDevice* in_device, GpuBuffer* in_buffer);

void gpu_create_texture(GpuDevice* in_device, const GpuTextureCreateInfo* in_create_info, GpuTexture* out_texture);
void gpu_write_texture(GpuDevice* in_device, const GpuTextureWriteInfo* in_upload_info);
void gpu_destroy_texture(GpuDevice* in_device, GpuTexture* in_texture);

void gpu_create_sampler(GpuDevice* in_device, const GpuSamplerCreateInfo* in_create_info, GpuSampler* out_sampler);
void gpu_destroy_sampler(GpuDevice* in_device, GpuSampler* in_sampler);

bool gpu_create_command_buffer(GpuDevice* in_device, GpuCommandBuffer* out_command_buffer);

bool gpu_get_next_drawable(GpuDevice* in_device, GpuCommandBuffer* in_command_buffer, GpuDrawable* out_drawable);
bool gpu_drawable_get_texture(GpuDrawable* in_drawable, GpuTexture* out_texture);

void gpu_begin_render_pass(GpuDevice* in_device, GpuRenderPassCreateInfo* in_create_info, GpuRenderPass* out_render_pass);
void gpu_end_render_pass(GpuRenderPass* in_render_pass);

void gpu_render_pass_set_render_pipeline(GpuRenderPass* in_render_pass, GpuRenderPipeline* in_render_pipeline);
void gpu_render_pass_set_bind_group(GpuRenderPass* in_render_pass, GpuRenderPipeline* in_render_pipeline, GpuBindGroup* in_bind_group);
void gpu_render_pass_draw(GpuRenderPass* in_render_pass, u32 vertex_start, u32 vertex_count);

void gpu_present_drawable(GpuDevice* in_device, GpuCommandBuffer* in_command_buffer, GpuDrawable* in_drawable);
bool gpu_commit_command_buffer(GpuDevice* in_device, GpuCommandBuffer* in_command_buffer);

// Helper Functions
u32 gpu_format_stride(GpuFormat format);
const char* gpu_get_api_name();

// FCS TODO: Remove bool returns on all functions
// FCS TODO: Rename "drawable" to something else
