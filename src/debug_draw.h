#pragma once

#include "stretchy_buffer.h"
#include "math/math_lib.h"

//FCS TODO: Remove after we support buffer resizing
const u32 DEBUG_DRAW_MAX_VERTICES =	10000000;
const u32 DEBUG_DRAW_MAX_INDICES =	10000000;

typedef struct DebugDrawVertex
{
    Vec4 position;
    Vec4 normal;
    Vec4 color;
} DebugDrawVertex;

typedef struct DebugDrawUniformStruct
{
	Mat4 view;
	Mat4 projection;
} DebugDrawUniformStruct;

typedef struct DebugDrawData
{
	GpuBuffer uniform_buffer; 
	DebugDrawUniformStruct* uniform_data;

	sbuffer(DebugDrawVertex) wireframe_vertices;
	GpuBuffer wireframe_vertex_buffer;
	sbuffer(u32) wireframe_indices;
	GpuBuffer wireframe_index_buffer;
	GpuBindGroup wireframe_bind_group;

	sbuffer(DebugDrawVertex) solid_vertices;
	GpuBuffer solid_vertex_buffer;
	sbuffer(u32) solid_indices;
	GpuBuffer solid_index_buffer;
	GpuBindGroup solid_bind_group;


} DebugDrawData;

typedef struct DebugDrawContext
{
	i32 backbuffer_count;
	i32 current_swapchain_index;
	sbuffer(DebugDrawData) draw_data;

	GpuBindGroupLayout bind_group_layout;
	GpuRenderPipeline wireframe_render_pipeline;
	GpuRenderPipeline solid_render_pipeline;
} DebugDrawContext;

typedef struct DebugDrawRecordInfo
{
	GpuDevice* gpu_device;
	GpuCommandBuffer* command_buffer;
	GpuTexture* color_texture;
	GpuTexture* depth_texture;

} DebugDrawRecordInfo;

// Debug Drawing... essentially ImGui but for 3D Objects (all verts are in world space...)

