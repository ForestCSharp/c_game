
#import <Metal/Metal.h>
#import <QuartzCore/CoreAnimation.h>

#define _OBJC_RELEASE(obj) { [obj release]; obj = nil; }

struct Gpu2Device
{
	id<MTLDevice> metal_device;
	CAMetalLayer* metal_layer;
	id<MTLCommandQueue> metal_queue;
};

struct Gpu2Shader
{
	id<MTLLibrary> metal_library;
	id<MTLFunction> metal_function;
};

struct Gpu2BindGroupLayout
{
	u32 index;
	u32 num_bindings;
	Gpu2ResourceBinding bindings[GPU2_BIND_GROUP_MAX_BINDINGS];
};

// Stores a reference to the last write at a given binding offset
//FCS TODO: Rename to denote it's metal-specific
typedef struct Gpu2BindGroupWriteReference
{
	bool is_valid;
	Gpu2BindingType type;
	id<MTLBuffer> metal_buffer;
	id<MTLTexture> metal_texture;
	id<MTLSamplerState> metal_sampler_state;
} Gpu2BindGroupWriteReference;

struct Gpu2BindGroup
{
	Gpu2BindGroupLayout layout;
	u64 binding_offsets[GPU2_BIND_GROUP_MAX_BINDINGS];
	Gpu2BindGroupWriteReference write_references[GPU2_BIND_GROUP_MAX_BINDINGS];

	id<MTLBuffer> metal_argument_buffer;
};

struct Gpu2RenderPipeline {
	id<MTLRenderPipelineState> metal_render_pipeline_state;	
	id<MTLDepthStencilState> metal_depth_stencil_state;
	MTLTriangleFillMode metal_triangle_fill_mode;
};

struct Gpu2RenderPass
{
	id<MTLRenderCommandEncoder> metal_render_command_encoder;
};

struct Gpu2Buffer {
	id<MTLBuffer> metal_buffer;
};

struct Gpu2Texture
{
	Gpu2TextureCreateInfo create_info;
	id<MTLTexture> metal_texture;
};

struct Gpu2Sampler
{
	id<MTLSamplerState> metal_sampler_state;
};

struct Gpu2CommandBuffer
{
	id<MTLCommandBuffer> metal_command_buffer;
};

struct Gpu2Drawable
{
	id<CAMetalDrawable> metal_drawable;
};

void gpu2_create_device(Window* in_window, Gpu2Device* out_device)
{
	*out_device = (Gpu2Device){};

	out_device->metal_device = MTLCreateSystemDefaultDevice();
	assert(out_device->metal_device);

	out_device->metal_layer = in_window->metal_layer;
	assert(out_device->metal_layer);
	[out_device->metal_layer setDevice:out_device->metal_device];

	out_device->metal_queue = [out_device->metal_device newCommandQueue];
	assert(out_device->metal_queue);
}

void gpu2_destroy_device(Gpu2Device* in_device)
{
	in_device->metal_device = nil;
	in_device->metal_layer = nil;
	in_device->metal_queue = nil;
}

u32 gpu2_get_swapchain_count(Gpu2Device* in_device)
{
	return in_device->metal_layer.maximumDrawableCount;
}

void gpu2_create_shader(Gpu2Device* in_device, Gpu2ShaderCreateInfo* in_create_info, Gpu2Shader* out_shader)
{
	String filename_string = string_new(in_create_info->filename);
	string_append(&filename_string, ".msl");
	
	size_t file_size;	
	char* shader_source;
	read_binary_file(&filename_string, &file_size, (void**)&shader_source);
	string_free(&filename_string);

	const char* entry_point = "main0";

	NSError* lib_error = nil;
	id<MTLLibrary> library = [in_device->metal_device newLibraryWithSource:[NSString stringWithUTF8String:shader_source] options:nil error:&lib_error];

	if (lib_error)
	{
		NSLog(@"newLibraryWithSource Error: %@", lib_error);
		assert(false);
	}

	assert(library);

	id<MTLFunction> function = [library newFunctionWithName:[NSString stringWithUTF8String:entry_point]];
	assert(function);

	*out_shader = (Gpu2Shader){
		.metal_library = library,
		.metal_function = function,
	};

	free(shader_source);
}

