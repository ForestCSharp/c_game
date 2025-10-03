#pragma once

#include "types.h"
#include "app/app.h"

typedef struct Gpu2Device Gpu2Device;

typedef struct Gpu2ShaderCreateInfo
{
	const char* filename;
} Gpu2ShaderCreateInfo;
typedef struct Gpu2Shader Gpu2Shader;

//FCS TODO: Buffer Type (Storage / Uniform buffers)
typedef struct Gpu2BufferCreateInfo
{
	bool is_cpu_visible;
	u64 size;
	void* data;
} Gpu2BufferCreateInfo;

typedef struct Gpu2BufferWriteInfo
{
	u64 size;
	void* data;
} Gpu2BufferWriteInfo;

typedef struct Gpu2TextureExtent
{
	u32 width;
	u32 height;
	u32 depth;
} Gpu2TextureExtent;


typedef enum Gpu2Format
{
    GPU2_FORMAT_RGBA8_UNORM, 
    GPU2_FORMAT_BGRA8_UNORM, 
    GPU2_FORMAT_RGBA8_SRGB, 
    GPU2_FORMAT_BGRA8_SRGB, 
    GPU2_FORMAT_RG32_SFLOAT, 
    GPU2_FORMAT_RGB32_SFLOAT, 
    GPU2_FORMAT_RGBA32_SFLOAT, 
    GPU2_FORMAT_D32_SFLOAT, 
    GPU2_FORMAT_R32_SINT, 
} Gpu2Format;

typedef enum Gpu2TextureUsageFlagBits
{
    GPU2_TEXTURE_USAGE_TRANSFER_SRC, 
    GPU2_TEXTURE_USAGE_TRANSFER_DST,
    GPU2_TEXTURE_USAGE_SAMPLED, 
    GPU2_TEXTURE_USAGE_STORAGE, 
    GPU2_TEXTURE_USAGE_COLOR_ATTACHMENT, 
    GPU2_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT, 
} Gpu2TextureUsageFlagBits;
typedef u32 Gpu2TextureUsageFlags;

typedef struct Gpu2TextureCreateInfo
{
	Gpu2Format format;
	Gpu2TextureExtent extent;
	Gpu2TextureUsageFlags usage; 
	bool is_cpu_visible;
} Gpu2TextureCreateInfo;

typedef struct Gpu2Buffer Gpu2Buffer;

typedef struct Gpu2Texture Gpu2Texture;

typedef enum Gpu2BindingType
{
	GPU2_BINDING_TYPE_BUFFER, // Storage buffer
	GPU2_BINDING_TYPE_TEXTURE,
} Gpu2BindingType;

typedef enum Gpu2ShaderStage
{
	GPU2_SHADER_STAGE_VERTEX 	= 1 << 0,
	GPU2_SHADER_STAGE_FRAGMENT 	= 1 << 1,
	GPU2_SHADER_STAGE_COMPUTE 	= 1 << 2,
} Gpu2ShaderStage;
typedef u32 Gpu2ShaderStageFlags;

typedef struct Gpu2ResourceBinding
{
	Gpu2BindingType type;
	Gpu2ShaderStageFlags shader_stages;
} Gpu2ResourceBinding; 

typedef struct Gpu2BindGroupLayoutCreateInfo
{
	u32 index;
	u32 num_bindings;
	Gpu2ResourceBinding* bindings;
	bool update_after_bind;
} Gpu2BindGroupLayoutCreateInfo;
typedef struct Gpu2BindGroupLayout Gpu2BindGroupLayout;

typedef struct Gpu2BufferBinding
{
	Gpu2Buffer* buffer;
} Gpu2BufferBinding;

typedef struct Gpu2TextureBinding
{
	Gpu2Texture* texture;
} Gpu2TextureBinding;

typedef struct Gpu2ResourceWrite
{
	u32 binding_index;
	Gpu2BindingType type;
	union 
	{
		Gpu2BufferBinding buffer_binding;
		Gpu2TextureBinding texture_binding;
	};
} Gpu2ResourceWrite;

typedef struct Gpu2BindGroupCreateInfo 
{
	u32 index;
	Gpu2BindGroupLayout* layout; 
} Gpu2BindGroupCreateInfo;

typedef struct Gpu2BindGroup Gpu2BindGroup;

typedef struct Gpu2BindGroupUpdateInfo
{
	Gpu2BindGroup* bind_group;
	u32 num_writes;
	Gpu2ResourceWrite* writes;
} Gpu2BindGroupUpdateInfo;

static const u32 GPU2_BIND_GROUP_MAX_BINDINGS = 16;

typedef struct Gpu2RenderPipelineCreateInfo
{
	Gpu2Shader* vertex_shader;
	Gpu2Shader* fragment_shader;
	u32 num_bind_group_layouts;
	Gpu2BindGroupLayout** bind_group_layouts; 
	bool depth_test_enabled;
} Gpu2RenderPipelineCreateInfo;
typedef struct Gpu2RenderPipeline Gpu2RenderPipeline;

typedef struct Gpu2CommandBuffer Gpu2CommandBuffer;
typedef struct Gpu2Drawable Gpu2Drawable;