void debug_draw_init(GpuDevice* in_gpu_device, DebugDrawContext* out_debug_draw_context)
{
	assert(in_gpu_device);
	assert(out_debug_draw_context);

	*out_debug_draw_context = (DebugDrawContext){};

	GpuShaderCreateInfo vertex_shader_create_info = {
		.filename = "bin/shaders/debug_draw.vert",
	};
	GpuShader vertex_shader;
	gpu_create_shader(in_gpu_device, &vertex_shader_create_info, &vertex_shader);

	GpuShaderCreateInfo fragment_shader_create_info = {
		.filename = "bin/shaders/debug_draw.frag",
	};
	GpuShader fragment_shader;
	gpu_create_shader(in_gpu_device, &fragment_shader_create_info, &fragment_shader);

	GpuBindGroupLayoutCreateInfo bind_group_layout_create_info = {
		.index = 0,
		.num_bindings = 3,
		.bindings = (GpuResourceBinding[3]){
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX,	
			},
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX,	
			},
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX,	
			},
		},
	};
	gpu_create_bind_group_layout(in_gpu_device, &bind_group_layout_create_info, &out_debug_draw_context->bind_group_layout);

	GpuBindGroupLayout* pipeline_bind_group_layouts[] = { &out_debug_draw_context->bind_group_layout };

	GpuRenderPipelineCreateInfo render_pipeline_create_info = {
		.vertex_shader = &vertex_shader,
		.fragment_shader = &fragment_shader,
		.num_bind_group_layouts = ARRAY_COUNT(pipeline_bind_group_layouts),
		.bind_group_layouts = pipeline_bind_group_layouts,
		.polygon_mode = GPU_POLYGON_MODE_LINE,
		.depth_test_enabled = true,
	};
	gpu_create_render_pipeline(in_gpu_device, &render_pipeline_create_info, &out_debug_draw_context->wireframe_render_pipeline);
	render_pipeline_create_info.polygon_mode = GPU_POLYGON_MODE_FILL;
	gpu_create_render_pipeline(in_gpu_device, &render_pipeline_create_info, &out_debug_draw_context->solid_render_pipeline);

    gpu_destroy_shader(in_gpu_device, &vertex_shader);
    gpu_destroy_shader(in_gpu_device, &fragment_shader);

	const u32 swapchain_count = gpu_get_swapchain_count(in_gpu_device);
	for (i32 i = 0; i < swapchain_count; ++i)
	{
		// uniform buffer create info
		GpuBufferCreateInfo uniform_buffer_create_info = {
			.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
			.is_cpu_visible = true,
			.size = sizeof(DebugDrawUniformStruct),
			.data = NULL,
		};
		GpuBuffer uniform_buffer;
		gpu_create_buffer(in_gpu_device, &uniform_buffer_create_info, &uniform_buffer);

		// vertex storage buffer create info
		const u32 max_vertices_size = sizeof(DebugDrawVertex) * DEBUG_DRAW_MAX_VERTICES;
		GpuBufferCreateInfo vertex_buffer_create_info = {
			.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
			.is_cpu_visible = true,
			.size = max_vertices_size,
			.data = NULL,
		};
		GpuBuffer wireframe_vertex_buffer;
		gpu_create_buffer(in_gpu_device, &vertex_buffer_create_info, &wireframe_vertex_buffer);
		GpuBuffer solid_vertex_buffer;
		gpu_create_buffer(in_gpu_device, &vertex_buffer_create_info, &solid_vertex_buffer);

		// index storage buffer create info
		const u32 max_indices_size = sizeof(u32) * DEBUG_DRAW_MAX_VERTICES;
		GpuBufferCreateInfo index_buffer_create_info = {
			.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
			.is_cpu_visible = true,
			.size = max_indices_size,
			.data = NULL,
		};
		GpuBuffer wireframe_index_buffer;
		gpu_create_buffer(in_gpu_device, &index_buffer_create_info, &wireframe_index_buffer);
		GpuBuffer solid_index_buffer;
		gpu_create_buffer(in_gpu_device, &index_buffer_create_info, &solid_index_buffer);

		// Create Global Bind Group per swapchain
		GpuBindGroupCreateInfo bind_group_create_info = {
			.layout = &out_debug_draw_context->bind_group_layout,
		};
		GpuBindGroup wireframe_bind_group;
		gpu_create_bind_group(in_gpu_device, &bind_group_create_info, &wireframe_bind_group);

		const GpuBindGroupUpdateInfo wireframe_bind_group_update_info = {
			.bind_group = &wireframe_bind_group,
			.num_writes = 3,
			.writes = (GpuResourceWrite[3]){
				{
					.binding_index = 0,
					.type = GPU_BINDING_TYPE_BUFFER,
					.buffer_binding = {
						.buffer = &uniform_buffer,
					},
				},
				{
					.binding_index = 1,
					.type = GPU_BINDING_TYPE_BUFFER,
					.buffer_binding = {
						.buffer = &wireframe_vertex_buffer,
					},
				},
				{
					.binding_index = 2,
					.type = GPU_BINDING_TYPE_BUFFER,
					.buffer_binding = {
						.buffer = &wireframe_index_buffer,
					},
				},
			},
		};
		gpu_update_bind_group(in_gpu_device, &wireframe_bind_group_update_info);

		GpuBindGroup solid_bind_group;
		gpu_create_bind_group(in_gpu_device, &bind_group_create_info, &solid_bind_group);

		const GpuBindGroupUpdateInfo solid_bind_group_update_info = {
			.bind_group = &solid_bind_group,
			.num_writes = 3,
			.writes = (GpuResourceWrite[3]){
				{
					.binding_index = 0,
					.type = GPU_BINDING_TYPE_BUFFER,
					.buffer_binding = {
						.buffer = &uniform_buffer,
					},
				},
				{
					.binding_index = 1,
					.type = GPU_BINDING_TYPE_BUFFER,
					.buffer_binding = {
						.buffer = &solid_vertex_buffer,
					},
				},
				{
					.binding_index = 2,
					.type = GPU_BINDING_TYPE_BUFFER,
					.buffer_binding = {
						.buffer = &solid_index_buffer,
					},
				},
			},
		};
		gpu_update_bind_group(in_gpu_device, &solid_bind_group_update_info);

		DebugDrawData new_draw_data = {
			.uniform_buffer = uniform_buffer,
			.uniform_data = gpu_map_buffer(in_gpu_device, &uniform_buffer),
			.wireframe_vertex_buffer = wireframe_vertex_buffer,
			.wireframe_index_buffer = wireframe_index_buffer,
			.wireframe_bind_group = wireframe_bind_group,
			.solid_vertex_buffer = solid_vertex_buffer,
			.solid_index_buffer = solid_index_buffer,
			.solid_bind_group = solid_bind_group,
		};
		sb_push(out_debug_draw_context->draw_data, new_draw_data);
	}
}

void debug_draw_shutdown(GpuDevice* in_gpu_device, DebugDrawContext* debug_draw_context)
{
	// FCS TODO: CLEANUP
}

