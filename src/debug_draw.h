#pragma once

#include "stretchy_buffer.h"
#include "math/math_lib.h"

//FCS TODO: Remove after we support buffer resizing
const u32 DEBUG_DRAW_MAX_VERTICES =	1000000;
const u32 DEBUG_DRAW_MAX_INDICES =	1000000;

typedef enum DebugDrawType
{
	DEBUG_DRAW_TYPE_WIREFRAME,
	DEBUG_DRAW_TYPE_SOLID,
	_DEBUG_DRAW_TYPE_COUNT,
} DebugDrawType;

#define FOR_EACH_DRAW_LIST_TYPE(iter, ...) \
    for (DebugDrawType iter = 0; iter < _DEBUG_DRAW_TYPE_COUNT; ++iter) \
    { \
        __VA_ARGS__ \
    }

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

typedef struct DebugDrawList
{
	sbuffer(DebugDrawVertex) vertices;
	GpuBuffer vertex_buffer;
	sbuffer(u32) indices;
	GpuBuffer index_buffer;
	GpuBindGroup bind_group;
} DebugDrawList;

DebugDrawList debug_draw_list_init(GpuDevice* in_gpu_device, GpuBindGroupLayout* in_bind_group_layout, GpuBuffer* in_uniform_buffer)
{	
	// vertex storage buffer create info
	const u32 max_vertices_size = sizeof(DebugDrawVertex) * DEBUG_DRAW_MAX_VERTICES;
	GpuBufferCreateInfo vertex_buffer_create_info = {
		.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = max_vertices_size,
		.data = NULL,
	};
	GpuBuffer vertex_buffer;
	gpu_create_buffer(in_gpu_device, &vertex_buffer_create_info, &vertex_buffer);

	// index storage buffer create info
	const u32 max_indices_size = sizeof(u32) * DEBUG_DRAW_MAX_VERTICES;
	GpuBufferCreateInfo index_buffer_create_info = {
		.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = max_indices_size,
		.data = NULL,
	};
	GpuBuffer index_buffer;
	gpu_create_buffer(in_gpu_device, &index_buffer_create_info, &index_buffer);

	// Create Global Bind Group per swapchain
	GpuBindGroupCreateInfo bind_group_create_info = {
		.layout = in_bind_group_layout,
	};
	GpuBindGroup bind_group;
	gpu_create_bind_group(in_gpu_device, &bind_group_create_info, &bind_group);

	const GpuBindGroupUpdateInfo bind_group_update_info = {
		.bind_group = &bind_group,
		.num_writes = 3,
		.writes = (GpuResourceWrite[3]){
			{
				.binding_index = 0,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = in_uniform_buffer,
				},
			},
			{
				.binding_index = 1,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &vertex_buffer,
				},
			},
			{
				.binding_index = 2,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &index_buffer,
				},
			},
		},
	};
	gpu_update_bind_group(in_gpu_device, &bind_group_update_info);

	return (DebugDrawList) {
		.vertices = NULL,
		.vertex_buffer = vertex_buffer,
		.indices = NULL,
		.index_buffer = index_buffer,
		.bind_group = bind_group,
	};
}

void debug_draw_list_reset(DebugDrawList* in_draw_list)
{
	assert(in_draw_list);
	sb_free(in_draw_list->vertices);
	sb_free(in_draw_list->indices);
}

void debug_draw_list_write_buffers(DebugDrawList* in_draw_list, GpuDevice* in_gpu_device)
{
		GpuBufferWriteInfo vertex_buffer_write_info = {
			.buffer = &in_draw_list->vertex_buffer,
			.size = sizeof(DebugDrawVertex) * sb_count(in_draw_list->vertices),
			.data = in_draw_list->vertices,
		};
		gpu_write_buffer(in_gpu_device, &vertex_buffer_write_info);

		GpuBufferWriteInfo index_buffer_write_info = {
			.buffer = &in_draw_list->index_buffer,
			.size = sizeof(u32) * sb_count(in_draw_list->indices),
			.data = in_draw_list->indices,
		};
		gpu_write_buffer(in_gpu_device, &index_buffer_write_info);
}

typedef struct DebugDrawData
{
	GpuBuffer uniform_buffer; 
	DebugDrawUniformStruct* uniform_data;	

	// Draw lists, one per draw type
	DebugDrawList draw_lists[_DEBUG_DRAW_TYPE_COUNT];
} DebugDrawData;