void gpu2_destroy_shader(Gpu2Device* in_device, Gpu2Shader* in_shader)
{
	in_shader->metal_library = nil;
	in_shader->metal_function = nil;
}

bool gpu2_create_bind_group_layout(Gpu2Device* in_device, const Gpu2BindGroupLayoutCreateInfo* in_create_info, Gpu2BindGroupLayout* out_bind_group_layout)
{
	out_bind_group_layout->index = in_create_info->index;
	out_bind_group_layout->num_bindings = in_create_info->num_bindings;
	memcpy(out_bind_group_layout->bindings, in_create_info->bindings, sizeof(Gpu2ResourceBinding) * in_create_info->num_bindings);	
	return true;
}

bool gpu2_create_bind_group(Gpu2Device* in_device, const Gpu2BindGroupCreateInfo* in_create_info, Gpu2BindGroup* out_bind_group)
{
	*out_bind_group = (Gpu2BindGroup) {};	

	Gpu2BindGroupLayout* layout = in_create_info->layout;

	out_bind_group->layout = *layout;

	// Determine Argument Buffer Length
	u64 argument_buffer_size = 0;
	for (i32 binding_index = 0; binding_index < layout->num_bindings; ++binding_index)
	{
		Gpu2ResourceBinding* resource_binding = &layout->bindings[binding_index];

		// Store offset to this binding
		out_bind_group->binding_offsets[binding_index] = argument_buffer_size;
		switch(resource_binding->type)
		{
			case GPU2_BINDING_TYPE_BUFFER:
				argument_buffer_size += sizeof(u64);
				break;
			case GPU2_BINDING_TYPE_TEXTURE:
				argument_buffer_size += sizeof(MTLResourceID);
				break;
			case GPU2_BINDING_TYPE_SAMPLER:
				argument_buffer_size += sizeof(MTLResourceID);
				break;
		}
	}

	// Create Argument Buffer
	MTLResourceOptions options = MTLResourceStorageModeShared | MTLResourceCPUCacheModeDefaultCache;
	out_bind_group->metal_argument_buffer = [in_device->metal_device newBufferWithLength:argument_buffer_size options:options];

	// Fill the entire buffer with zeroes
	memset(out_bind_group->metal_argument_buffer.contents, 0, argument_buffer_size);

	return true;
}

void gpu2_update_bind_group(Gpu2Device* in_device, const Gpu2BindGroupUpdateInfo* in_update_info)
{
	// Write Resources into Argument Buffer
	Gpu2BindGroup* bind_group = in_update_info->bind_group;
	Gpu2BindGroupLayout* layout = &bind_group->layout;
	for (i32 write_index = 0; write_index < in_update_info->num_writes; ++write_index)
	{
		Gpu2ResourceWrite* resource_write = &in_update_info->writes[write_index];	
		u32 resource_binding_index = resource_write->binding_index;

		assert(resource_binding_index < layout->num_bindings);
		Gpu2ResourceBinding* resource_binding = &layout->bindings[resource_binding_index];
		assert(resource_write->type == resource_binding->type);

		u32 argbuffer_offset = bind_group->binding_offsets[resource_binding_index];	

		switch (resource_write->type)
		{
			case GPU2_BINDING_TYPE_BUFFER:
			{
				Gpu2Buffer* buffer = resource_write->buffer_binding.buffer;
				assert(buffer && buffer->metal_buffer);

				// Get argbuffer contents and apply offset
				u8* argbuffer_contents = (u8*) bind_group->metal_argument_buffer.contents;		
				argbuffer_contents += argbuffer_offset;

				const u64 gpu_address = buffer->metal_buffer.gpuAddress;
				memcpy(argbuffer_contents, &gpu_address, sizeof(u64));

				bind_group->write_references[resource_binding_index] = (Gpu2BindGroupWriteReference){
					.is_valid = true,
					.type = GPU2_BINDING_TYPE_BUFFER,
					.metal_buffer = buffer->metal_buffer,
				};
				break;
			}
			case GPU2_BINDING_TYPE_TEXTURE:
			{
				Gpu2Texture* texture = resource_write->texture_binding.texture;
				assert(texture && texture->metal_texture);

				// Get argbuffer contents and apply offset
				u8* argbuffer_contents = (u8*) bind_group->metal_argument_buffer.contents;
				argbuffer_contents += argbuffer_offset;

				const MTLResourceID tex_id = texture->metal_texture.gpuResourceID;
				memcpy(argbuffer_contents, &tex_id, sizeof(MTLResourceID));

				bind_group->write_references[resource_binding_index] = (Gpu2BindGroupWriteReference){
					.is_valid = true,
					.type = GPU2_BINDING_TYPE_TEXTURE,
					.metal_texture = texture->metal_texture,
				};
				break;
			}
			case GPU2_BINDING_TYPE_SAMPLER:
			{
				Gpu2Sampler* sampler = resource_write->sampler_binding.sampler;
				assert(sampler && sampler->metal_sampler_state);

				// Get argbuffer contents and apply offset
				u8* argbuffer_contents = (u8*) bind_group->metal_argument_buffer.contents;
				argbuffer_contents += argbuffer_offset;

				MTLResourceID sampler_id = sampler->metal_sampler_state.gpuResourceID;
				memcpy(argbuffer_contents, &sampler_id, sizeof(MTLResourceID));

				bind_group->write_references[resource_binding_index] = (Gpu2BindGroupWriteReference){
					.is_valid = true,
					.type = GPU2_BINDING_TYPE_SAMPLER,
					.metal_sampler_state = sampler->metal_sampler_state,
				};
				break;
			}
		}
	}
}

