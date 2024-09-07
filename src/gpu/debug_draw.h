#pragma once

#include "gpu/gpu.h"
#include "math/matrix.h"
#include "math/quat.h"
#include "math/vec.h"
#include "stretchy_buffer.h"

//FCS TODO: Remove after we support buffer resizing
const u32 DEBUG_DRAW_MAX_VERTICES =	1000000;
const u32 DEBUG_DRAW_MAX_INDICES =	1000000;

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

	sbuffer(DebugDrawVertex) vertices;
	GpuBuffer vertex_buffer;

	sbuffer(u32) indices;
	GpuBuffer index_buffer;	

	GpuDescriptorSet descriptor_set;
} DebugDrawData;

typedef struct DebugDrawContext
{
	i32 backbuffer_count;
	i32 current_swapchain_index;
	sbuffer(DebugDrawData) draw_data;

	GpuDescriptorLayout descriptor_layout;
	GpuPipelineLayout pipeline_layout;
	GpuPipeline pipeline;
} DebugDrawContext;

// Debug Drawing... essentially ImGui but for 3D Objects (all verts are in world space...)

void debug_draw_init(GpuContext* gpu_context, DebugDrawContext* out_debug_draw_context)
{
	assert(gpu_context);
	assert(out_debug_draw_context);

	*out_debug_draw_context = (DebugDrawContext){};

	// create pipeline
    GpuShaderModule vertex_module = gpu_create_shader_module_from_file(gpu_context, "data/shaders/vertex/debug_draw.vert.spv");
	GpuShaderModule fragment_module = gpu_create_shader_module_from_file(gpu_context, "data/shaders/fragment/debug_draw.frag.spv"); 

    GpuPipelineRenderingCreateInfo rendering_info = {
        .color_attachment_count = 1,
        .color_formats =
			// FIXME: Store and Get from Context (as a GpuFormat)
            (GpuFormat *)&gpu_context->surface_format.format, 
        .depth_format = GPU_FORMAT_D32_SFLOAT,
    };

	const GpuDescriptorLayoutCreateInfo debug_draw_descriptor_layout_create_info = {
		.set_number = 0,
		.binding_count = 3,
		.bindings = (GpuDescriptorBinding[3]){
			DESCRIPTOR_BINDING_UNIFORM_BUFFER(0, 1, GPU_SHADER_STAGE_ALL_GRAPHICS),
			DESCRIPTOR_BINDING_STORAGE_BUFFER(1, 1, GPU_SHADER_STAGE_ALL_GRAPHICS),
			DESCRIPTOR_BINDING_STORAGE_BUFFER(2, 1, GPU_SHADER_STAGE_ALL_GRAPHICS),
		},
	};
	out_debug_draw_context->descriptor_layout =  gpu_create_descriptor_layout(gpu_context, &debug_draw_descriptor_layout_create_info);

	GpuDescriptorLayout pipeline_descriptor_layouts[] = { out_debug_draw_context->descriptor_layout };
	i32 pipeline_descriptor_layout_count = sizeof(pipeline_descriptor_layouts) / sizeof(pipeline_descriptor_layouts[0]);
	out_debug_draw_context->pipeline_layout = gpu_create_pipeline_layout(gpu_context, pipeline_descriptor_layout_count, pipeline_descriptor_layouts);

	GpuGraphicsPipelineCreateInfo static_pipeline_create_info = {
		.vertex_module = &vertex_module,
		.fragment_module = &fragment_module,
		.rendering_info = &rendering_info,
		.layout = &out_debug_draw_context->pipeline_layout,
		.num_attributes = 0,
		.attribute_formats = NULL,
		.depth_stencil = {
			.depth_test = true,
			.depth_write = true,
		},
		.rasterizer = {
			.polygon_mode = GPU_POLYGON_MODE_LINE,
		},
	};
	out_debug_draw_context->pipeline = gpu_create_graphics_pipeline(gpu_context, &static_pipeline_create_info);

    gpu_destroy_shader_module(gpu_context, &vertex_module);
    gpu_destroy_shader_module(gpu_context, &fragment_module);

	for (i32 i = 0; i < gpu_context->swapchain_image_count; ++i)
	{
		static const GpuMemoryPropertyFlags memory_properties = 	GPU_MEMORY_PROPERTY_DEVICE_LOCAL 
																| 	GPU_MEMORY_PROPERTY_HOST_VISIBLE
																| 	GPU_MEMORY_PROPERTY_HOST_COHERENT;

		// uniform buffer create info
		GpuBufferCreateInfo uniform_buffer_create_info = {
			.size = sizeof(DebugDrawUniformStruct),
			.usage = GPU_BUFFER_USAGE_UNIFORM_BUFFER,
			.memory_properties = memory_properties, 
			.debug_name = "global_uniform_buffer",
		};

		GpuBuffer uniform_buffer = gpu_create_buffer(gpu_context, &uniform_buffer_create_info);
		// vertex storage buffer create info
		const u32 max_vertices_size = sizeof(DebugDrawVertex) * DEBUG_DRAW_MAX_VERTICES;
		const GpuBufferCreateInfo vertex_buffer_create_info = {
			.size = max_vertices_size,
			.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
			.memory_properties = memory_properties, 
			.debug_name = "debug draw vertex buffer",
		};

		// index storage buffer create info
		const u32 max_indices_size = sizeof(u32) * DEBUG_DRAW_MAX_INDICES;
		const GpuBufferCreateInfo index_buffer_create_info = {
			.size = max_indices_size,
			.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
			.memory_properties = memory_properties, 
			.debug_name = "debug draw index buffer",
		};

		DebugDrawData new_draw_data = {
			.uniform_buffer = uniform_buffer,
			.vertex_buffer = gpu_create_buffer(gpu_context, &vertex_buffer_create_info),
			.index_buffer = gpu_create_buffer(gpu_context, &index_buffer_create_info),
			.uniform_data = gpu_map_buffer(gpu_context, &uniform_buffer),
			.descriptor_set = gpu_create_descriptor_set(gpu_context, &out_debug_draw_context->descriptor_layout),
		};


        GpuDescriptorWrite descriptor_writes[3] = {
			{
				.binding_desc = &out_debug_draw_context->descriptor_layout.bindings[0],
				.index = 0,
				.buffer_write = &(GpuDescriptorWriteBuffer) {
						.buffer = &new_draw_data.uniform_buffer,
						.offset = 0,
						.range = uniform_buffer_create_info.size,
				}
			},
			{
				.binding_desc = &out_debug_draw_context->descriptor_layout.bindings[1],
				.index = 0,
				.buffer_write = &(GpuDescriptorWriteBuffer) {
						.buffer = &new_draw_data.vertex_buffer,
						.offset = 0,
						.range = vertex_buffer_create_info.size,
				}
			},	
			{
				.binding_desc = &out_debug_draw_context->descriptor_layout.bindings[2],
				.index = 0,
				.buffer_write = &(GpuDescriptorWriteBuffer) {
						.buffer = &new_draw_data.index_buffer,
						.offset = 0,
						.range = index_buffer_create_info.size,
				}
			},
		};

		gpu_update_descriptor_set(gpu_context, &new_draw_data.descriptor_set, 3, descriptor_writes);

		sb_push(out_debug_draw_context->draw_data, new_draw_data);
	}
}

