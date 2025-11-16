
#import <Metal/Metal.h>
#import <QuartzCore/CoreAnimation.h>

#define _OBJC_RELEASE(obj) { [obj release]; obj = nil; }

struct GpuDevice
{
	id<MTLDevice> metal_device;
	CAMetalLayer* metal_layer;
	id<MTLCommandQueue> metal_queue;
};

struct GpuShader
{
	id<MTLLibrary> metal_library;
	id<MTLFunction> metal_function;
};

struct GpuBindGroupLayout
{
	u32 index;
	u32 num_bindings;
	GpuResourceBinding bindings[GPU_BIND_GROUP_MAX_BINDINGS];
};

// Stores a reference to the last write at a given binding offset
//FCS TODO: Rename to denote it's metal-specific
typedef struct GpuBindGroupWriteReference
{
	bool is_valid;
	GpuBindingType type;
	id<MTLBuffer> metal_buffer;
	id<MTLTexture> metal_texture;
	id<MTLSamplerState> metal_sampler_state;
} GpuBindGroupWriteReference;

struct GpuBindGroup
{
	GpuBindGroupLayout layout;
	u64 binding_offsets[GPU_BIND_GROUP_MAX_BINDINGS];
	GpuBindGroupWriteReference write_references[GPU_BIND_GROUP_MAX_BINDINGS];

	id<MTLBuffer> metal_argument_buffer;
};

struct GpuRenderPipeline {
	id<MTLRenderPipelineState> metal_render_pipeline_state;	
	id<MTLDepthStencilState> metal_depth_stencil_state;
	MTLTriangleFillMode metal_triangle_fill_mode;
};

struct GpuRenderPass
{
	id<MTLRenderCommandEncoder> metal_render_command_encoder;
};

struct GpuBuffer {
	id<MTLBuffer> metal_buffer;
};

struct GpuTexture
{
	GpuTextureCreateInfo create_info;
	id<MTLTexture> metal_texture;
};

struct GpuSampler
{
	id<MTLSamplerState> metal_sampler_state;
};

struct GpuCommandBuffer
{
	id<MTLCommandBuffer> metal_command_buffer;
};

struct GpuDrawable
{
	id<CAMetalDrawable> metal_drawable;
};

void gpu_create_device(Window* in_window, GpuDevice* out_device)
{
	*out_device = (GpuDevice){};

	out_device->metal_device = MTLCreateSystemDefaultDevice();
	assert(out_device->metal_device);

	out_device->metal_layer = in_window->metal_layer;
	assert(out_device->metal_layer);
	[out_device->metal_layer setDevice:out_device->metal_device];

	out_device->metal_queue = [out_device->metal_device newCommandQueue];
	assert(out_device->metal_queue);
}

void gpu_destroy_device(GpuDevice* in_device)
{
	in_device->metal_device = nil;
	in_device->metal_layer = nil;
	in_device->metal_queue = nil;
}

u32 gpu_get_swapchain_count(GpuDevice* in_device)
{
	return in_device->metal_layer.maximumDrawableCount;
}

void gpu_create_shader(GpuDevice* in_device, GpuShaderCreateInfo* in_create_info, GpuShader* out_shader)
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

	*out_shader = (GpuShader){
		.metal_library = library,
		.metal_function = function,
	};

	free(shader_source);
}

void gpu_destroy_shader(GpuDevice* in_device, GpuShader* in_shader)
{
	in_shader->metal_library = nil;
	in_shader->metal_function = nil;
}

bool gpu_create_bind_group_layout(GpuDevice* in_device, const GpuBindGroupLayoutCreateInfo* in_create_info, GpuBindGroupLayout* out_bind_group_layout)
{
	out_bind_group_layout->index = in_create_info->index;
	out_bind_group_layout->num_bindings = in_create_info->num_bindings;
	memcpy(out_bind_group_layout->bindings, in_create_info->bindings, sizeof(GpuResourceBinding) * in_create_info->num_bindings);	
	return true;
}