void gpu2_destroy_bind_group(Gpu2Device* in_device, Gpu2BindGroup* in_bind_group)
{
	in_bind_group->metal_argument_buffer = nil;
	*in_bind_group = (Gpu2BindGroup){};
}

void gpu2_destroy_bind_group_layout(Gpu2Device* in_device, Gpu2BindGroupLayout* in_bind_group_layout)
{
	*in_bind_group_layout = (Gpu2BindGroupLayout){};
}

MTLTriangleFillMode gpu2_polygon_mode_to_mtl_triangle_fill_mode(Gpu2PolygonMode in_mode)
{
	switch (in_mode)
	{
		case GPU2_POLYGON_MODE_FILL: return MTLTriangleFillModeFill;
		case GPU2_POLYGON_MODE_LINE: return MTLTriangleFillModeLines;
		default: assert(false);
	}
}

//FCS TODO: currently just assumes a single MTLPixelFormatBGRA8Unorm color attachment
bool gpu2_create_render_pipeline(Gpu2Device* in_device, Gpu2RenderPipelineCreateInfo* in_create_info, Gpu2RenderPipeline* out_render_pipeline)
{	
	MTLRenderPipelineDescriptor *metal_render_pipeline_descriptor = [[MTLRenderPipelineDescriptor alloc] init];
	assert(in_create_info->vertex_shader);
	metal_render_pipeline_descriptor.vertexFunction = in_create_info->vertex_shader->metal_function;
	assert(in_create_info->fragment_shader);
	metal_render_pipeline_descriptor.fragmentFunction = in_create_info->fragment_shader->metal_function;

	MTLRenderPipelineColorAttachmentDescriptor* color_attachment = metal_render_pipeline_descriptor.colorAttachments[0];
	color_attachment.pixelFormat = MTLPixelFormatBGRA8Unorm;
	color_attachment.blendingEnabled = YES;
	// Color blending
	color_attachment.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
	color_attachment.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
	color_attachment.rgbBlendOperation = MTLBlendOperationAdd;
	// Alpha blending 
	color_attachment.sourceAlphaBlendFactor = MTLBlendFactorOne;
	color_attachment.destinationAlphaBlendFactor = MTLBlendFactorZero;
	color_attachment.alphaBlendOperation = MTLBlendOperationAdd;

	metal_render_pipeline_descriptor.depthAttachmentPixelFormat = in_create_info->depth_test_enabled ? MTLPixelFormatDepth32Float : MTLPixelFormatInvalid;


    MTLDepthStencilDescriptor* metal_depth_stencil_desc = [[MTLDepthStencilDescriptor alloc] init];
	metal_depth_stencil_desc.depthCompareFunction = in_create_info->depth_test_enabled ? MTLCompareFunctionLess : MTLCompareFunctionAlways;
	metal_depth_stencil_desc.depthWriteEnabled = in_create_info->depth_test_enabled;
	id<MTLDepthStencilState> metal_depth_stencil_state = [in_device->metal_device newDepthStencilStateWithDescriptor:metal_depth_stencil_desc];

	*out_render_pipeline = (Gpu2RenderPipeline){
		.metal_render_pipeline_state = [in_device->metal_device newRenderPipelineStateWithDescriptor:metal_render_pipeline_descriptor error:nil],
		.metal_depth_stencil_state = metal_depth_stencil_state,
		.metal_triangle_fill_mode = gpu2_polygon_mode_to_mtl_triangle_fill_mode(in_create_info->polygon_mode),
	};

	return true;
}