typedef struct DebugDrawContext
{
	i32 backbuffer_count;
	i32 current_swapchain_index;
	sbuffer(DebugDrawData) draw_data;

	// Bind group layout common to all debug drawing operations
	GpuBindGroupLayout bind_group_layout;

	// Render pipelines, one per draw type
	GpuRenderPipeline render_pipelines[_DEBUG_DRAW_TYPE_COUNT];
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
	gpu_create_render_pipeline(in_gpu_device, &render_pipeline_create_info, &out_debug_draw_context->render_pipelines[DEBUG_DRAW_TYPE_WIREFRAME]);
	render_pipeline_create_info.polygon_mode = GPU_POLYGON_MODE_FILL;
	gpu_create_render_pipeline(in_gpu_device, &render_pipeline_create_info, &out_debug_draw_context->render_pipelines[DEBUG_DRAW_TYPE_SOLID]);

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

		DebugDrawData new_draw_data = {
			.uniform_buffer			= uniform_buffer,
			.uniform_data			= gpu_map_buffer(in_gpu_device, &uniform_buffer),
		};

		// Init draw type draw lists
		FOR_EACH_DRAW_LIST_TYPE(draw_type_idx,
			new_draw_data.draw_lists[draw_type_idx] = debug_draw_list_init(in_gpu_device, &out_debug_draw_context->bind_group_layout, &uniform_buffer);
		);

		sb_push(out_debug_draw_context->draw_data, new_draw_data);
	}
}

void debug_draw_shutdown(GpuDevice* in_gpu_device, DebugDrawContext* debug_draw_context)
{
	// FCS TODO: CLEANUP
}

DebugDrawData* debug_draw_get_current_draw_data(DebugDrawContext* debug_draw_context)
{
	return &debug_draw_context->draw_data[debug_draw_context->current_swapchain_index];
}

DebugDrawList* debug_draw_get_current_draw_list(DebugDrawContext* debug_draw_context, DebugDrawType in_draw_type)
{
	assert(in_draw_type < _DEBUG_DRAW_TYPE_COUNT);
	DebugDrawData* draw_data = debug_draw_get_current_draw_data(debug_draw_context);
	return &draw_data->draw_lists[in_draw_type];
}

void debug_draw_begin_frame(DebugDrawContext* in_context, const i32 in_swapchain_index)
{
	in_context->current_swapchain_index = in_swapchain_index;
	DebugDrawData* current_draw_data = debug_draw_get_current_draw_data(in_context);

	FOR_EACH_DRAW_LIST_TYPE(draw_type_idx,
		debug_draw_list_reset(&current_draw_data->draw_lists[draw_type_idx]);
	);
}

void debug_draw_end_frame(DebugDrawContext* debug_draw_context, GpuDevice* in_gpu_device, Mat4* view, Mat4* projection)
{
	DebugDrawData* current_draw_data = debug_draw_get_current_draw_data(debug_draw_context);

	DebugDrawUniformStruct* uniform_data = current_draw_data->uniform_data;
	uniform_data->view = *view; 
	uniform_data->projection = *projection;

	FOR_EACH_DRAW_LIST_TYPE(draw_type_idx,
		debug_draw_list_write_buffers(&current_draw_data->draw_lists[draw_type_idx], in_gpu_device);
	);
}

void debug_draw_record_commands(DebugDrawContext* debug_draw_context, DebugDrawRecordInfo* record_info)
{
	DebugDrawData* current_draw_data = debug_draw_get_current_draw_data(debug_draw_context);

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

	FOR_EACH_DRAW_LIST_TYPE(draw_type_idx,
		GpuRenderPipeline* render_pipeline = &debug_draw_context->render_pipelines[draw_type_idx];
		DebugDrawList* draw_list = &current_draw_data->draw_lists[draw_type_idx];
		const u32 draw_list_index_count = sb_count(draw_list->indices);
		if (draw_list_index_count > 0)
		{
			gpu_render_pass_set_render_pipeline(&render_pass, render_pipeline);
			gpu_render_pass_set_bind_group(&render_pass, render_pipeline, &draw_list->bind_group);
			gpu_render_pass_draw(&render_pass, 0, draw_list_index_count);
		}
	);
	
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
	DebugDrawType draw_type;
	bool shade;
} DebugDrawSphere;

void debug_draw_sphere(DebugDrawContext* debug_draw_context, const DebugDrawSphere* debug_draw_sphere)
{
	// Get the correct draw list based on our draw type
	DebugDrawList* draw_list = debug_draw_get_current_draw_list(debug_draw_context, debug_draw_sphere->draw_type);

	const Mat3 orientation_matrix = quat_to_mat3(debug_draw_sphere->orientation);

	//Figure out where our indices should start
	const u32 index_offset = sb_count(draw_list->vertices);

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

			sb_push(draw_list->vertices, vertex);
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
				sb_push(draw_list->indices, index_offset + k1);
				sb_push(draw_list->indices, index_offset + k2);
				sb_push(draw_list->indices, index_offset + k1 + 1);
			}

			if (i != (latitudes - 1))
			{
				sb_push(draw_list->indices, index_offset + k1 + 1);
				sb_push(draw_list->indices, index_offset + k2);
				sb_push(draw_list->indices, index_offset + k2 + 1);
			}
		}
	}
}