bool gpu_create_bind_group(GpuDevice* in_device, const GpuBindGroupCreateInfo* in_create_info, GpuBindGroup* out_bind_group)
{
	*out_bind_group = (GpuBindGroup) {};	

	GpuBindGroupLayout* layout = in_create_info->layout;

	out_bind_group->layout = *layout;

	// Determine Argument Buffer Length
	u64 argument_buffer_size = 0;
	for (i32 binding_index = 0; binding_index < layout->num_bindings; ++binding_index)
	{
		GpuResourceBinding* resource_binding = &layout->bindings[binding_index];

		// Store offset to this binding
		out_bind_group->binding_offsets[binding_index] = argument_buffer_size;
		switch(resource_binding->type)
		{
			case GPU_BINDING_TYPE_BUFFER:
				argument_buffer_size += sizeof(u64);
				break;
			case GPU_BINDING_TYPE_TEXTURE:
				argument_buffer_size += sizeof(MTLResourceID);
				break;
			case GPU_BINDING_TYPE_SAMPLER:
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

void gpu_update_bind_group(GpuDevice* in_device, const GpuBindGroupUpdateInfo* in_update_info)
{
	// Write Resources into Argument Buffer
	GpuBindGroup* bind_group = in_update_info->bind_group;
	GpuBindGroupLayout* layout = &bind_group->layout;
	for (i32 write_index = 0; write_index < in_update_info->num_writes; ++write_index)
	{
		GpuResourceWrite* resource_write = &in_update_info->writes[write_index];	
		u32 resource_binding_index = resource_write->binding_index;

		assert(resource_binding_index < layout->num_bindings);
		GpuResourceBinding* resource_binding = &layout->bindings[resource_binding_index];
		assert(resource_write->type == resource_binding->type);

		u32 argbuffer_offset = bind_group->binding_offsets[resource_binding_index];	

		switch (resource_write->type)
		{
			case GPU_BINDING_TYPE_BUFFER:
			{
				GpuBuffer* buffer = resource_write->buffer_binding.buffer;
				assert(buffer && buffer->metal_buffer);

				// Get argbuffer contents and apply offset
				u8* argbuffer_contents = (u8*) bind_group->metal_argument_buffer.contents;		
				argbuffer_contents += argbuffer_offset;

				const u64 gpu_address = buffer->metal_buffer.gpuAddress;
				memcpy(argbuffer_contents, &gpu_address, sizeof(u64));

				bind_group->write_references[resource_binding_index] = (GpuBindGroupWriteReference){
					.is_valid = true,
					.type = GPU_BINDING_TYPE_BUFFER,
					.metal_buffer = buffer->metal_buffer,
				};
				break;
			}
			case GPU_BINDING_TYPE_TEXTURE:
			{
				GpuTexture* texture = resource_write->texture_binding.texture;
				assert(texture && texture->metal_texture);

				// Get argbuffer contents and apply offset
				u8* argbuffer_contents = (u8*) bind_group->metal_argument_buffer.contents;
				argbuffer_contents += argbuffer_offset;

				const MTLResourceID tex_id = texture->metal_texture.gpuResourceID;
				memcpy(argbuffer_contents, &tex_id, sizeof(MTLResourceID));

				bind_group->write_references[resource_binding_index] = (GpuBindGroupWriteReference){
					.is_valid = true,
					.type = GPU_BINDING_TYPE_TEXTURE,
					.metal_texture = texture->metal_texture,
				};
				break;
			}
			case GPU_BINDING_TYPE_SAMPLER:
			{
				GpuSampler* sampler = resource_write->sampler_binding.sampler;
				assert(sampler && sampler->metal_sampler_state);

				// Get argbuffer contents and apply offset
				u8* argbuffer_contents = (u8*) bind_group->metal_argument_buffer.contents;
				argbuffer_contents += argbuffer_offset;

				MTLResourceID sampler_id = sampler->metal_sampler_state.gpuResourceID;
				memcpy(argbuffer_contents, &sampler_id, sizeof(MTLResourceID));

				bind_group->write_references[resource_binding_index] = (GpuBindGroupWriteReference){
					.is_valid = true,
					.type = GPU_BINDING_TYPE_SAMPLER,
					.metal_sampler_state = sampler->metal_sampler_state,
				};
				break;
			}
		}
	}
}

void gpu_destroy_bind_group(GpuDevice* in_device, GpuBindGroup* in_bind_group)
{
	in_bind_group->metal_argument_buffer = nil;
	*in_bind_group = (GpuBindGroup){};
}

void gpu_destroy_bind_group_layout(GpuDevice* in_device, GpuBindGroupLayout* in_bind_group_layout)
{
	*in_bind_group_layout = (GpuBindGroupLayout){};
}

MTLTriangleFillMode gpu_polygon_mode_to_mtl_triangle_fill_mode(GpuPolygonMode in_mode)
{
	switch (in_mode)
	{
		case GPU_POLYGON_MODE_FILL: return MTLTriangleFillModeFill;
		case GPU_POLYGON_MODE_LINE: return MTLTriangleFillModeLines;
		default: assert(false);
	}
}

//FCS TODO: currently just assumes a single MTLPixelFormatBGRA8Unorm color attachment
bool gpu_create_render_pipeline(GpuDevice* in_device, GpuRenderPipelineCreateInfo* in_create_info, GpuRenderPipeline* out_render_pipeline)
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

	*out_render_pipeline = (GpuRenderPipeline){
		.metal_render_pipeline_state = [in_device->metal_device newRenderPipelineStateWithDescriptor:metal_render_pipeline_descriptor error:nil],
		.metal_depth_stencil_state = metal_depth_stencil_state,
		.metal_triangle_fill_mode = gpu_polygon_mode_to_mtl_triangle_fill_mode(in_create_info->polygon_mode),
	};

	return true;
}

//TODO: is_cpu_visible arg support
void gpu_create_buffer(GpuDevice* in_device, const GpuBufferCreateInfo* in_create_info, GpuBuffer* out_buffer)
{
	*out_buffer = (GpuBuffer){};

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

void gpu_write_buffer(GpuDevice* in_device, const GpuBufferWriteInfo* in_write_info)
{
	assert(in_write_info->buffer->metal_buffer.contents != NULL);
	memcpy(in_write_info->buffer->metal_buffer.contents, in_write_info->data, in_write_info->size);
}

void* gpu_map_buffer(GpuDevice* in_device, GpuBuffer* in_buffer)
{
	return in_buffer->metal_buffer.contents;
}

void gpu_unmap_buffer(GpuDevice* in_device, GpuBuffer* in_buffer)
{
	// Nothing to do
}

void gpu_destroy_buffer(GpuDevice* in_device, GpuBuffer* in_buffer)
{
	_OBJC_RELEASE(in_buffer->metal_buffer);
}

MTLPixelFormat gpu_format_to_mtl_format(GpuFormat in_format)
{
	switch(in_format)
	{
		case GPU_FORMAT_RGBA8_UNORM: return MTLPixelFormatRGBA8Unorm;
		case GPU_FORMAT_D32_SFLOAT: return MTLPixelFormatDepth32Float;
		default: 
			printf("gpu_format_to_vk_format: Unimplemented Format\n");
			exit(0);
			return 0;
	}
}


MTLTextureUsage gpu_texture_usage_flags_to_mtl_texture_usage(GpuTextureUsageFlags in_flags)
{
	MTLTextureUsage out_flags = 0;
	
	if (BIT_COMPARE(in_flags, GPU_TEXTURE_USAGE_TRANSFER_SRC))
	{
		//
	}

	if (BIT_COMPARE(in_flags, GPU_TEXTURE_USAGE_TRANSFER_DST))
	{
		//
	}

	if (BIT_COMPARE(in_flags, GPU_TEXTURE_USAGE_SAMPLED))
	{
		out_flags |= MTLTextureUsageShaderRead;
	}

	if (BIT_COMPARE(in_flags, GPU_TEXTURE_USAGE_STORAGE))
	{
		out_flags |= MTLTextureUsageShaderRead;
		out_flags |= MTLTextureUsageShaderWrite;
	}

	if (BIT_COMPARE(in_flags, GPU_TEXTURE_USAGE_COLOR_ATTACHMENT))
	{
		out_flags |= MTLTextureUsageRenderTarget;
	}

	if (BIT_COMPARE(in_flags, GPU_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT))
	{
		//
	}

	return out_flags;
}

void gpu_create_texture(GpuDevice* in_device, const GpuTextureCreateInfo* in_create_info, GpuTexture* out_texture)
{
	const GpuTextureExtent extent	= in_create_info->extent;
    MTLTextureType mtl_texture_type = extent.depth > 1 	?	MTLTextureType3D	
									: extent.height > 1	? 	MTLTextureType2D
									: 						MTLTextureType1D;

	MTLTextureDescriptor *texture_descriptor = [[MTLTextureDescriptor alloc] init];
	texture_descriptor.textureType = mtl_texture_type;
	texture_descriptor.pixelFormat = gpu_format_to_mtl_format(in_create_info->format);
	texture_descriptor.width = extent.width;
	texture_descriptor.height = extent.height;
	texture_descriptor.depth = extent.depth;
	texture_descriptor.mipmapLevelCount = 1;
	texture_descriptor.sampleCount = 1;
	texture_descriptor.arrayLength = 1;
	texture_descriptor.storageMode = in_create_info->is_cpu_visible ? MTLStorageModeShared : MTLStorageModeManaged;
	texture_descriptor.usage = gpu_texture_usage_flags_to_mtl_texture_usage(in_create_info->usage);
	
	out_texture->create_info = *in_create_info;
	out_texture->metal_texture = [in_device->metal_device newTextureWithDescriptor:texture_descriptor];
}

void gpu_write_texture(GpuDevice* in_device, const GpuTextureWriteInfo* in_upload_info)
{
	GpuTexture* texture = in_upload_info->texture;
    id<MTLTexture> metal_texture = texture->metal_texture;
    MTLRegion region = MTLRegionMake2D(0, 0, in_upload_info->width, in_upload_info->height);

	const u32 format_stride = gpu_format_stride(texture->create_info.format);
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

void gpu_destroy_texture(GpuDevice* in_device, GpuTexture* in_texture)
{
	_OBJC_RELEASE(in_texture->metal_texture);
}

MTLSamplerMinMagFilter gpu_filter_to_mtl_filter(GpuFilter in_filter)
{
	switch (in_filter)
	{
		case GPU_FILTER_NEAREST: return MTLSamplerMinMagFilterNearest;
		case GPU_FILTER_LINEAR:  return MTLSamplerMinMagFilterLinear;
	}
	assert(false);
	return MTLSamplerMinMagFilterNearest; // fallback to silence warnings
}

MTLSamplerAddressMode gpu_sampler_address_mode_to_mtl_sampler_address_mode(GpuSamplerAddressMode in_mode)
{
	switch (in_mode)
	{
		case GPU_SAMPLER_ADDRESS_MODE_REPEAT:
			return MTLSamplerAddressModeRepeat;
		case GPU_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
			return MTLSamplerAddressModeMirrorRepeat;
		case GPU_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
			return MTLSamplerAddressModeClampToEdge;
		case GPU_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
			// Metal calls this "clamp to border color"
			return MTLSamplerAddressModeClampToBorderColor;
	}
	assert(false);
	return MTLSamplerAddressModeClampToEdge;
}

void gpu_create_sampler(GpuDevice* in_device, const GpuSamplerCreateInfo* in_create_info, GpuSampler* out_sampler)
{	
	MTLSamplerDescriptor *metal_sampler_desc = [[MTLSamplerDescriptor alloc] init];
	metal_sampler_desc.supportArgumentBuffers = YES;
	metal_sampler_desc.magFilter     = gpu_filter_to_mtl_filter(in_create_info->filters.mag);
	metal_sampler_desc.minFilter     = gpu_filter_to_mtl_filter(in_create_info->filters.min);
	metal_sampler_desc.mipFilter     = MTLSamplerMipFilterLinear;
	metal_sampler_desc.sAddressMode  = gpu_sampler_address_mode_to_mtl_sampler_address_mode(in_create_info->address_modes.u);
	metal_sampler_desc.tAddressMode  = gpu_sampler_address_mode_to_mtl_sampler_address_mode(in_create_info->address_modes.v);
	metal_sampler_desc.rAddressMode  = gpu_sampler_address_mode_to_mtl_sampler_address_mode(in_create_info->address_modes.w);

	id<MTLSamplerState> metal_sampler_state = [in_device->metal_device newSamplerStateWithDescriptor:metal_sampler_desc];

	*out_sampler = (GpuSampler) {
		.metal_sampler_state = metal_sampler_state,
	};
}

void gpu_destroy_sampler(GpuDevice* in_device, GpuSampler* in_sampler)
{
	_OBJC_RELEASE(in_sampler->metal_sampler_state);
}

bool gpu_create_command_buffer(GpuDevice* in_device, GpuCommandBuffer* out_command_buffer)
{
	*out_command_buffer = (GpuCommandBuffer){};
	out_command_buffer->metal_command_buffer = [in_device->metal_queue commandBuffer];
	return true;
}

bool gpu_get_next_drawable(GpuDevice* in_device, GpuCommandBuffer* in_command_buffer, GpuDrawable* out_drawable)
{
	*out_drawable = (GpuDrawable) {
		.metal_drawable = [in_device->metal_layer nextDrawable],
	};
	return true; 
}

bool gpu_drawable_get_texture(GpuDrawable* in_drawable, GpuTexture* out_texture)
{
	assert(in_drawable);
	assert(out_texture);

	*out_texture = (GpuTexture) {
		.metal_texture = in_drawable->metal_drawable.texture,
	};

	return true;
}

MTLLoadAction gpu_load_action_to_metal_load_action(GpuLoadAction in_load_action)
{
	switch (in_load_action)
	{
		case GPU_LOAD_ACTION_DONT_CARE:	return MTLLoadActionDontCare;
		case GPU_LOAD_ACTION_LOAD: 		return MTLLoadActionLoad;
		case GPU_LOAD_ACTION_CLEAR:		return MTLLoadActionClear;
	}
	assert(false);
}

MTLStoreAction gpu_store_action_to_metal_store_action(GpuStoreAction in_store_action)
{
	switch (in_store_action)
	{
		case GPU_STORE_ACTION_DONT_CARE: 	return MTLStoreActionDontCare;
		case GPU_STORE_ACTION_STORE:		return MTLStoreActionStore;
	}
	assert(false);
}

void gpu_begin_render_pass(GpuDevice* in_device, GpuRenderPassCreateInfo* in_create_info, GpuRenderPass* out_render_pass)
{
	assert(in_device);
	assert(in_create_info);
	assert(out_render_pass);

	*out_render_pass = (GpuRenderPass){};
	MTLRenderPassDescriptor* metal_render_pass_descriptor = [MTLRenderPassDescriptor renderPassDescriptor];


	for (i32 i = 0; i < in_create_info->num_color_attachments; ++i)
	{
		GpuColorAttachmentDescriptor* color_attachment = &in_create_info->color_attachments[i];
		if (color_attachment->texture)
		{
			metal_render_pass_descriptor.colorAttachments[i].texture = color_attachment->texture->metal_texture;
			metal_render_pass_descriptor.colorAttachments[i].loadAction = gpu_load_action_to_metal_load_action(color_attachment->load_action);
			metal_render_pass_descriptor.colorAttachments[i].storeAction = gpu_store_action_to_metal_store_action(color_attachment->store_action);
			metal_render_pass_descriptor.colorAttachments[i].clearColor = MTLClearColorMake(
				color_attachment->clear_color[0],
				color_attachment->clear_color[1],
				color_attachment->clear_color[2],
				color_attachment->clear_color[3]
			);
		}
	}

	GpuDepthAttachmentDescriptor* depth_attachment = in_create_info->depth_attachment;
	if (depth_attachment)
	{
		metal_render_pass_descriptor.depthAttachment.texture = depth_attachment->texture->metal_texture;
		metal_render_pass_descriptor.depthAttachment.loadAction = gpu_load_action_to_metal_load_action(depth_attachment->load_action);
		metal_render_pass_descriptor.depthAttachment.storeAction = gpu_store_action_to_metal_store_action(depth_attachment->store_action);
		metal_render_pass_descriptor.depthAttachment.clearDepth = depth_attachment->clear_depth;
	}

	out_render_pass->metal_render_command_encoder = [in_create_info->command_buffer->metal_command_buffer renderCommandEncoderWithDescriptor:metal_render_pass_descriptor];	
}

void gpu_end_render_pass(GpuRenderPass* in_render_pass)
{
	[in_render_pass->metal_render_command_encoder endEncoding];
}

void gpu_render_pass_set_render_pipeline(GpuRenderPass* in_render_pass, GpuRenderPipeline* in_render_pipeline)
{
	[in_render_pass->metal_render_command_encoder setRenderPipelineState:in_render_pipeline->metal_render_pipeline_state];
	[in_render_pass->metal_render_command_encoder setDepthStencilState:in_render_pipeline->metal_depth_stencil_state];
	[in_render_pass->metal_render_command_encoder setTriangleFillMode:in_render_pipeline->metal_triangle_fill_mode];
}

void gpu_render_pass_set_bind_group(GpuRenderPass* in_render_pass, GpuRenderPipeline* in_render_pipeline, GpuBindGroup* in_bind_group)
{
	assert(in_render_pass);
	assert(in_bind_group);
	
	GpuBindGroupLayout* layout = &in_bind_group->layout;
	for (i32 binding_index = 0; binding_index < layout->num_bindings; ++binding_index)
	{
		GpuBindGroupWriteReference* write_reference = &in_bind_group->write_references[binding_index];
		if (!write_reference->is_valid)
		{
			continue;
		}

		GpuResourceBinding* resource_binding = &layout->bindings[binding_index];
		MTLRenderStages metal_render_stages = 0;	
		if (BIT_COMPARE(resource_binding->shader_stages, GPU_SHADER_STAGE_VERTEX))
		{
			metal_render_stages |= MTLRenderStageVertex;
		}
		if (BIT_COMPARE(resource_binding->shader_stages, GPU_SHADER_STAGE_FRAGMENT))
		{
			metal_render_stages |= MTLRenderStageFragment;
	  	}
	  		
	  	switch(write_reference->type)
		{
			case GPU_BINDING_TYPE_BUFFER:
				[in_render_pass->metal_render_command_encoder 
					useResource:write_reference->metal_buffer 
					usage:MTLResourceUsageRead
	  				stages:metal_render_stages
				];
	  			break;
			case GPU_BINDING_TYPE_TEXTURE:
				[in_render_pass->metal_render_command_encoder
					useResource:write_reference->metal_texture
					usage:MTLResourceUsageRead
					stages:metal_render_stages];
				break;

			case GPU_BINDING_TYPE_SAMPLER:
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

void gpu_render_pass_draw(GpuRenderPass* in_render_pass, u32 vertex_start, u32 vertex_count)
{
	[in_render_pass->metal_render_command_encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:vertex_start vertexCount:vertex_count];
}

void gpu_present_drawable(GpuDevice* in_device, GpuCommandBuffer* in_command_buffer, GpuDrawable* in_drawable)
{
	[in_command_buffer->metal_command_buffer presentDrawable:in_drawable->metal_drawable];
}

bool gpu_commit_command_buffer(GpuDevice* in_device, GpuCommandBuffer* in_command_buffer)
{
	[in_command_buffer->metal_command_buffer commit];

	//FCS TODO: remove this, we shouldn't wait
	//[in_command_buffer->metal_command_buffer waitUntilCompleted];
	return true;
}

const char* gpu_get_api_name()
{
	return "Metal";
}