void debug_draw_begin_frame(DebugDrawContext* in_context, const i32 in_swapchain_index)
{
	in_context->current_swapchain_index = in_swapchain_index;
	sb_free(in_context->draw_data[in_context->current_swapchain_index].wireframe_vertices);
	sb_free(in_context->draw_data[in_context->current_swapchain_index].wireframe_indices);
	sb_free(in_context->draw_data[in_context->current_swapchain_index].solid_vertices);
	sb_free(in_context->draw_data[in_context->current_swapchain_index].solid_indices);
}

DebugDrawData* debug_draw_get_current_draw_data(DebugDrawContext* debug_draw_context)
{
	return &debug_draw_context->draw_data[debug_draw_context->current_swapchain_index];
}

void debug_draw_end_frame(DebugDrawContext* debug_draw_context, GpuDevice* in_gpu_device, Mat4* view, Mat4* projection)
{
	DebugDrawData* draw_data = debug_draw_get_current_draw_data(debug_draw_context);

	DebugDrawUniformStruct* uniform_data = draw_data->uniform_data;
	uniform_data->view = *view; 
	uniform_data->projection = *projection;

	//FCS TODO: Just persistently map vtx and idx buffers?

	{
		GpuBufferWriteInfo vertex_buffer_write_info = {
			.buffer = &draw_data->wireframe_vertex_buffer,
			.size = sizeof(DebugDrawVertex) * sb_count(draw_data->wireframe_vertices),
			.data = draw_data->wireframe_vertices,
		};
		gpu_write_buffer(in_gpu_device, &vertex_buffer_write_info);

		GpuBufferWriteInfo index_buffer_write_info = {
			.buffer = &draw_data->wireframe_index_buffer,
			.size = sizeof(u32) * sb_count(draw_data->wireframe_indices),
			.data = draw_data->wireframe_indices,
		};
		gpu_write_buffer(in_gpu_device, &index_buffer_write_info);
	}

	{
		GpuBufferWriteInfo vertex_buffer_write_info = {
			.buffer = &draw_data->solid_vertex_buffer,
			.size = sizeof(DebugDrawVertex) * sb_count(draw_data->solid_vertices),
			.data = draw_data->solid_vertices,
		};
		gpu_write_buffer(in_gpu_device, &vertex_buffer_write_info);

		GpuBufferWriteInfo index_buffer_write_info = {
			.buffer = &draw_data->solid_index_buffer,
			.size = sizeof(u32) * sb_count(draw_data->solid_indices),
			.data = draw_data->solid_indices,
		};
		gpu_write_buffer(in_gpu_device, &index_buffer_write_info);
	}
}

void debug_draw_record_commands(DebugDrawContext* debug_draw_context, DebugDrawRecordInfo* record_info)
{
	DebugDrawData* draw_data = debug_draw_get_current_draw_data(debug_draw_context);

	GpuColorAttachmentDescriptor render_pass_color_attachments[] = {
		{
			.texture = record_info->color_texture, 
			.clear_color = {0.392f, 0.584f, 0.929f, 0.f},
			.load_action = GPU_LOAD_ACTION_LOAD,
			.store_action = GPU_STORE_ACTION_STORE,
		},
	};

	GpuDepthAttachmentDescriptor render_pass_depth_attachment = {
		.texture = record_info->depth_texture,
		.clear_depth = 1.0f,
		.load_action = GPU_LOAD_ACTION_LOAD,
		.store_action = GPU_STORE_ACTION_STORE,
	};

	GpuRenderPassCreateInfo render_pass_create_info = {
		.num_color_attachments = ARRAY_COUNT(render_pass_color_attachments), 
		.color_attachments = render_pass_color_attachments,
		.depth_attachment = &render_pass_depth_attachment,
		.command_buffer = record_info->command_buffer,
	};
	GpuRenderPass render_pass;
	gpu_begin_render_pass(record_info->gpu_device, &render_pass_create_info, &render_pass);
	
	// Draw Wireframe Geometry
	if (sb_count(draw_data->wireframe_indices) > 0)
	{
		gpu_render_pass_set_render_pipeline(&render_pass, &debug_draw_context->wireframe_render_pipeline);
		gpu_render_pass_set_bind_group(&render_pass, &debug_draw_context->wireframe_render_pipeline, &draw_data->wireframe_bind_group);
		gpu_render_pass_draw(&render_pass, 0, sb_count(draw_data->wireframe_indices));
	}

	// Draw Solid Geometry
	if (sb_count(draw_data->solid_indices) > 0)
	{
		gpu_render_pass_set_render_pipeline(&render_pass, &debug_draw_context->solid_render_pipeline);
		gpu_render_pass_set_bind_group(&render_pass, &debug_draw_context->wireframe_render_pipeline, &draw_data->solid_bind_group);
		gpu_render_pass_draw(&render_pass, 0, sb_count(draw_data->solid_indices));
	}

	gpu_end_render_pass(&render_pass);
}

