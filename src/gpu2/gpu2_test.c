#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>

#include "app/app.h"

// GPU2_IMPLEMENTATION_<BACKEND> set up in build.sh	
#include "gpu2/gpu2.c"

#include "math/basic_math.h"
#include "math/matrix.h"
#include "math/quat.h"
#include "math/vec.h"

#include "stretchy_buffer.h"

typedef struct Gpu2TestVertex
{
    Vec4 position;
    Vec4 normal;
    Vec4 color;
    Vec2 uv;
	Vec2 padding;
} Gpu2TestVertex;

void gpu2_test_append_box(const Vec3 axes[3], const float half_extents[3], sbuffer(Gpu2TestVertex)* out_vertices, sbuffer(u32)* out_indices);

int main()
{
	// Create our window
	i32 window_width = 1280;
	i32 window_height = 720;
    Window window = window_create("C Game", window_width, window_height);

	Gpu2Device gpu2_device;
	assert(gpu2_create_device(&window, &gpu2_device));

	Gpu2ShaderCreateInfo vertex_shader_create_info = {
		.filename = "bin/shaders/gpu2_test.vert",
	};
	Gpu2Shader gpu2_vertex_shader;
	assert(gpu2_create_shader(&gpu2_device, &vertex_shader_create_info, &gpu2_vertex_shader));

	Gpu2ShaderCreateInfo fragment_shader_create_info = {
		.filename = "bin/shaders/gpu2_test.frag",
	};
	Gpu2Shader gpu2_fragment_shader;
	assert(gpu2_create_shader(&gpu2_device, &fragment_shader_create_info, &gpu2_fragment_shader));

	sbuffer(Gpu2TestVertex) gpu2_vertices = NULL;
	sbuffer(u32) gpu2_indices = NULL;

	Vec3 box_axes[3] = {
		vec3_new(1,0,0),
		vec3_new(0,1,0),
		vec3_new(0,0,1)
	};
	float box_halfwidths[3] = {1.0f, 1.0f, 1.0f};
	gpu2_test_append_box(box_axes, box_halfwidths, &gpu2_vertices, &gpu2_indices);	

	Gpu2BufferCreateInfo vertex_buffer_create_info = {
		.is_cpu_visible = true,
		.size = sb_count(gpu2_vertices) * sizeof(Gpu2TestVertex),
		.data = gpu2_vertices,
	};
	Gpu2Buffer vertex_buffer;
	assert(gpu2_create_buffer(&gpu2_device, &vertex_buffer_create_info, &vertex_buffer));

	Gpu2BufferCreateInfo index_buffer_create_info = {
		.is_cpu_visible = true,
		.size = sb_count(gpu2_indices) * sizeof(u32),
		.data = gpu2_indices,
	};
	Gpu2Buffer index_buffer;
	assert(gpu2_create_buffer(&gpu2_device, &index_buffer_create_info, &index_buffer));

	typedef struct Gpu2TestUniformStruct {
		Mat4 model;
		Mat4 view;
		Mat4 projection;
	} Gpu2TestUniformStruct;

	Quat rotation = quat_identity;
	Vec3 translation = vec3_new(0,0,5);

	Mat4 model_matrix = mat4_mul_mat4(quat_to_mat4(rotation), mat4_translation(translation));

	//FCS TODO: Only write in loop. Not initially
	Gpu2TestUniformStruct gpu2_uniform_data = {
		.model = model_matrix,
		.view = mat4_look_at(vec3_new(0,0,0), vec3_new(0,0,1), vec3_new(0,1,0)),
		.projection = mat4_perspective(60.0f, (float)window_width / (float)window_height, 0.01f, 4000.0f),
	};
	
	Gpu2BufferCreateInfo uniform_buffer_create_info = {
		.is_cpu_visible = true,
		.size = sizeof(Gpu2TestUniformStruct),
		.data = &gpu2_uniform_data,
	};
	Gpu2Buffer uniform_buffer;
	assert(gpu2_create_buffer(&gpu2_device, &uniform_buffer_create_info, &uniform_buffer));

	Gpu2ResourceBinding bindings[] = 
	{
		{
			.type = GPU2_BINDING_TYPE_BUFFER,
			.shader_stages = GPU2_SHADER_STAGE_VERTEX | GPU2_SHADER_STAGE_FRAGMENT,	
		},
		{	
			.type = GPU2_BINDING_TYPE_BUFFER,
			.shader_stages = GPU2_SHADER_STAGE_VERTEX | GPU2_SHADER_STAGE_FRAGMENT,	
		},
		{	
			.type = GPU2_BINDING_TYPE_BUFFER,
			.shader_stages = GPU2_SHADER_STAGE_VERTEX | GPU2_SHADER_STAGE_FRAGMENT,	
		}
	};

	Gpu2BindGroupLayoutCreateInfo bind_group_layout_create_info = {
		.index = 0,
		.num_bindings = ARRAY_COUNT(bindings),
		.bindings = bindings,
	};

	Gpu2BindGroupLayout bind_group_layout;
	assert(gpu2_create_bind_group_layout(&gpu2_device, &bind_group_layout_create_info, &bind_group_layout));

	Gpu2ResourceWrite writes[] = {
		{
			.type = GPU2_BINDING_TYPE_BUFFER,
			.buffer_binding = {
				.buffer = &vertex_buffer,
			},
		},
		{
			.type = GPU2_BINDING_TYPE_BUFFER,
			.buffer_binding = {
				.buffer = &index_buffer,
			},
		},
		{
			.type = GPU2_BINDING_TYPE_BUFFER,
			.buffer_binding = {
				.buffer = &uniform_buffer,
			},
		}
	};

	Gpu2BindGroupCreateInfo bind_group_create_info = {
		.layout = &bind_group_layout,
		.num_writes = ARRAY_COUNT(writes),
		.writes = writes,
	};

	Gpu2BindGroup bind_group;
	assert(gpu2_create_bind_group(&gpu2_device, &bind_group_create_info, &bind_group));		

	Gpu2BindGroup* pipeline_bind_groups[] = { &bind_group };

	Gpu2RenderPipelineCreateInfo render_pipeline_create_info = {
		.vertex_shader = &gpu2_vertex_shader,
		.fragment_shader = &gpu2_fragment_shader,
		.num_bind_groups = ARRAY_COUNT(pipeline_bind_groups),
		.bind_groups = pipeline_bind_groups,
		.depth_test_enabled = true,
	};
	Gpu2RenderPipeline gpu2_render_pipeline;
	assert(gpu2_create_render_pipeline(&gpu2_device, &render_pipeline_create_info, &gpu2_render_pipeline));	

	Gpu2TextureCreateInfo depth_texture_create_info = {
		.format = GPU2_FORMAT_D32_SFLOAT,	
		.extent = {
			.width = window_width,
			.height = window_height,
			.depth = 1,
		},
		.usage = GPU2_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
	};
	Gpu2Texture depth_texture;
	assert(gpu2_create_texture(&gpu2_device, &depth_texture_create_info, &depth_texture));

	while (window_handle_messages(&window))
	{
		if (window_input_pressed(&window, KEY_ESCAPE))
		{
			break;
		}

		{	// Updating Uniform Data
			rotation = quat_mul(rotation, quat_new(vec3_new(0,1,0), 0.01f));
			gpu2_uniform_data.model = mat4_mul_mat4(quat_to_mat4(rotation), mat4_translation(translation));
			Gpu2BufferWriteInfo uniform_buffer_write_info = {
				.size = sizeof(Gpu2TestUniformStruct),
				.data = &gpu2_uniform_data,
			};
			gpu2_write_buffer(&gpu2_device, &uniform_buffer, &uniform_buffer_write_info);
		}


		Gpu2CommandBuffer command_buffer;
		assert(gpu2_create_command_buffer(&gpu2_device, &command_buffer));

		Gpu2Drawable drawable;
		assert(gpu2_get_next_drawable(&gpu2_device, &command_buffer, &drawable));
		Gpu2Texture drawable_texture;
		assert(gpu2_drawable_get_texture(&drawable, &drawable_texture));

		Gpu2ColorAttachmentDescriptor color_attachments[] = {
			{
				.texture = &drawable_texture, 
				.clear_color = {0.392f, 0.584f, 0.929f, 0.f},
				.load_action = GPU2_LOAD_ACTION_CLEAR,
				.store_action = GPU2_STORE_ACTION_STORE,
			},
		};

		Gpu2DepthAttachmentDescriptor depth_attachment = {
			.texture = &depth_texture,
			.clear_depth = 1.0f,
			.load_action = GPU2_LOAD_ACTION_CLEAR,
			.store_action = GPU2_STORE_ACTION_STORE,
		};

		Gpu2RenderPassCreateInfo render_pass_create_info = {
			.num_color_attachments = ARRAY_COUNT(color_attachments), 
			.color_attachments = color_attachments,
			.depth_attachment = &depth_attachment,
			.command_buffer = &command_buffer,
		};
		Gpu2RenderPass render_pass;
		gpu2_begin_render_pass(&gpu2_device, &render_pass_create_info, &render_pass);
		{
			gpu2_render_pass_set_render_pipeline(&render_pass, &gpu2_render_pipeline);
			gpu2_render_pass_set_bind_group(&render_pass, &gpu2_render_pipeline, &bind_group);
			gpu2_render_pass_draw(&render_pass, 0, sb_count(gpu2_indices));
		}
		gpu2_end_render_pass(&render_pass);

		//4. request present 
		gpu2_present_drawable(&gpu2_device, &command_buffer, &drawable);

		assert(gpu2_commit_command_buffer(&gpu2_device, &command_buffer));
	}

	return 0;
}