void debug_draw_shutdown(GpuContext* gpu_context, DebugDrawContext* debug_draw_context)
{
	//FCS TODO: Clean up resources...
	for (i32 i = 0; i < sb_count(debug_draw_context->draw_data); ++i)
	{
		DebugDrawData* draw_data = &debug_draw_context->draw_data[i];

		gpu_destroy_buffer(gpu_context, &draw_data->vertex_buffer);
		gpu_destroy_buffer(gpu_context, &draw_data->index_buffer);
		gpu_destroy_buffer(gpu_context, &draw_data->uniform_buffer);
		gpu_destroy_descriptor_set(gpu_context, &draw_data->descriptor_set);
	}
	sb_free(debug_draw_context->draw_data);

	gpu_destroy_descriptor_layout(gpu_context, &debug_draw_context->descriptor_layout);
	gpu_destroy_pipeline(gpu_context, &debug_draw_context->pipeline);
	gpu_destroy_pipeline_layout(gpu_context, &debug_draw_context->pipeline_layout);
}

void debug_draw_begin_frame(DebugDrawContext* in_context, const i32 in_swapchain_index)
{
	in_context->current_swapchain_index = in_swapchain_index;
	sb_free(in_context->draw_data[in_context->current_swapchain_index].vertices);
	sb_free(in_context->draw_data[in_context->current_swapchain_index].indices);
}