typedef struct DebugDrawBox
{
	Vec3 center;
	Quat orientation;
	Vec3 extents;
	Vec4 color;
	DebugDrawType draw_type;
	bool shade;
} DebugDrawBox;

void debug_draw_box(DebugDrawContext* debug_draw_context, const DebugDrawBox* debug_draw_box)
{
    // Get the correct draw list based on our draw type
    DebugDrawList* draw_list = debug_draw_get_current_draw_list(debug_draw_context, debug_draw_box->draw_type);

    const Mat3 orientation_matrix = quat_to_mat3(debug_draw_box->orientation);
    const Vec3 center = debug_draw_box->center;
    const Vec3 half_extents = debug_draw_box->extents;
    const Vec4 color = debug_draw_box->color;

    // Calculate the 8 corners of the box in local space
    Vec3 local_corners[8] = {
        {-half_extents.x, -half_extents.y, -half_extents.z},
        { half_extents.x, -half_extents.y, -half_extents.z},
        { half_extents.x,  half_extents.y, -half_extents.z},
        {-half_extents.x,  half_extents.y, -half_extents.z},
        {-half_extents.x, -half_extents.y,  half_extents.z},
        { half_extents.x, -half_extents.y,  half_extents.z},
        { half_extents.x,  half_extents.y,  half_extents.z},
        {-half_extents.x,  half_extents.y,  half_extents.z},
    };

    // Transform corners to world space
    Vec3 world_corners[8];
    for (i32 i = 0; i < 8; ++i)
    {
        world_corners[i] = mat3_mul_vec3(orientation_matrix, local_corners[i]);
        world_corners[i] = vec3_add(center, world_corners[i]);
    }

    // Calculate normals for each face (in world space)
    Vec3 face_normals[6] = {
        { 0.0f,  0.0f, -1.0f}, // front
        { 0.0f,  0.0f,  1.0f}, // back
        {-1.0f,  0.0f,  0.0f}, // left
        { 1.0f,  0.0f,  0.0f}, // right
        { 0.0f, -1.0f,  0.0f}, // bottom
        { 0.0f,  1.0f,  0.0f}, // top
    };
    for (i32 i = 0; i < 6; ++i)
    {
        face_normals[i] = mat3_mul_vec3(orientation_matrix, face_normals[i]);
    }

    // Figure out where our indices should start
    u32 index_offset = sb_count(draw_list->vertices);

    // Add vertices for each face (2 triangles per face)
    const u32 indices_per_face = 6;
    const u32 vertices_per_face = 4;

    // Face order: front, back, left, right, bottom, top
    const u32 face_corner_indices[6][4] = {
        {0, 1, 2, 3}, // front
        {4, 5, 6, 7}, // back
        {0, 3, 7, 4}, // left
        {1, 2, 6, 5}, // right
        {0, 1, 5, 4}, // bottom
        {3, 2, 6, 7}, // top
    };

    for (i32 face = 0; face < 6; ++face)
    {
        for (i32 i = 0; i < 4; ++i)
        {
            DebugDrawVertex vertex = {};
            vertex.position = vec4_from_vec3(world_corners[face_corner_indices[face][i]], 1.0f);
            vertex.normal = vec4_from_vec3(face_normals[face], 0.0f);
            vertex.color = color;
            sb_push(draw_list->vertices, vertex);
        }

        // Two triangles per face
        sb_push(draw_list->indices, index_offset + 0);
        sb_push(draw_list->indices, index_offset + 1);
        sb_push(draw_list->indices, index_offset + 2);
        sb_push(draw_list->indices, index_offset + 0);
        sb_push(draw_list->indices, index_offset + 2);
        sb_push(draw_list->indices, index_offset + 3);
        index_offset += 4;
    }

    // Optional: Apply shading if enabled
    if (debug_draw_box->shade)
    {
        const u32 num_vertices = sb_count(draw_list->vertices);
        for (u32 i = index_offset - 24; i < num_vertices; ++i)
        {
            f32 brightness = 0.25f + 0.75f * fabs((draw_list->vertices)[i].normal.x + (draw_list->vertices)[i].normal.y + (draw_list->vertices)[i].normal.z) / 3.0f;
            (draw_list->vertices)[i].color.x *= brightness;
            (draw_list->vertices)[i].color.y *= brightness;
            (draw_list->vertices)[i].color.z *= brightness;
        }
    }
}