//TODO: is_cpu_visible arg support
void gpu2_create_buffer(Gpu2Device* in_device, const Gpu2BufferCreateInfo* in_create_info, Gpu2Buffer* out_buffer)
{
	*out_buffer = (Gpu2Buffer){};

	MTLResourceOptions options = MTLResourceCPUCacheModeDefaultCache;
	if (in_create_info->is_cpu_visible)
	{
		options |= MTLStorageModeShared;
	}

	if (in_create_info->data)
	{
		out_buffer->metal_buffer = [in_device->metal_device newBufferWithBytes:in_create_info->data length:in_create_info->size options:options];
	}
	else
	{
		out_buffer->metal_buffer = [in_device->metal_device newBufferWithLength:in_create_info->size options:options];
	}
}

void gpu2_write_buffer(Gpu2Device* in_device, const Gpu2BufferWriteInfo* in_write_info)
{
	assert(in_write_info->buffer->metal_buffer.contents != NULL);
	memcpy(in_write_info->buffer->metal_buffer.contents, in_write_info->data, in_write_info->size);
}

void* gpu2_map_buffer(Gpu2Device* in_device, Gpu2Buffer* in_buffer)
{
	return in_buffer->metal_buffer.contents;
}

void gpu_unmap_buffer(Gpu2Device* in_device, Gpu2Buffer* in_buffer)
{
	// Nothing to do
}

void gpu2_destroy_buffer(Gpu2Device* in_device, Gpu2Buffer* in_buffer)
{
	_OBJC_RELEASE(in_buffer->metal_buffer);
}

MTLPixelFormat gpu2_format_to_mtl_format(Gpu2Format in_format)
{
	switch(in_format)
	{
		case GPU2_FORMAT_RGBA8_UNORM: return MTLPixelFormatRGBA8Unorm;
		case GPU2_FORMAT_D32_SFLOAT: return MTLPixelFormatDepth32Float;
		default: 
			printf("gpu2_format_to_vk_format: Unimplemented Format\n");
			exit(0);
			return 0;
	}
}


MTLTextureUsage gpu2_texture_usage_flags_to_mtl_texture_usage(Gpu2TextureUsageFlags in_flags)
{
	MTLTextureUsage out_flags = 0;
	
	if (BIT_COMPARE(in_flags, GPU2_TEXTURE_USAGE_TRANSFER_SRC))
	{
		//
	}

	if (BIT_COMPARE(in_flags, GPU2_TEXTURE_USAGE_TRANSFER_DST))
	{
		//
	}

	if (BIT_COMPARE(in_flags, GPU2_TEXTURE_USAGE_SAMPLED))
	{
		out_flags |= MTLTextureUsageShaderRead;
	}

	if (BIT_COMPARE(in_flags, GPU2_TEXTURE_USAGE_STORAGE))
	{
		out_flags |= MTLTextureUsageShaderRead;
		out_flags |= MTLTextureUsageShaderWrite;
	}

	if (BIT_COMPARE(in_flags, GPU2_TEXTURE_USAGE_COLOR_ATTACHMENT))
	{
		out_flags |= MTLTextureUsageRenderTarget;
	}

	if (BIT_COMPARE(in_flags, GPU2_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT))
	{
		//
	}

	return out_flags;
}

