#pragma once

#include "gpu.h"

//FCS TODO: Need to prevent double-registers
//FCS TODO: need "Invalid" resource for each type, which we write to freed-up slots

static const u32 BINDLESS_COUNT = 1000;
static const u32 UNIFORM_BINDING = 0;
static const u32 STORAGE_BINDING = 1;
static const u32 TEXTURE_BINDING = 2;

typedef struct { u32 idx; } UniformBufferHandle;
typedef struct { u32 idx; } StorageBufferHandle;
typedef struct { u32 idx; } TextureHandle;

typedef struct BindlessBuffer
{
	bool is_valid;
	GpuBuffer buffer;
} BindlessBuffer;

typedef struct BindlessTexture
{
	bool is_valid;
	GpuImage image;
	GpuImageView image_view;
	GpuSampler sampler;
} BindlessTexture;

// Bindless Resource Manager. Can register resources for ease-of-access from a single-bound descriptor set
// Takes ownership over registered resources, handling their destruction on unregister
typedef struct BindlessResourceManager
{
	// Descriptor set and layout
	GpuDescriptorLayout 			descriptor_layout;
	GpuDescriptorSet				descriptor_set;

	// Registered uniform buffers
	sbuffer(BindlessBuffer)			uniform_buffers;
	sbuffer(UniformBufferHandle)	uniform_buffers_free_list;

	// Registered storage buffers
	sbuffer(BindlessBuffer)			storage_buffers;
	sbuffer(StorageBufferHandle)	storage_buffers_free_list;

	// Registered textures
	sbuffer(BindlessTexture)		textures;
	sbuffer(TextureHandle)			textures_free_list;

} BindlessResourceManager;

void bindless_resource_manager_create(GpuContext* in_gpu_context, BindlessResourceManager* out_manager)
{
	*out_manager = (BindlessResourceManager){};

	static const GpuDescriptorBindingFlags BINDING_FLAGS = GPU_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND | GPU_DESCRIPTOR_BINDING_PARTIALLY_BOUND;

	const GpuDescriptorLayoutCreateInfo bindless_descriptor_layout_create_info = {
		.set_number = 0,
		.binding_count = 3,
		.bindings = (GpuDescriptorBinding[3]) {
			{ 
				.binding = UNIFORM_BINDING, 
				.count = BINDLESS_COUNT, 
				.type = GPU_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
				.stage_flags = GPU_SHADER_STAGE_ALL_GRAPHICS, 
				.binding_flags = BINDING_FLAGS, 
			},
			{ 
				.binding = STORAGE_BINDING, 
				.count = BINDLESS_COUNT, 
				.type = GPU_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
				.stage_flags = GPU_SHADER_STAGE_ALL_GRAPHICS, 
				.binding_flags = BINDING_FLAGS, 
			},
			{ 
				.binding = TEXTURE_BINDING, 
				.count = BINDLESS_COUNT, 
				.type = GPU_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
				.stage_flags = GPU_SHADER_STAGE_ALL_GRAPHICS, 
				.binding_flags = BINDING_FLAGS, 
			},
		},
	};
	out_manager->descriptor_layout = gpu_create_descriptor_layout(in_gpu_context, &bindless_descriptor_layout_create_info);	
	out_manager->descriptor_set = gpu_create_descriptor_set(in_gpu_context, &out_manager->descriptor_layout);
}

UniformBufferHandle bindless_resource_manager_register_uniform_buffer(GpuContext* in_gpu_context, BindlessResourceManager* in_manager, GpuBuffer* in_buffer)
{
	UniformBufferHandle out_handle = {};

	if (sb_count(in_manager->uniform_buffers_free_list) > 0)
	{
		out_handle = in_manager->uniform_buffers_free_list[0];
		sb_del(in_manager->uniform_buffers_free_list, 0);	
	}
	else
	{
		out_handle.idx = sb_count(in_manager->uniform_buffers);
		sb_push(in_manager->uniform_buffers, (BindlessBuffer) {});
	}

	in_manager->uniform_buffers[out_handle.idx].is_valid = true;
	in_manager->uniform_buffers[out_handle.idx].buffer = *in_buffer;
	
	GpuDescriptorWrite descriptor_write = {	
		.index = out_handle.idx,
		.binding_desc = &in_manager->descriptor_layout.bindings[UNIFORM_BINDING],
		.buffer_write = &(GpuDescriptorWriteBuffer) {
			.buffer = in_buffer,
			.offset = 0,
			.range = in_buffer->memory->memory_region->size,
		},
	};

	gpu_update_descriptor_set(in_gpu_context, &in_manager->descriptor_set, 1, &descriptor_write);

	return out_handle;
}

void bindless_resource_manager_unregister_uniform_buffer(GpuContext* in_gpu_context, BindlessResourceManager* in_manager, UniformBufferHandle in_handle)
{	
	assert(in_handle.idx < sb_count(in_manager->uniform_buffers));
	assert(in_manager->uniform_buffers[in_handle.idx].is_valid);

	// Destroy buffer
	gpu_destroy_buffer(in_gpu_context, &in_manager->uniform_buffers[in_handle.idx].buffer);

	// Reset entry
	in_manager->uniform_buffers[in_handle.idx] = (BindlessBuffer) { .is_valid = false, .buffer = (GpuBuffer) {}};

	//FCS TODO: Write "invalid" resource here...

	// Add Index to free list
	sb_push(in_manager->uniform_buffers_free_list, in_handle);
}