DebugDrawData* debug_draw_get_current_draw_data(DebugDrawContext* debug_draw_context)
{
	return &debug_draw_context->draw_data[debug_draw_context->current_swapchain_index];
}

void debug_draw_end_frame(DebugDrawContext* debug_draw_context, GpuContext* gpu_context, Mat4* view, Mat4* projection)
{
	//, &view, &projection FCS TODO: Allow buffer resizing. 

	DebugDrawData* draw_data = debug_draw_get_current_draw_data(debug_draw_context);

	DebugDrawUniformStruct* uniform_data = draw_data->uniform_data;
	uniform_data->view = *view; 
	uniform_data->projection = *projection;

	//FCS TODO: Just persistently map our vtx/idx data and do away with sbuffers?
	gpu_upload_buffer(
		gpu_context, 
		&draw_data->vertex_buffer, 
		sizeof(DebugDrawVertex) * sb_count(draw_data->vertices),
		draw_data->vertices
	);
	gpu_upload_buffer(
		gpu_context, 
		&draw_data->index_buffer, 
		sizeof(u32) * sb_count(draw_data->indices),
		draw_data->indices
	);
}

void debug_draw_record_commands(DebugDrawContext* debug_draw_context, GpuCommandBuffer* command_buffer)
{
	DebugDrawData* draw_data = debug_draw_get_current_draw_data(debug_draw_context);
	gpu_cmd_bind_pipeline(command_buffer, &debug_draw_context->pipeline);
	gpu_cmd_bind_descriptor_set(command_buffer, &debug_draw_context->pipeline_layout, &draw_data->descriptor_set);
	gpu_cmd_draw(command_buffer, sb_count(draw_data->indices), 1);
}

typedef struct DebugDrawSphere
{
	Vec3 center;
	f32 radius;
	i32 latitudes;
	i32 longitudes;
	Vec4 color;
} DebugDrawSphere;

void debug_draw_sphere(DebugDrawContext* debug_draw_context, const DebugDrawSphere* debug_draw_sphere)
{
	DebugDrawData* draw_data = debug_draw_get_current_draw_data(debug_draw_context);		
	sbuffer(DebugDrawVertex)* out_vertices = &draw_data->vertices;
	sbuffer(u32)* out_indices = &draw_data->indices;

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

			vertex.position = vec4_from_vec3(sphere_center, 1.0);
			vertex.position.x += xz * cosf(longitudeAngle);	
			vertex.position.y += y;
			vertex.position.z += xz * sinf(longitudeAngle);		/* z = r * sin(phi) */

			//vertex.uv.x = (float) j/longitudes;				/* s */
			//vertex.uv.y = (float) i/latitudes;				/* t */

			const f32 lengthInv = 1.0f / radius; 
			vertex.normal.x = vertex.position.x * lengthInv;
			vertex.normal.y = vertex.position.y * lengthInv;
			vertex.normal.z = vertex.position.z * lengthInv;

			vertex.color = debug_draw_sphere->color;

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