void gpu2_create_texture(Gpu2Device* in_device, const Gpu2TextureCreateInfo* in_create_info, Gpu2Texture* out_texture)
{
	const Gpu2TextureExtent extent	= in_create_info->extent;
    MTLTextureType mtl_texture_type = extent.depth > 1 	?	MTLTextureType3D	
									: extent.height > 1	? 	MTLTextureType2D
									: 						MTLTextureType1D;

	MTLTextureDescriptor *texture_descriptor = [[MTLTextureDescriptor alloc] init];
	texture_descriptor.textureType = mtl_texture_type;
	texture_descriptor.pixelFormat = gpu2_format_to_mtl_format(in_create_info->format);
	texture_descriptor.width = extent.width;
	texture_descriptor.height = extent.height;
	texture_descriptor.depth = extent.depth;
	texture_descriptor.mipmapLevelCount = 1;
	texture_descriptor.sampleCount = 1;
	texture_descriptor.arrayLength = 1;
	texture_descriptor.storageMode = in_create_info->is_cpu_visible ? MTLStorageModeShared : MTLStorageModeManaged;
	texture_descriptor.usage = gpu2_texture_usage_flags_to_mtl_texture_usage(in_create_info->usage);
	
	out_texture->create_info = *in_create_info;
	out_texture->metal_texture = [in_device->metal_device newTextureWithDescriptor:texture_descriptor];
}

void gpu2_write_texture(Gpu2Device* in_device, const Gpu2TextureWriteInfo* in_upload_info)
{
	Gpu2Texture* texture = in_upload_info->texture;
    id<MTLTexture> metal_texture = texture->metal_texture;
    MTLRegion region = MTLRegionMake2D(0, 0, in_upload_info->width, in_upload_info->height);

	const u32 format_stride = gpu2_format_stride(texture->create_info.format);
	const u32 bytes_per_row = in_upload_info->width * format_stride;
    
    [metal_texture replaceRegion:region
    	mipmapLevel:0
    	withBytes:in_upload_info->data
    	bytesPerRow:bytes_per_row
	];

	if (!texture->create_info.is_cpu_visible)
	{
		//TODO: Need to sync for non-cpu-visible textures
		// after replaceRegion: (when using Managed storage on macOS)
		//id<MTLCommandBuffer> cb = [in_device->metal_queue commandBuffer];
		//id<MTLBlitCommandEncoder> bl = [cb blitCommandEncoder];
		//[bl synchronizeResource: texture];     // ensure GPU sees CPU changes
		//[bl endEncoding];
		//[cb commit];
		//[cb waitUntilCompleted]; // or do async
	}
}

void gpu2_destroy_texture(Gpu2Device* in_device, Gpu2Texture* in_texture)
{
	_OBJC_RELEASE(in_texture->metal_texture);
}

MTLSamplerMinMagFilter gpu2_filter_to_mtl_filter(Gpu2Filter in_filter)
{
	switch (in_filter)
	{
		case GPU2_FILTER_NEAREST: return MTLSamplerMinMagFilterNearest;
		case GPU2_FILTER_LINEAR:  return MTLSamplerMinMagFilterLinear;
	}
	assert(false);
	return MTLSamplerMinMagFilterNearest; // fallback to silence warnings
}

MTLSamplerAddressMode gpu2_sampler_address_mode_to_mtl_sampler_address_mode(Gpu2SamplerAddressMode in_mode)
{
	switch (in_mode)
	{
		case GPU2_SAMPLER_ADDRESS_MODE_REPEAT:
			return MTLSamplerAddressModeRepeat;
		case GPU2_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
			return MTLSamplerAddressModeMirrorRepeat;
		case GPU2_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
			return MTLSamplerAddressModeClampToEdge;
		case GPU2_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
			// Metal calls this "clamp to border color"
			return MTLSamplerAddressModeClampToBorderColor;
	}
	assert(false);
	return MTLSamplerAddressModeClampToEdge;
}

void gpu2_create_sampler(Gpu2Device* in_device, const Gpu2SamplerCreateInfo* in_create_info, Gpu2Sampler* out_sampler)
{	
	MTLSamplerDescriptor *metal_sampler_desc = [[MTLSamplerDescriptor alloc] init];
	metal_sampler_desc.supportArgumentBuffers = YES;
	metal_sampler_desc.magFilter     = gpu2_filter_to_mtl_filter(in_create_info->filters.mag);
	metal_sampler_desc.minFilter     = gpu2_filter_to_mtl_filter(in_create_info->filters.min);
	metal_sampler_desc.mipFilter     = MTLSamplerMipFilterLinear;
	metal_sampler_desc.sAddressMode  = gpu2_sampler_address_mode_to_mtl_sampler_address_mode(in_create_info->address_modes.u);
	metal_sampler_desc.tAddressMode  = gpu2_sampler_address_mode_to_mtl_sampler_address_mode(in_create_info->address_modes.v);
	metal_sampler_desc.rAddressMode  = gpu2_sampler_address_mode_to_mtl_sampler_address_mode(in_create_info->address_modes.w);

	id<MTLSamplerState> metal_sampler_state = [in_device->metal_device newSamplerStateWithDescriptor:metal_sampler_desc];

	*out_sampler = (Gpu2Sampler) {
		.metal_sampler_state = metal_sampler_state,
	};
}