typedef struct DebugDrawSphere
{
	Vec3 center;
	Quat orientation;
	f32 radius;
	i32 latitudes;
	i32 longitudes;
	Vec4 color;
	bool solid;
	bool shade;
} DebugDrawSphere;

void debug_draw_sphere(DebugDrawContext* debug_draw_context, const DebugDrawSphere* debug_draw_sphere)
{
	DebugDrawData* draw_data = debug_draw_get_current_draw_data(debug_draw_context);	
	sbuffer(DebugDrawVertex)* out_vertices = debug_draw_sphere->solid ? &draw_data->solid_vertices : &draw_data->wireframe_vertices;
	sbuffer(u32)* out_indices = debug_draw_sphere->solid ? &draw_data->solid_indices : &draw_data->wireframe_indices;

	const Mat3 orientation_matrix = quat_to_mat3(debug_draw_sphere->orientation);

	//Figure out where our indices should start
	const u32 index_offset = sb_count(*out_vertices);

	const Vec3 sphere_center = debug_draw_sphere->center;
	const f32 radius = debug_draw_sphere->radius;
	const i32 latitudes = MAX(2, debug_draw_sphere->latitudes);
	const i32 longitudes = MAX(3, debug_draw_sphere->longitudes);

	f32 deltaLatitude = PI / (f32) latitudes;
	f32 deltaLongitude = 2.0f * PI / (f32) longitudes;
	
	for (i32 i = 0; i <= latitudes; ++i)
	{
		f32 latitudeAngle = PI / 2.0f - i * deltaLatitude;	/* Starting -pi/2 to pi/2 */
		f32 xz = radius * cosf(latitudeAngle);				/* r * cos(phi) */
		f32 y = radius * sinf(latitudeAngle);				/* r * sin(phi )*/

		/*
			* We add (latitudes + 1) vertices per longitude because of equator,
			* the North pole and South pole are not counted here, as they overlap.
			* The first and last vertices have same position and normal, but
			* different tex coords.
		*/
		for (i32 j = 0; j <= longitudes; ++j)
		{
			f32 longitudeAngle = j * deltaLongitude;

			DebugDrawVertex vertex = {};

			Vec3 local_position = vec3_zero;		
			local_position.x += xz * cosf(longitudeAngle);	
			local_position.y += y;
			local_position.z += xz * sinf(longitudeAngle);		/* z = r * sin(phi) */
			local_position = mat3_mul_vec3(orientation_matrix, local_position);

			Vec3 world_position = vec3_add(sphere_center, local_position);

			vertex.position = vec4_from_vec3(world_position, 1.0);


			Vec3 local_normal = vec3_scale(local_position, 1.0f / radius);
			Vec3 world_normal = mat3_mul_vec3(orientation_matrix, local_normal);
			vertex.normal = vec4_from_vec3(world_normal, 0.0f);

			// 4. SEAMLESS COLORING (x,y,z,w)
			f32 u = (f32)j / (f32)longitudes; 
			f32 v = (f32)i / (f32)latitudes;

			//FCS TODO:
			//vertex.uv.x = u;
			//vertex.uv.y = v;

			vertex.color = debug_draw_sphere->color;
			if (debug_draw_sphere->shade)
			{
				f32 brightness = 0.25f + pow(v, 2.0f);
				//f32 brightness = i % (latitudes / 2);
				vertex.color.x *= brightness;
				vertex.color.y *= brightness;
				vertex.color.z *= brightness;
			}

			sb_push(*out_vertices, vertex);
		}
	}

	/*
	*  Indices
	*  k1--k1+1
	*  |  / |
	*  | /  |
	*  k2--k2+1
	*/
	unsigned int k1, k2;
	for(int i = 0; i < latitudes; ++i)
	{
		k1 = (i * (longitudes + 1));
		k2 = (k1 + longitudes + 1);
		// 2 Triangles per latitude block excluding the first and last longitudes blocks
		for(int j = 0; j < longitudes; ++j, ++k1, ++k2)
		{
			if (i != 0)
			{
				sb_push(*out_indices, index_offset + k1);
				sb_push(*out_indices, index_offset + k2);
				sb_push(*out_indices, index_offset + k1 + 1);
			}

			if (i != (latitudes - 1))
			{
				sb_push(*out_indices, index_offset + k1 + 1);
				sb_push(*out_indices, index_offset + k2);
				sb_push(*out_indices, index_offset + k2 + 1);
			}
		}
	}
}


