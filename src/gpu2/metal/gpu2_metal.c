
#import <Metal/Metal.h>
#import <QuartzCore/CoreAnimation.h>

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

struct Gpu2BindGroup
{
	Gpu2BindGroupCreateInfo create_info;
	id<MTLBuffer> metal_argument_buffer;
};

struct Gpu2RenderPipeline {
	id<MTLRenderPipelineState> metal_render_pipeline_state;	
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
	id<MTLTexture> metal_texture;
};

struct Gpu2CommandBuffer
{
	id<MTLCommandBuffer> metal_command_buffer;
};

struct Gpu2Drawable
{
	id<CAMetalDrawable> metal_drawable;
};

bool gpu2_create_device(Window* in_window, Gpu2Device* out_device)
{
	*out_device = (Gpu2Device){};

	out_device->metal_device = MTLCreateSystemDefaultDevice();
	assert(out_device->metal_device);

	out_device->metal_layer = in_window->metal_layer;
	assert(out_device->metal_layer);
	[out_device->metal_layer setDevice:out_device->metal_device];

	out_device->metal_queue = [out_device->metal_device newCommandQueue];
	assert(out_device->metal_queue);

	return true;
}

bool gpu2_create_shader(Gpu2Device* in_device, Gpu2ShaderCreateInfo* in_create_info, Gpu2Shader* out_shader)
{
	String filename_string = string_new(in_create_info->filename);
	string_append(&filename_string, ".msl");
	
	size_t file_size;	
	char* shader_source;
	read_binary_file(&filename_string, &file_size, (void**)&shader_source);
	string_free(&filename_string);

	const char* entry_point = "main0";

	NSError* error;
	id<MTLLibrary> library = [in_device->metal_device newLibraryWithSource:[NSString stringWithUTF8String:shader_source] options:nil error:&error];
	assert(!error);
	assert(library);

	id<MTLFunction> function = [library newFunctionWithName:[NSString stringWithUTF8String:entry_point]]; 
	assert(function);

	*out_shader = (Gpu2Shader){
		.metal_library = library,
		.metal_function = function,
	};

	free(shader_source);

	return true;
}

bool gpu2_create_bind_group_layout(Gpu2Device* in_device, Gpu2BindGroupLayoutCreateInfo* in_create_info, Gpu2BindGroupLayout* out_bind_group_layout)
{
	out_bind_group_layout->index = in_create_info->index;
	out_bind_group_layout->num_bindings = in_create_info->num_bindings;
	memcpy(out_bind_group_layout->bindings, in_create_info->bindings, sizeof(Gpu2ResourceBinding) * in_create_info->num_bindings);
	
	return true;
}

bool gpu2_create_bind_group(Gpu2Device* in_device, Gpu2BindGroupCreateInfo* in_create_info, Gpu2BindGroup* out_bind_group)
{
	*out_bind_group = (Gpu2BindGroup) {};	

	out_bind_group->create_info = *in_create_info;
	//FCS TODO: in_create_info bindings may not live long enough...

	// Determine Argument Buffer Length
	Gpu2BindGroupLayout* layout = in_create_info->layout;
	u64 argument_buffer_size = 0;
	for (i32 binding_index = 0; binding_index < layout->num_bindings; ++binding_index)
	{
		Gpu2ResourceBinding* resource_binding = &layout->bindings[binding_index];
		switch(resource_binding->type)
		{
			case GPU2_BINDING_TYPE_BUFFER:
				argument_buffer_size += sizeof(u64);
				break;
			case GPU2_BINDING_TYPE_TEXTURE:
				//FCS TODO:
				assert(false);
				break;
		}
	}

	// Create Argument Buffer
	out_bind_group->metal_argument_buffer = [in_device->metal_device newBufferWithLength:argument_buffer_size options:MTLResourceCPUCacheModeDefaultCache];

	// Write Resources into Argument Buffer
	void* buffer_contents = out_bind_group->metal_argument_buffer.contents;
	for (i32 write_index = 0; write_index < in_create_info->num_writes; ++write_index)
	{
		Gpu2ResourceWrite* resource_write = &in_create_info->writes[write_index];
		Gpu2ResourceBinding* resource_binding = &layout->bindings[write_index];
		assert(resource_write->type == resource_binding->type);
		switch (resource_write->type)
		{
			case GPU2_BINDING_TYPE_BUFFER:
			{
				Gpu2Buffer* buffer = resource_write->buffer_binding.buffer;
				const u64 gpu_address = buffer->metal_buffer.gpuAddress;
				memcpy(buffer_contents, &gpu_address, sizeof(u64));
				buffer_contents += sizeof(u64);
				break;
			}
			case GPU2_BINDING_TYPE_TEXTURE:
			{
				//FCS TODO:
				assert(false);
				break;
			}
		}
	}

	return true;
}

