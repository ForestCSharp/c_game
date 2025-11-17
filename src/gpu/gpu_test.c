#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>

#include "app/app.h"
#include "timer.h"

// GPU_IMPLEMENTATION_<BACKEND> set up in build.sh	
#include "gpu/gpu.c"

#include "math/basic_math.h"
#include "math/matrix.h"
#include "math/quat.h"
#include "math/vec.h"

#include "stretchy_buffer.h"

#include "model/static_model.h"
#include "model/animated_model.h"

int main()
{
	// Create our window	
	String window_title = string_new("GPU Test (");
	string_append(&window_title, gpu_get_api_name());
	string_append(&window_title, ")");

	i32 window_width = 1280;
	i32 window_height = 720;
    Window window = window_create(window_title.data, window_width, window_height);

	GpuDevice gpu_device;
	gpu_create_device(&window, &gpu_device);

	// Set up Static Model
	StaticModel static_model;
	if (!static_model_load("data/meshes/Monkey.glb", &gpu_device, &static_model))
	{
		printf("Failed to Load Static Model\n");
		return 1;
	}

	// Set up Animated Model 
	AnimatedModel animated_model;
	if (!animated_model_load("data/meshes/cesium_man.glb", &gpu_device, &animated_model))
	{
		printf("Failed to Load Animated Model\n");
		return 1;
	}

	// Go ahead and allocate the buffer we use to animate our joints
	Mat4* joint_matrices = calloc(animated_model.num_joints, sizeof(Mat4));
	GpuBufferCreateInfo joints_buffer_create_info = {
		.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = animated_model.joints_buffer_size,
		.data = joint_matrices,
	};
	GpuBuffer joint_matrices_buffer;
	gpu_create_buffer(&gpu_device, &joints_buffer_create_info, &joint_matrices_buffer);

	printf("1\n");

	GpuShaderCreateInfo vertex_shader_create_info = {
		.filename = "bin/shaders/gpu_test.vert",
	};
	GpuShader gpu_vertex_shader;
	gpu_create_shader(&gpu_device, &vertex_shader_create_info, &gpu_vertex_shader);

	printf("2\n");

	GpuShaderCreateInfo fragment_shader_create_info = {
		.filename = "bin/shaders/gpu_test.frag",
	};
	GpuShader gpu_fragment_shader;
	gpu_create_shader(&gpu_device, &fragment_shader_create_info, &gpu_fragment_shader);

	printf("3\n");

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
	
	GpuBufferCreateInfo static_uniform_buffer_create_info = {
		.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = sizeof(UniformStruct),
		.data = &static_uniform_data,
	};
	GpuBuffer static_uniform_buffer;
	gpu_create_buffer(&gpu_device, &static_uniform_buffer_create_info, &static_uniform_buffer);

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
	
	GpuBufferCreateInfo animated_uniform_buffer_create_info = {
		.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = sizeof(UniformStruct),
		.data = &animated_uniform_data,
	};
	GpuBuffer animated_uniform_buffer;
	gpu_create_buffer(&gpu_device, &animated_uniform_buffer_create_info, &animated_uniform_buffer);
	
	// Create Bind Group Layout
	GpuBindGroupLayoutCreateInfo bind_group_layout_create_info = {
		.index = 0,
		.num_bindings = 6,
		.bindings = (GpuResourceBinding[6]){
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX | GPU_SHADER_STAGE_FRAGMENT,	
			},
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX | GPU_SHADER_STAGE_FRAGMENT,	
			},
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX | GPU_SHADER_STAGE_FRAGMENT,	
			},
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX | GPU_SHADER_STAGE_FRAGMENT,	
			},
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX | GPU_SHADER_STAGE_FRAGMENT,	
			},
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX | GPU_SHADER_STAGE_FRAGMENT,	
			},
		},
	};
	GpuBindGroupLayout bind_group_layout;
	assert(gpu_create_bind_group_layout(&gpu_device, &bind_group_layout_create_info, &bind_group_layout));

	// Create Static Bind Group
	GpuBindGroupCreateInfo static_bind_group_create_info = {
		.layout = &bind_group_layout,
	};
	GpuBindGroup static_bind_group;
	assert(gpu_create_bind_group(&gpu_device, &static_bind_group_create_info, &static_bind_group));

	// Create Animated Bind Group
	GpuBindGroupCreateInfo animated_bind_group_create_info = {
		.layout = &bind_group_layout,
	};
	GpuBindGroup animated_bind_group;
	assert(gpu_create_bind_group(&gpu_device, &animated_bind_group_create_info, &animated_bind_group));

	//  Write updates to our static bind group
	const GpuBindGroupUpdateInfo static_bind_group_update_info = {
		.bind_group = &static_bind_group,
		.num_writes = 3,
		.writes = (GpuResourceWrite[3]){
			{
				.binding_index = 0,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &static_model.vertex_buffer,
				},
			},
			{
				.binding_index = 1,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &static_model.index_buffer,
				},
			},
			{
				.binding_index = 2,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &static_uniform_buffer,
				},
			},
		},
	};
	gpu_update_bind_group(&gpu_device, &static_bind_group_update_info);

	//  Write updates to our animated bind group
	const GpuBindGroupUpdateInfo animated_bind_group_update_info = {
		.bind_group = &animated_bind_group,
		.num_writes = 6,
		.writes = (GpuResourceWrite[6]){
			{
				.binding_index = 0,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &animated_model.static_vertex_buffer,
				},
			},
			{
				.binding_index = 1,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &animated_model.index_buffer,
				},
			},
			{
				.binding_index = 2,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &animated_uniform_buffer,
				},
			},
			{
				.binding_index = 3,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &animated_model.skinned_vertex_buffer,
				},
			},
			{
				.binding_index = 4,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &animated_model.inverse_bind_matrices_buffer,
				},
			},
			{
				.binding_index = 5,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &joint_matrices_buffer,
				},
			},
		},
	};
	gpu_update_bind_group(&gpu_device, &animated_bind_group_update_info);

	GpuBindGroupLayout* pipeline_bind_group_layouts[] = { &bind_group_layout };

	GpuRenderPipelineCreateInfo render_pipeline_create_info = {
		.vertex_shader = &gpu_vertex_shader,
		.fragment_shader = &gpu_fragment_shader,
		.num_bind_group_layouts = ARRAY_COUNT(pipeline_bind_group_layouts),
		.bind_group_layouts = pipeline_bind_group_layouts,
		.depth_test_enabled = true,
	};
	GpuRenderPipeline gpu_render_pipeline;
	assert(gpu_create_render_pipeline(&gpu_device, &render_pipeline_create_info, &gpu_render_pipeline));	

	GpuTextureCreateInfo depth_texture_create_info = {
		.format = GPU_FORMAT_D32_SFLOAT,	
		.extent = {
			.width = window_width,
			.height = window_height,
			.depth = 1,
		},
		.usage = GPU_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
	};
	GpuTexture depth_texture;
	gpu_create_texture(&gpu_device, &depth_texture_create_info, &depth_texture);

	u64 time = time_now();

    u32 current_frame = 0;
	const u32 swapchain_count = gpu_get_swapchain_count(&gpu_device);
	GpuCommandBuffer command_buffers[swapchain_count];
	for (i32 command_buffer_idx = 0; command_buffer_idx < swapchain_count; ++command_buffer_idx)
	{
		gpu_create_command_buffer(&gpu_device, &command_buffers[command_buffer_idx]);
	}

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
			GpuBufferWriteInfo uniform_buffer_write_info = {
				.buffer = &static_uniform_buffer,
				.size = sizeof(UniformStruct),
				.data = &static_uniform_data,
			};
			gpu_write_buffer(&gpu_device, &uniform_buffer_write_info);
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
			GpuBufferWriteInfo joint_matrices_buffer_write_info = {
				.buffer = &joint_matrices_buffer,
				.size = sizeof(Mat4) * animated_model.num_joints,
				.data = joint_matrices,
			};
			gpu_write_buffer(&gpu_device, &joint_matrices_buffer_write_info);
		}

		// gpu_reset_command_buffer will wait on the command buffer if its still in-flight
		GpuCommandBuffer* command_buffer = &command_buffers[current_frame];
		gpu_reset_command_buffer(&gpu_device, command_buffer);

		GpuDrawable drawable;
		assert(gpu_get_next_drawable(&gpu_device, command_buffer, &drawable));
		GpuTexture drawable_texture;
		assert(gpu_drawable_get_texture(&drawable, &drawable_texture));

		GpuColorAttachmentDescriptor color_attachments[] = {
			{
				.texture = &drawable_texture, 
				.clear_color = {0.392f, 0.584f, 0.929f, 0.f},
				.load_action = GPU_LOAD_ACTION_CLEAR,
				.store_action = GPU_STORE_ACTION_STORE,
			},
		};

		GpuDepthAttachmentDescriptor depth_attachment = {
			.texture = &depth_texture,
			.clear_depth = 1.0f,
			.load_action = GPU_LOAD_ACTION_CLEAR,
			.store_action = GPU_STORE_ACTION_STORE,
		};

		GpuRenderPassCreateInfo render_pass_create_info = {
			.num_color_attachments = ARRAY_COUNT(color_attachments), 
			.color_attachments = color_attachments,
			.depth_attachment = &depth_attachment,
			.command_buffer = command_buffer,
		};
		GpuRenderPass render_pass;
		gpu_begin_render_pass(&gpu_device, &render_pass_create_info, &render_pass);
		{
			gpu_render_pass_set_render_pipeline(&render_pass, &gpu_render_pipeline);

			gpu_render_pass_set_bind_group(&render_pass, &gpu_render_pipeline, &static_bind_group);
			gpu_render_pass_draw(&render_pass, 0, static_model.num_indices);

			gpu_render_pass_set_bind_group(&render_pass, &gpu_render_pipeline, &animated_bind_group);
			gpu_render_pass_draw(&render_pass, 0, animated_model.num_indices);

		}
		gpu_end_render_pass(&render_pass);

		//4. request present 
		gpu_present_drawable(&gpu_device, command_buffer, &drawable);

		assert(gpu_commit_command_buffer(&gpu_device, command_buffer));

		current_frame = (current_frame + 1) % swapchain_count;
	}

	// Cleanup
	static_model_free(&gpu_device, &static_model);
	gpu_destroy_buffer(&gpu_device, &static_uniform_buffer);
	gpu_destroy_buffer(&gpu_device, &animated_uniform_buffer);

	gpu_destroy_texture(&gpu_device, &depth_texture);

	gpu_destroy_bind_group(&gpu_device, &static_bind_group);
	gpu_destroy_bind_group(&gpu_device, &animated_bind_group);
	gpu_destroy_bind_group_layout(&gpu_device, &bind_group_layout);

	gpu_destroy_device(&gpu_device);

	return 0;
}