void gpu2_destroy_sampler(Gpu2Device* in_device, Gpu2Sampler* in_sampler)
{
	_OBJC_RELEASE(in_sampler->metal_sampler_state);
}

bool gpu2_create_command_buffer(Gpu2Device* in_device, Gpu2CommandBuffer* out_command_buffer)
{
	*out_command_buffer = (Gpu2CommandBuffer){};
	out_command_buffer->metal_command_buffer = [in_device->metal_queue commandBuffer];
	return true;
}

bool gpu2_get_next_drawable(Gpu2Device* in_device, Gpu2CommandBuffer* in_command_buffer, Gpu2Drawable* out_drawable)
{
	*out_drawable = (Gpu2Drawable) {
		.metal_drawable = [in_device->metal_layer nextDrawable],
	};
	return true; 
}

bool gpu2_drawable_get_texture(Gpu2Drawable* in_drawable, Gpu2Texture* out_texture)
{
	assert(in_drawable);
	assert(out_texture);

	*out_texture = (Gpu2Texture) {
		.metal_texture = in_drawable->metal_drawable.texture,
	};

	return true;
}

MTLLoadAction gpu2_load_action_to_metal_load_action(Gpu2LoadAction in_load_action)
{
	switch (in_load_action)
	{
		case GPU2_LOAD_ACTION_DONT_CARE:	return MTLLoadActionDontCare;
		case GPU2_LOAD_ACTION_LOAD: 		return MTLLoadActionLoad;
		case GPU2_LOAD_ACTION_CLEAR:		return MTLLoadActionClear;
	}
	assert(false);
}

MTLStoreAction gpu2_store_action_to_metal_store_action(Gpu2StoreAction in_store_action)
{
	switch (in_store_action)
	{
		case GPU2_STORE_ACTION_DONT_CARE: 	return MTLStoreActionDontCare;
		case GPU2_STORE_ACTION_STORE:		return MTLStoreActionStore;
	}
	assert(false);
}

void gpu2_begin_render_pass(Gpu2Device* in_device, Gpu2RenderPassCreateInfo* in_create_info, Gpu2RenderPass* out_render_pass)
{
	assert(in_device);
	assert(in_create_info);
	assert(out_render_pass);

	*out_render_pass = (Gpu2RenderPass){};
	MTLRenderPassDescriptor* metal_render_pass_descriptor = [MTLRenderPassDescriptor renderPassDescriptor];


	for (i32 i = 0; i < in_create_info->num_color_attachments; ++i)
	{
		Gpu2ColorAttachmentDescriptor* color_attachment = &in_create_info->color_attachments[i];
		if (color_attachment->texture)
		{
			metal_render_pass_descriptor.colorAttachments[i].texture = color_attachment->texture->metal_texture;
			metal_render_pass_descriptor.colorAttachments[i].loadAction = gpu2_load_action_to_metal_load_action(color_attachment->load_action);
			metal_render_pass_descriptor.colorAttachments[i].storeAction = gpu2_store_action_to_metal_store_action(color_attachment->store_action);
			metal_render_pass_descriptor.colorAttachments[i].clearColor = MTLClearColorMake(
				color_attachment->clear_color[0],
				color_attachment->clear_color[1],
				color_attachment->clear_color[2],
				color_attachment->clear_color[3]
			);
		}
	}

	Gpu2DepthAttachmentDescriptor* depth_attachment = in_create_info->depth_attachment;
	if (depth_attachment)
	{
		metal_render_pass_descriptor.depthAttachment.texture = depth_attachment->texture->metal_texture;
		metal_render_pass_descriptor.depthAttachment.loadAction = gpu2_load_action_to_metal_load_action(depth_attachment->load_action);
		metal_render_pass_descriptor.depthAttachment.storeAction = gpu2_store_action_to_metal_store_action(depth_attachment->store_action);
		metal_render_pass_descriptor.depthAttachment.clearDepth = depth_attachment->clear_depth;
	}

	out_render_pass->metal_render_command_encoder = [in_create_info->command_buffer->metal_command_buffer renderCommandEncoderWithDescriptor:metal_render_pass_descriptor];	
}