bool gpu2_create_render_pipeline(Gpu2Device* in_device, Gpu2RenderPipelineCreateInfo* in_create_info, Gpu2RenderPipeline* out_render_pipeline)
{	
	MTLRenderPipelineDescriptor *metal_render_pipeline_descriptor = [[MTLRenderPipelineDescriptor alloc] init];
	assert(in_create_info->vertex_shader);
	metal_render_pipeline_descriptor.vertexFunction = in_create_info->vertex_shader->metal_function;
	assert(in_create_info->fragment_shader);
	metal_render_pipeline_descriptor.fragmentFunction = in_create_info->fragment_shader->metal_function;

	//FCS TODO: CreateInfo Arg for color attachments...
	metal_render_pipeline_descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

	*out_render_pipeline = (Gpu2RenderPipeline){
		.metal_render_pipeline_state = [in_device->metal_device newRenderPipelineStateWithDescriptor:metal_render_pipeline_descriptor error:nil],
	};

	return true;
}

bool gpu2_create_buffer(Gpu2Device* in_device, Gpu2BufferCreateInfo* in_create_info, Gpu2Buffer* out_buffer)
{
	*out_buffer = (Gpu2Buffer){};

	if (in_create_info->data)
	{
		out_buffer->metal_buffer = [in_device->metal_device newBufferWithBytes:in_create_info->data length:in_create_info->size options:MTLResourceCPUCacheModeDefaultCache];
	}
	else
	{
		out_buffer->metal_buffer = [in_device->metal_device newBufferWithLength:in_create_info->size options:MTLResourceCPUCacheModeDefaultCache];
	}
	return true;
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
			const float r = color_attachment->clear_color[0];
			const float g = color_attachment->clear_color[1];
			const float b = color_attachment->clear_color[2];
			const float a = color_attachment->clear_color[3];
			metal_render_pass_descriptor.colorAttachments[i].clearColor = MTLClearColorMake(r,g,b,a);
		}
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
}

void gpu2_render_pass_set_bind_group(Gpu2RenderPass* in_render_pass, Gpu2RenderPipeline* in_render_pipeline, Gpu2BindGroup* in_bind_group)
{
	assert(in_render_pass);
	assert(in_bind_group);
	
	//FCS TODO: This might not live long enough...
	Gpu2BindGroupLayout* layout = in_bind_group->create_info.layout;

	for (i32 binding_index = 0; binding_index < layout->num_bindings; ++binding_index)
	{
		Gpu2ResourceBinding* resource_binding = &layout->bindings[binding_index];
	  	Gpu2ResourceWrite* resource_write = &in_bind_group->create_info.writes[binding_index];

		MTLRenderStages metal_render_stages = 0;	
		if (BIT_COMPARE(resource_binding->shader_stages, GPU2_SHADER_STAGE_VERTEX))
		{
			metal_render_stages |= MTLRenderStageVertex;
		}
		if (BIT_COMPARE(resource_binding->shader_stages, GPU2_SHADER_STAGE_FRAGMENT))
		{
			metal_render_stages |= MTLRenderStageFragment;
	  	}
	  		
		[in_render_pass->metal_render_command_encoder useResource:resource_write->buffer_binding.buffer->metal_buffer usage:MTLResourceUsageRead stages:metal_render_stages];
	}

	[in_render_pass->metal_render_command_encoder setVertexBuffer:in_bind_group->metal_argument_buffer offset:0 atIndex:in_bind_group->create_info.index];
	[in_render_pass->metal_render_command_encoder setFragmentBuffer:in_bind_group->metal_argument_buffer offset:0 atIndex:in_bind_group->create_info.index];	
}

void gpu2_render_pass_draw(Gpu2RenderPass* in_render_pass, u32 vertex_start, u32 vertex_count)
{
	//FCS TODO: primitive type arg? 
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