StorageBufferHandle bindless_resource_manager_register_storage_buffer(GpuContext* in_gpu_context, BindlessResourceManager* in_manager, GpuBuffer* in_buffer)
{
	StorageBufferHandle out_handle = {};

	if (sb_count(in_manager->storage_buffers_free_list) > 0)
	{
		out_handle = in_manager->storage_buffers_free_list[0];
		sb_del(in_manager->storage_buffers_free_list, 0);	
	}
	else
	{
		out_handle.idx = sb_count(in_manager->storage_buffers);
		sb_push(in_manager->storage_buffers, (BindlessBuffer) {});
	}

	in_manager->storage_buffers[out_handle.idx].is_valid = true;
	in_manager->storage_buffers[out_handle.idx].buffer = *in_buffer;
	
	GpuDescriptorWrite descriptor_write = {	
		.index = out_handle.idx,
		.binding_desc = &in_manager->descriptor_layout.bindings[STORAGE_BINDING],
		.buffer_write = &(GpuDescriptorWriteBuffer) {
			.buffer = in_buffer,
			.offset = 0,
			.range = in_buffer->memory->memory_region->size,
		},
	};

	gpu_update_descriptor_set(in_gpu_context, &in_manager->descriptor_set, 1, &descriptor_write);

	return out_handle;
}

void bindless_resource_manager_unregister_storage_buffer(GpuContext* in_gpu_context, BindlessResourceManager* in_manager, StorageBufferHandle in_handle)
{
	assert(in_handle.idx < sb_count(in_manager->storage_buffers));
	assert(in_manager->storage_buffers[in_handle.idx].is_valid);

	// Destroy buffer
	gpu_destroy_buffer(in_gpu_context, &in_manager->storage_buffers[in_handle.idx].buffer);

	// Reset Entry
	in_manager->storage_buffers[in_handle.idx] = (BindlessBuffer) { .is_valid = false, .buffer = (GpuBuffer) {}};

	//FCS TODO: Write "invalid" resource here...

	// Add Index to free list
	sb_push(in_manager->storage_buffers_free_list, in_handle);
}

TextureHandle bindless_resource_manager_register_texture(GpuContext* in_gpu_context, BindlessResourceManager* in_manager, BindlessTexture in_bindless_texture)
{
	TextureHandle out_handle = {};

	if (sb_count(in_manager->textures_free_list) > 0)
	{
		out_handle = in_manager->textures_free_list[0];
		sb_del(in_manager->textures_free_list, 0);	
	}
	else
	{
		out_handle.idx = sb_count(in_manager->textures);
		sb_push(in_manager->textures, (BindlessTexture) {});
	}

	in_manager->textures[out_handle.idx] = in_bindless_texture;
	in_manager->textures[out_handle.idx].is_valid = true;
	
	GpuDescriptorWrite descriptor_write = {	
		.index = out_handle.idx,
		.binding_desc = &in_manager->descriptor_layout.bindings[TEXTURE_BINDING],
		.image_write = &(GpuDescriptorWriteImage){
			.image_view = &in_manager->textures[out_handle.idx].image_view,
			.sampler = &in_manager->textures[out_handle.idx].sampler,
			.layout = GPU_IMAGE_LAYOUT_SHADER_READ_ONLY,
		},
	};

	gpu_update_descriptor_set(in_gpu_context, &in_manager->descriptor_set, 1, &descriptor_write);

	return out_handle;
}

void bindless_resource_manager_unregister_texture(GpuContext* in_gpu_context, BindlessResourceManager* in_manager, TextureHandle in_handle)
{
	assert(in_handle.idx < sb_count(in_manager->storage_buffers));
	assert(in_manager->textures[in_handle.idx].is_valid);

	// Destroy Image, ImageView and Sampler
	gpu_destroy_sampler(in_gpu_context, &in_manager->textures[in_handle.idx].sampler);
	gpu_destroy_image_view(in_gpu_context, &in_manager->textures[in_handle.idx].image_view);
	gpu_destroy_image(in_gpu_context, &in_manager->textures[in_handle.idx].image);

	in_manager->textures[in_handle.idx] = (BindlessTexture) { 
		.is_valid = false,
		.image = (GpuImage) {},
		.image_view = (GpuImageView) {},
		.sampler = (GpuSampler) {},
	};
	
	//FCS TODO: Write "invalid" resource into descriptor set here...

	// Add Index to free list
	sb_push(in_manager->textures_free_list, in_handle);
}

void bindless_resource_manager_destroy(GpuContext* in_gpu_context, BindlessResourceManager* in_manager)
{
	// Clean up any registered resources
	for (i32 idx = 0; idx < sb_count(in_manager->uniform_buffers); ++idx)
	{
		BindlessBuffer* buffer = &in_manager->uniform_buffers[idx];
		if (buffer->is_valid)
		{
			UniformBufferHandle handle = {.idx = idx, };
			bindless_resource_manager_unregister_uniform_buffer(in_gpu_context, in_manager, handle);
		}
	}

	for (i32 idx = 0; idx < sb_count(in_manager->storage_buffers); ++idx)
	{
		BindlessBuffer* buffer = &in_manager->storage_buffers[idx];
		if (buffer->is_valid)
		{
			StorageBufferHandle handle = {.idx = idx, };
			bindless_resource_manager_unregister_storage_buffer(in_gpu_context, in_manager, handle);
		}
	}

	for (i32 idx = 0; idx < sb_count(in_manager->textures); ++idx)
	{
		BindlessTexture* texture = &in_manager->textures[idx];
		if (texture->is_valid)
		{
			TextureHandle handle = {.idx = idx, };
			bindless_resource_manager_unregister_texture(in_gpu_context, in_manager, handle);
		}
	}

	gpu_destroy_descriptor_set(in_gpu_context, &in_manager->descriptor_set);
	gpu_destroy_descriptor_layout(in_gpu_context, &in_manager->descriptor_layout);
}