void gpu2_end_render_pass(Gpu2RenderPass* in_render_pass)
{
	[in_render_pass->metal_render_command_encoder endEncoding];
}

void gpu2_render_pass_set_render_pipeline(Gpu2RenderPass* in_render_pass, Gpu2RenderPipeline* in_render_pipeline)
{
	[in_render_pass->metal_render_command_encoder setRenderPipelineState:in_render_pipeline->metal_render_pipeline_state];
	[in_render_pass->metal_render_command_encoder setDepthStencilState:in_render_pipeline->metal_depth_stencil_state];
	[in_render_pass->metal_render_command_encoder setTriangleFillMode:in_render_pipeline->metal_triangle_fill_mode];
}

void gpu2_render_pass_set_bind_group(Gpu2RenderPass* in_render_pass, Gpu2RenderPipeline* in_render_pipeline, Gpu2BindGroup* in_bind_group)
{
	assert(in_render_pass);
	assert(in_bind_group);
	
	Gpu2BindGroupLayout* layout = &in_bind_group->layout;
	for (i32 binding_index = 0; binding_index < layout->num_bindings; ++binding_index)
	{
		Gpu2BindGroupWriteReference* write_reference = &in_bind_group->write_references[binding_index];
		if (!write_reference->is_valid)
		{
			continue;
		}

		Gpu2ResourceBinding* resource_binding = &layout->bindings[binding_index];
		MTLRenderStages metal_render_stages = 0;	
		if (BIT_COMPARE(resource_binding->shader_stages, GPU2_SHADER_STAGE_VERTEX))
		{
			metal_render_stages |= MTLRenderStageVertex;
		}
		if (BIT_COMPARE(resource_binding->shader_stages, GPU2_SHADER_STAGE_FRAGMENT))
		{
			metal_render_stages |= MTLRenderStageFragment;
	  	}
	  		
	  	switch(write_reference->type)
		{
			case GPU2_BINDING_TYPE_BUFFER:
				[in_render_pass->metal_render_command_encoder 
					useResource:write_reference->metal_buffer 
					usage:MTLResourceUsageRead
	  				stages:metal_render_stages
				];
	  			break;
			case GPU2_BINDING_TYPE_TEXTURE:
				[in_render_pass->metal_render_command_encoder
					useResource:write_reference->metal_texture
					usage:MTLResourceUsageRead
					stages:metal_render_stages];
				break;

			case GPU2_BINDING_TYPE_SAMPLER:
				// Nothing to do for Samplers
				break;
		}
	}

	[in_render_pass->metal_render_command_encoder
		setVertexBuffer:in_bind_group->metal_argument_buffer
		offset:0 
		atIndex:layout->index
	];

	[in_render_pass->metal_render_command_encoder
		setFragmentBuffer:in_bind_group->metal_argument_buffer
		offset:0
		atIndex:layout->index
	];	
}

void gpu2_render_pass_draw(Gpu2RenderPass* in_render_pass, u32 vertex_start, u32 vertex_count)
{
	[in_render_pass->metal_render_command_encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:vertex_start vertexCount:vertex_count];
}

void gpu2_present_drawable(Gpu2Device* in_device, Gpu2CommandBuffer* in_command_buffer, Gpu2Drawable* in_drawable)
{
	[in_command_buffer->metal_command_buffer presentDrawable:in_drawable->metal_drawable];
}

bool gpu2_commit_command_buffer(Gpu2Device* in_device, Gpu2CommandBuffer* in_command_buffer)
{
	[in_command_buffer->metal_command_buffer commit];

	//FCS TODO: remove this, we shouldn't wait
	[in_command_buffer->metal_command_buffer waitUntilCompleted];
	return true;
}

const char* gpu2_get_api_name()
{
	return "Metal";
}