void gpu2_test_append_box(const Vec3 axes[3], const float half_extents[3], sbuffer(Gpu2TestVertex)* out_vertices, sbuffer(u32)* out_indices)
{
	u32 index_offset = sb_count(*out_vertices);

	Vec4 colors[6] = 
	{
		vec4_new(1,0,0,1),
		vec4_new(0,1,0,1),
		vec4_new(0,0,1,1),
		vec4_new(1,1,0,1),
		vec4_new(1,0,1,1),
		vec4_new(0,1,1,1),
	};

	i32 current_color_idx = 0;

	for (i32 axis_idx = 0; axis_idx < 3; ++axis_idx)
	{
		Vec3 positive_quad_center = vec3_scale(axes[axis_idx], half_extents[axis_idx]);
		Vec3 negative_quad_center = vec3_scale(axes[axis_idx], -half_extents[axis_idx]);

		i32 other_axis_a = (axis_idx + 1) % 3;
		Vec3 offset_a = vec3_scale(axes[other_axis_a], half_extents[other_axis_a]);
		
		i32 other_axis_b = (axis_idx + 2) % 3;
		Vec3 offset_b = vec3_scale(axes[other_axis_b], half_extents[other_axis_b]);

		Vec3 positions[8] = 
		{
			vec3_add(vec3_add(positive_quad_center, offset_a), offset_b),
			vec3_add(vec3_sub(positive_quad_center, offset_a), offset_b),
			vec3_sub(vec3_add(positive_quad_center, offset_a), offset_b),
			vec3_sub(vec3_sub(positive_quad_center, offset_a), offset_b),
			vec3_add(vec3_add(negative_quad_center, offset_a), offset_b),
			vec3_add(vec3_sub(negative_quad_center, offset_a), offset_b),
			vec3_sub(vec3_add(negative_quad_center, offset_a), offset_b),
			vec3_sub(vec3_sub(negative_quad_center, offset_a), offset_b),
		};

		Vec2 uvs[8] = 
		{
			vec2_new(1.0, 0.0),
			vec2_new(1.0, 1.0),
			vec2_new(0.0, 1.0),
			vec2_new(0.0, 0.0),
			vec2_new(1.0, 0.0),
			vec2_new(1.0, 1.0),
			vec2_new(0.0, 1.0),
			vec2_new(0.0, 0.0),
		};

		for (i32 vtx_idx = 0; vtx_idx < 8; ++vtx_idx)
		{
			Vec4 color = colors[vtx_idx > 3 ? current_color_idx + 1: current_color_idx];

			Gpu2TestVertex vtx = {
				.position = vec4_from_vec3(positions[vtx_idx], 1.0),	// Set up later
				.normal = vec4_from_vec3(axes[axis_idx], 0.0),			// Point in direction of axis
				.color = color, 										// Color
				.uv = uvs[vtx_idx], 									// Set Up Later
			};
			sb_push(*out_vertices, vtx);
		}

		current_color_idx += 2;

		sb_push(*out_indices, index_offset + 0);
		sb_push(*out_indices, index_offset + 1);
		sb_push(*out_indices, index_offset + 2);
		sb_push(*out_indices, index_offset + 1);
		sb_push(*out_indices, index_offset + 2);
		sb_push(*out_indices, index_offset + 3);
		index_offset += 4;

		sb_push(*out_indices, index_offset + 0);
		sb_push(*out_indices, index_offset + 1);
		sb_push(*out_indices, index_offset + 2);
		sb_push(*out_indices, index_offset + 1);
		sb_push(*out_indices, index_offset + 2);
		sb_push(*out_indices, index_offset + 3);
		index_offset += 4;
	}
}
