#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>

#include "app/app.h"
#include "timer.h"

// GPU2_IMPLEMENTATION_<BACKEND> set up in build.sh	
#include "gpu2/gpu2.c"

#include "math/basic_math.h"
#include "math/matrix.h"
#include "math/quat.h"
#include "math/vec.h"

#include "stretchy_buffer.h"

#include "model/static_model.h"
#include "model/animated_model.h"

//FCS TODO: IN ORDER
// Skinned Model Port (kill bindless)
// Game Object Port (kill bindless)
// UI Port

int main()
{
	// Create our window	
	String window_title = string_new("GPU Test (");
	string_append(&window_title, gpu2_get_api_name());
	string_append(&window_title, ")");

	i32 window_width = 1280;
	i32 window_height = 720;
    Window window = window_create(window_title.data, window_width, window_height);

	Gpu2Device gpu2_device;
	gpu2_create_device(&window, &gpu2_device);

	// Set up Static Model
	StaticModel static_model;
	if (!static_model_load("data/meshes/Monkey.glb", &gpu2_device, &static_model))
	{
		printf("Failed to Load Static Model\n");
		return 1;
	}

	// Set up Animated Model 
	AnimatedModel animated_model;
	if (!animated_model_load("data/meshes/cesium_man.glb", &gpu2_device, &animated_model))
	{
		printf("Failed to Load Animated Model\n");
		return 1;
	}

	// Go ahead and allocate the buffer we use to animate our joints
	Mat4* joint_matrices = calloc(animated_model.num_joints, sizeof(Mat4));
	Gpu2BufferCreateInfo joints_buffer_create_info = {
		.usage = GPU2_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = animated_model.joints_buffer_size,
		.data = joint_matrices,
	};
	Gpu2Buffer joint_matrices_buffer;
	gpu2_create_buffer(&gpu2_device, &joints_buffer_create_info, &joint_matrices_buffer);

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

	typedef struct UniformStruct {
		Mat4 model;
		Mat4 view;
		Mat4 projection;
		bool is_skinned;
		uint padding[3];
	} UniformStruct;

	Mat4 view_matrix = mat4_look_at(vec3_new(0,0,0), vec3_new(0,0,1), vec3_new(0,1,0));
	Mat4 proj_matrix = mat4_perspective(60.0f, (float)window_width / (float)window_height, 0.01f, 4000.0f);

	// Static Uniform Data Setup
	Quat static_rotation = quat_identity;
	Vec3 static_translation = vec3_new(-2,0.25,5);

	Mat4 static_model_matrix = mat4_mul_mat4(
		mat4_scale(vec3_new(1, 1, 1)),
		mat4_mul_mat4(
			quat_to_mat4(static_rotation), 
			mat4_translation(static_translation)
		)
	);

	UniformStruct static_uniform_data = {
		.model = static_model_matrix,
		.view = view_matrix,
		.projection = proj_matrix,
		.is_skinned = false,
	};
	
	Gpu2BufferCreateInfo static_uniform_buffer_create_info = {
		.usage = GPU2_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = sizeof(UniformStruct),
		.data = &static_uniform_data,
	};
	Gpu2Buffer static_uniform_buffer;
	gpu2_create_buffer(&gpu2_device, &static_uniform_buffer_create_info, &static_uniform_buffer);

	// Animated Uniform Data Setup
	Quat animated_rotation = quat_new(vec3_new(0,1,0), 180.f * DEGREES_TO_RADIANS); 
	Vec3 animated_translation = vec3_new(2,-0.5,5);

	Mat4 animated_model_matrix = mat4_mul_mat4(
		mat4_scale(vec3_new(1, 1, 1)),
		mat4_mul_mat4(
			quat_to_mat4(animated_rotation), 
			mat4_translation(animated_translation)
		)
	);

	UniformStruct animated_uniform_data = {
		.model = animated_model_matrix,
		.view = view_matrix,
		.projection = proj_matrix,
		.is_skinned = true,
	};
	
	Gpu2BufferCreateInfo animated_uniform_buffer_create_info = {
		.usage = GPU2_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = sizeof(UniformStruct),
		.data = &animated_uniform_data,
	};
	Gpu2Buffer animated_uniform_buffer;
	gpu2_create_buffer(&gpu2_device, &animated_uniform_buffer_create_info, &animated_uniform_buffer);

	//FCS TODO: HERE! Create Separate static and skinned vertex shaders, bind-group-layouts, etc.
	
	// Create Bind Group Layout
	Gpu2BindGroupLayoutCreateInfo bind_group_layout_create_info = {
		.index = 0,
		.num_bindings = 6,
		.bindings = (Gpu2ResourceBinding[6]){
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
			},
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
			},
		},
	};
	Gpu2BindGroupLayout bind_group_layout;
	assert(gpu2_create_bind_group_layout(&gpu2_device, &bind_group_layout_create_info, &bind_group_layout));

	// Create Static Bind Group
	Gpu2BindGroupCreateInfo static_bind_group_create_info = {
		.layout = &bind_group_layout,
	};
	Gpu2BindGroup static_bind_group;
	assert(gpu2_create_bind_group(&gpu2_device, &static_bind_group_create_info, &static_bind_group));

	// Create Animated Bind Group
	Gpu2BindGroupCreateInfo animated_bind_group_create_info = {
		.layout = &bind_group_layout,
	};
	Gpu2BindGroup animated_bind_group;
	assert(gpu2_create_bind_group(&gpu2_device, &animated_bind_group_create_info, &animated_bind_group));

	//  Write updates to our static bind group
	const Gpu2BindGroupUpdateInfo static_bind_group_update_info = {
		.bind_group = &static_bind_group,
		.num_writes = 3,
		.writes = (Gpu2ResourceWrite[3]){
			{
				.binding_index = 0,
				.type = GPU2_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &static_model.vertex_buffer,
				},
			},
			{
				.binding_index = 1,
				.type = GPU2_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &static_model.index_buffer,
				},
			},
			{
				.binding_index = 2,
				.type = GPU2_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &static_uniform_buffer,
				},
			},
		},
	};
	gpu2_update_bind_group(&gpu2_device, &static_bind_group_update_info);

	//  Write updates to our animated bind group
	const Gpu2BindGroupUpdateInfo animated_bind_group_update_info = {
		.bind_group = &animated_bind_group,
		.num_writes = 6,
		.writes = (Gpu2ResourceWrite[6]){
			{
				.binding_index = 0,
				.type = GPU2_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &animated_model.static_vertex_buffer,
				},
			},
			{
				.binding_index = 1,
				.type = GPU2_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &animated_model.index_buffer,
				},
			},
			{
				.binding_index = 2,
				.type = GPU2_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &animated_uniform_buffer,
				},
			},
			{
				.binding_index = 3,
				.type = GPU2_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &animated_model.skinned_vertex_buffer,
				},
			},
			{
				.binding_index = 4,
				.type = GPU2_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &animated_model.inverse_bind_matrices_buffer,
				},
			},
			{
				.binding_index = 5,
				.type = GPU2_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &joint_matrices_buffer,
				},
			},
		},
	};
	gpu2_update_bind_group(&gpu2_device, &animated_bind_group_update_info);

	Gpu2BindGroupLayout* pipeline_bind_group_layouts[] = { &bind_group_layout };

	Gpu2RenderPipelineCreateInfo render_pipeline_create_info = {
		.vertex_shader = &gpu2_vertex_shader,
		.fragment_shader = &gpu2_fragment_shader,
		.num_bind_group_layouts = ARRAY_COUNT(pipeline_bind_group_layouts),
		.bind_group_layouts = pipeline_bind_group_layouts,
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
	gpu2_create_texture(&gpu2_device, &depth_texture_create_info, &depth_texture);

	u64 time = time_now();

	while (window_handle_messages(&window))
	{
		if (window_input_pressed(&window, KEY_ESCAPE))
		{
			break;
		}
		
        const u64 new_time = time_now();
        const double delta_time = time_seconds(new_time - time);
        time = new_time;

		{	// Updating Static Uniform Data
			static_rotation = quat_mul(static_rotation, quat_new(vec3_new(0,1,0), 0.01f));
			static_uniform_data.model = mat4_mul_mat4(quat_to_mat4(static_rotation), mat4_translation(static_translation));
			Gpu2BufferWriteInfo uniform_buffer_write_info = {
				.buffer = &static_uniform_buffer,
				.size = sizeof(UniformStruct),
				.data = &static_uniform_data,
			};
			gpu2_write_buffer(&gpu2_device, &uniform_buffer_write_info);
		}

		{	// Updating Animated Model
			static float current_anim_time = 0.0f;
			static const float animation_rate = 1.0f;
			current_anim_time += (delta_time * animation_rate);
			if (current_anim_time > animated_model.baked_animation.end_time)
			{ 
				current_anim_time = animated_model.baked_animation.start_time;
			}
			if (current_anim_time < animated_model.baked_animation.start_time)
			{ 
				current_anim_time = animated_model.baked_animation.end_time;
			}

			animated_model_update_animation(&animated_model, current_anim_time, joint_matrices);
			Gpu2BufferWriteInfo joint_matrices_buffer_write_info = {
				.buffer = &joint_matrices_buffer,
				.size = sizeof(Mat4) * animated_model.num_joints,
				.data = joint_matrices,
			};
			gpu2_write_buffer(&gpu2_device, &joint_matrices_buffer_write_info);
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

			gpu2_render_pass_set_bind_group(&render_pass, &gpu2_render_pipeline, &static_bind_group);
			gpu2_render_pass_draw(&render_pass, 0, static_model.num_indices);

			gpu2_render_pass_set_bind_group(&render_pass, &gpu2_render_pipeline, &animated_bind_group);
			gpu2_render_pass_draw(&render_pass, 0, animated_model.num_indices);

		}
		gpu2_end_render_pass(&render_pass);

		//4. request present 
		gpu2_present_drawable(&gpu2_device, &command_buffer, &drawable);

		assert(gpu2_commit_command_buffer(&gpu2_device, &command_buffer));
	}

	// Cleanup
	static_model_free(&gpu2_device, &static_model);
	gpu2_destroy_buffer(&gpu2_device, &static_uniform_buffer);
	gpu2_destroy_buffer(&gpu2_device, &animated_uniform_buffer);

	gpu2_destroy_texture(&gpu2_device, &depth_texture);

	gpu2_destroy_bind_group(&gpu2_device, &static_bind_group);
	gpu2_destroy_bind_group(&gpu2_device, &animated_bind_group);
	gpu2_destroy_bind_group_layout(&gpu2_device, &bind_group_layout);

	gpu2_destroy_device(&gpu2_device);

	return 0;
}