typedef enum Gpu2LoadAction
{
	GPU2_LOAD_ACTION_DONT_CARE,
	GPU2_LOAD_ACTION_LOAD,
	GPU2_LOAD_ACTION_CLEAR,
} Gpu2LoadAction;

typedef enum Gpu2StoreAction
{
	GPU2_STORE_ACTION_DONT_CARE,
	GPU2_STORE_ACTION_STORE,
} Gpu2StoreAction;

typedef struct Gpu2ColorAttachmentDescriptor
{
	Gpu2Texture* texture; 
	float clear_color[4];
	Gpu2LoadAction load_action;
	Gpu2StoreAction store_action;
} Gpu2ColorAttachmentDescriptor;

typedef struct Gpu2DepthAttachmentDescriptor
{
	Gpu2Texture* texture;
	float clear_depth;
	Gpu2LoadAction load_action;
	Gpu2StoreAction store_action;
} Gpu2DepthAttachmentDescriptor;

typedef struct Gpu2RenderPassCreateInfo
{
	u32 num_color_attachments;
	Gpu2ColorAttachmentDescriptor* color_attachments;
	Gpu2DepthAttachmentDescriptor* depth_attachment;
	Gpu2CommandBuffer* command_buffer;
} Gpu2RenderPassCreateInfo;
typedef struct Gpu2RenderPass Gpu2RenderPass;

bool gpu2_create_device(Window* in_window, Gpu2Device* out_device);
void gpu2_destroy_device(Gpu2Device* in_device);

u32 gpu2_get_swapchain_count(Gpu2Device* in_device);

bool gpu2_create_shader(Gpu2Device* in_device, Gpu2ShaderCreateInfo* in_create_info, Gpu2Shader* out_shader);

bool gpu2_create_bind_group_layout(Gpu2Device* in_device, const Gpu2BindGroupLayoutCreateInfo* in_create_info, Gpu2BindGroupLayout* out_bind_group_layout);
bool gpu2_create_bind_group(Gpu2Device* in_device, const Gpu2BindGroupCreateInfo* in_create_info, Gpu2BindGroup* out_bind_group);
void gpu2_update_bind_group(Gpu2Device* in_device, const Gpu2BindGroupUpdateInfo* in_update_info);
void gpu2_destroy_bind_group(Gpu2Device* in_device, Gpu2BindGroup* in_bind_group);
void gpu2_destroy_bind_group_layout(Gpu2Device* in_device, Gpu2BindGroupLayout* in_bind_group_layout);

bool gpu2_create_render_pipeline(Gpu2Device* in_device, Gpu2RenderPipelineCreateInfo* in_create_info, Gpu2RenderPipeline* out_render_pipeline);

bool gpu2_create_buffer(Gpu2Device* in_device, Gpu2BufferCreateInfo* in_create_info, Gpu2Buffer* out_buffer);
void gpu2_write_buffer(Gpu2Device* in_device, Gpu2Buffer* in_buffer, Gpu2BufferWriteInfo* in_write_info);
void* gpu2_map_buffer(Gpu2Device* in_device, Gpu2Buffer* in_buffer);
void gpu2_destroy_buffer(Gpu2Device* in_device, Gpu2Buffer* in_buffer);

bool gpu2_create_texture(Gpu2Device* in_device, Gpu2TextureCreateInfo* in_create_info, Gpu2Texture* out_texture);
void gpu2_destroy_texture(Gpu2Device* in_device, Gpu2Texture* in_texture);

bool gpu2_create_command_buffer(Gpu2Device* in_device, Gpu2CommandBuffer* out_command_buffer);

bool gpu2_get_next_drawable(Gpu2Device* in_device, Gpu2CommandBuffer* in_command_buffer, Gpu2Drawable* out_drawable);
bool gpu2_drawable_get_texture(Gpu2Drawable* in_drawable, Gpu2Texture* out_texture);

void gpu2_begin_render_pass(Gpu2Device* in_device, Gpu2RenderPassCreateInfo* in_create_info, Gpu2RenderPass* out_render_pass);
void gpu2_end_render_pass(Gpu2RenderPass* in_render_pass);

void gpu2_render_pass_set_render_pipeline(Gpu2RenderPass* in_render_pass, Gpu2RenderPipeline* in_render_pipeline);
void gpu2_render_pass_set_bind_group(Gpu2RenderPass* in_render_pass, Gpu2RenderPipeline* in_render_pipeline, Gpu2BindGroup* in_bind_group);
void gpu2_render_pass_draw(Gpu2RenderPass* in_render_pass, u32 vertex_start, u32 vertex_count);

void gpu2_present_drawable(Gpu2Device* in_device, Gpu2CommandBuffer* in_command_buffer, Gpu2Drawable* in_drawable);
bool gpu2_commit_command_buffer(Gpu2Device* in_device, Gpu2CommandBuffer* in_command_buffer);

// FCS TODO: Remove bool returns.
// FCS TODO: Rename "drawable" to something else
// FCS TODO: Rename BindGroup to ResourceGroup
// FCS TODO: Resource/Object destroy functions 
