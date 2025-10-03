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
#include "game_object/game_object.h"
#include "uniforms.h"

// FCS TODO: Gpu2 Porting Checklist
// UI Port
// Delete old GPU directory
// Rename all 'Gpu2' to just 'Gpu'

int main()
{
	// Create our window
	i32 window_width = 1280;
	i32 window_height = 720;
    Window window = window_create("C Game", window_width, window_height);

	Gpu2Device gpu2_device;
	assert(gpu2_create_device(&window, &gpu2_device));

	//GameObject + Components Setup
	GameObjectManager game_object_manager = {};
	GameObjectManager* game_object_manager_ptr = &game_object_manager;
	game_object_manager_init(game_object_manager_ptr);

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

	//FCS TODO: Wrap these in a "Character" struct
	GameObjectHandle root_object_handle = ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle body_object_handle = ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle head_object_handle = ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle legs_object_handle = ADD_OBJECT(game_object_manager_ptr);	
	GameObjectHandle left_arm_object_handle = ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle right_arm_object_handle = ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle camera_root_object_handle = ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle camera_object_handle = ADD_OBJECT(game_object_manager_ptr);
	{	// Set up Main character hierarchy
		TransformComponent root_transform = {
			.trs = {
				.scale = vec3_new(5,5,5),
				.rotation = quat_identity,
				.translation = vec3_new(0,0,0),
			},
			.parent = {},
		};
		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, root_object_handle, root_transform);

		PlayerControlComponent player_control = {
			.move_speed = 100.0f,
		};
		OBJECT_CREATE_COMPONENT(PlayerControlComponent, game_object_manager_ptr, root_object_handle, player_control);

		ColliderComponent collider_comp = {
			.collider = {
				.type = COLLIDER_TYPE_CAPSULE,
				.capsule = {
					.center = vec3_zero,
					.orientation = quat_identity,
					.half_height = 5.0f,
				},
			},
		};
		//FCS TODO: Collision Routines will need to handle xforms...
		OBJECT_CREATE_COMPONENT(ColliderComponent, game_object_manager_ptr, root_object_handle, collider_comp);

		StaticModel body_static_model;
		assert(static_model_load("data/meshes/StarterMech_Body.glb", &gpu2_device, &body_static_model));
		StaticModelComponent body_model_component_data = {.static_model = body_static_model, };
		OBJECT_CREATE_COMPONENT(StaticModelComponent, game_object_manager_ptr, body_object_handle, body_model_component_data);

		TransformComponent body_transform = {
			.trs = {
				.scale = vec3_new(1,1,1),
				.rotation = quat_identity,
				.translation = vec3_new(0,0,0),
			},
			.parent = optional_init({
				.object_handle = root_object_handle,
				.name = optional_none(),	
			}),
		};
		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, body_object_handle, body_transform);

		StaticModel head_static_model;
		assert(static_model_load("data/meshes/StarterMech_Head.glb", &gpu2_device, &head_static_model));
		StaticModelComponent head_model_component_data = {.static_model = head_static_model, };
		OBJECT_CREATE_COMPONENT(StaticModelComponent, game_object_manager_ptr, head_object_handle, head_model_component_data);

		TransformComponent head_transform = {
			.trs = {
				.scale = vec3_new(1,1,1),
				.rotation = quat_identity,
				.translation = vec3_new(0,2.5,0),
			},
			.parent = optional_init({
				.object_handle = body_object_handle,
				.name = optional_none(),
			}),
		};
		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, head_object_handle, head_transform);

		StaticModel arm_static_model;
		assert(static_model_load("data/meshes/StarterMech_Arm.glb", &gpu2_device, &arm_static_model));
		StaticModelComponent arm_model_component_data = {.static_model = arm_static_model, };
		
		// Left Arm
		OBJECT_CREATE_COMPONENT(StaticModelComponent, game_object_manager_ptr, left_arm_object_handle, arm_model_component_data);

		TransformComponent left_arm_transform = {
			.trs = {
				.scale = vec3_new(1,1,1),
				.rotation = quat_identity,
				.translation = vec3_new(3.25,0,0),
			},
			.parent = optional_init({
				.object_handle = body_object_handle,
				.name = optional_none(),
			}),
		};
		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, left_arm_object_handle, left_arm_transform);

		// Right Arm
		OBJECT_CREATE_COMPONENT(StaticModelComponent, game_object_manager_ptr, right_arm_object_handle, arm_model_component_data);

		TransformComponent right_arm_transform = {
			.trs = {
				.scale = vec3_new(1,1,1),
				.rotation = quat_identity,
				.translation = vec3_new(-3.25,0,0),
			},
			.parent = optional_init({
				.object_handle = body_object_handle,
				.name = optional_none(),
			}),
		};
		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, right_arm_object_handle, right_arm_transform);

		StaticModel legs_static_model;
		assert(static_model_load("data/meshes/StarterMech_Legs.glb", &gpu2_device, &legs_static_model));
		StaticModelComponent legs_model_component_data = {.static_model = legs_static_model, };
		OBJECT_CREATE_COMPONENT(StaticModelComponent, game_object_manager_ptr, legs_object_handle, legs_model_component_data);

		TransformComponent legs_transform = {
			.trs = {
				.scale = vec3_new(1,1,1),
				.rotation = quat_identity,
				.translation = vec3_new(0,-4.5,0),
			},
			.parent = optional_init({
				.object_handle = body_object_handle,
				.ignore_rotation = true,
				.name = optional_none(),
			}),
		};
		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, legs_object_handle, legs_transform);

		TransformComponent cam_root_transform = {
			.trs = {
				.scale = vec3_new(1,1,1),
				.rotation = quat_new(vec3_new(1,0,0), -15.0f * DEGREES_TO_RADIANS),
				.translation = vec3_new(0,0,0),
			},
		};
		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, camera_root_object_handle, cam_root_transform);

		// Camera Setup
		TransformComponent cam_transform = {
			.trs = {
				.scale = vec3_new(1,1,1),
				.rotation = quat_identity,
				.translation = vec3_new(0,0,200),
			},
			.parent = optional_init({
				.object_handle = camera_root_object_handle,
				.name = optional_none(),
			}),
		};
		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, camera_object_handle, cam_transform);
		CameraComponent cam_component_data = {
			.camera_up = vec3_new(0,1,0),
			.camera_forward = vec3_new(0,0,1),
			.fov = 60.0f,
		};
		OBJECT_CREATE_COMPONENT(CameraComponent, game_object_manager_ptr, camera_object_handle, cam_component_data);
	}

	// Generate some random animated + static meshes
	const i32 OBJECTS_TO_CREATE = 500;
	for (i32 i = 0; i < OBJECTS_TO_CREATE; ++i)
	{
		GameObjectHandle new_object_handle = ADD_OBJECT(game_object_manager_ptr);

		if ((i % 2) == 0)
		{
			//Mat4* joint_matrices = calloc(animated_model.num_joints, sizeof(Mat4));
			Gpu2BufferCreateInfo joints_buffer_create_info = {
				.is_cpu_visible = true,
				.size = animated_model.joints_buffer_size,
				.data = NULL, //joint_matrices,
			};
			Gpu2Buffer joint_matrices_buffer;
			assert(gpu2_create_buffer(&gpu2_device, &joints_buffer_create_info, &joint_matrices_buffer));

			AnimatedModelComponent animated_model_component_data = {
				.animated_model = animated_model,
				.joint_matrices_buffer = joint_matrices_buffer,
				.mapped_buffer_data = gpu2_map_buffer(&gpu2_device, &joint_matrices_buffer),
				.animation_rate = rand_f32(0.01f, 10.0f),
			};

			OBJECT_CREATE_COMPONENT(AnimatedModelComponent, game_object_manager_ptr, new_object_handle, animated_model_component_data);
		}
		else
		{
			StaticModelComponent static_model_component_data = {
				.static_model = static_model,
			};

			OBJECT_CREATE_COMPONENT(StaticModelComponent, game_object_manager_ptr, new_object_handle, static_model_component_data);
		}

		const float spawn_scale = OBJECTS_TO_CREATE / 100.0f;
		const float spawn_span = OBJECTS_TO_CREATE / 2.0f;

		Vec3 translation = vec3_new(
			rand_f32(-spawn_span, spawn_span),
			rand_f32(-spawn_span, spawn_span),
			rand_f32(-spawn_span, spawn_span)
		);

		TransformComponent t = {
			.trs = {
				.scale = vec3_new(spawn_scale, spawn_scale, spawn_scale),
				.rotation = quat_new(
					vec3_normalize(
						vec3_new(
							rand_f32(-1, 1),
							rand_f32(-1, 1),
							rand_f32(-1,1)
						)
					), 
					rand_f32(-180.0f, 180.0f) * DEGREES_TO_RADIANS
				),
				.translation = translation, 
			},
			.parent = {},
		};


		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, new_object_handle, t);
	}

	// Create Per Object Bind Group Layout at set index 1
	Gpu2BindGroupLayoutCreateInfo per_object_bind_group_layout_create_info = {
		.index = 1,
		.num_bindings = 6,
		.bindings = (Gpu2ResourceBinding[6]){
			{
				.type = GPU2_BINDING_TYPE_BUFFER,
				.shader_stages = GPU2_SHADER_STAGE_VERTEX,	
			},
			{
				.type = GPU2_BINDING_TYPE_BUFFER,
				.shader_stages = GPU2_SHADER_STAGE_VERTEX,	
			},
			{
				.type = GPU2_BINDING_TYPE_BUFFER,
				.shader_stages = GPU2_SHADER_STAGE_VERTEX,	
			},
			{
				.type = GPU2_BINDING_TYPE_BUFFER,
				.shader_stages = GPU2_SHADER_STAGE_VERTEX,	
			},
			{
				.type = GPU2_BINDING_TYPE_BUFFER,
				.shader_stages = GPU2_SHADER_STAGE_VERTEX,	
			},
			{
				.type = GPU2_BINDING_TYPE_BUFFER,
				.shader_stages = GPU2_SHADER_STAGE_VERTEX,	
			},
		},
	};
	Gpu2BindGroupLayout per_object_bind_group_layout;
	assert(gpu2_create_bind_group_layout(&gpu2_device, &per_object_bind_group_layout_create_info, &per_object_bind_group_layout));

	// Finally set up our render data for all game objects...
	for (i64 obj_idx = 0; obj_idx < sb_count(game_object_manager.game_object_array); ++obj_idx)
	{
		GameObjectHandle object_handle = {.idx = obj_idx, };
		if (!OBJECT_IS_VALID(&game_object_manager, object_handle)) 
		{
			continue;
		}

		game_object_render_data_setup(
			&game_object_manager, 
			&gpu2_device, 
			object_handle, 
			&per_object_bind_group_layout
		);
	}

	Gpu2ShaderCreateInfo vertex_shader_create_info = {
		.filename = "bin/shaders/mesh_render.vert",
	};
	Gpu2Shader gpu2_vertex_shader;
	assert(gpu2_create_shader(&gpu2_device, &vertex_shader_create_info, &gpu2_vertex_shader));

	Gpu2ShaderCreateInfo fragment_shader_create_info = {
		.filename = "bin/shaders/mesh_render.frag",
	};
	Gpu2Shader gpu2_fragment_shader;
	assert(gpu2_create_shader(&gpu2_device, &fragment_shader_create_info, &gpu2_fragment_shader));	

	GlobalUniformStruct global_uniform_data = {
		.view = mat4_identity,
		.projection = mat4_identity,
		.eye = vec4_zero,
	};
	
	Gpu2BufferCreateInfo global_uniform_buffer_create_info = {
		.is_cpu_visible = true,
		.size = sizeof(GlobalUniformStruct),
		.data = NULL,
	};
	Gpu2Buffer global_uniform_buffer;
	assert(gpu2_create_buffer(&gpu2_device, &global_uniform_buffer_create_info, &global_uniform_buffer));
	
	// Create Bind Group Layout
	Gpu2BindGroupLayoutCreateInfo global_bind_group_layout_create_info = {
		.index = 0,
		.num_bindings = 1,
		.bindings = (Gpu2ResourceBinding[1]){
			{
				.type = GPU2_BINDING_TYPE_BUFFER,
				.shader_stages = GPU2_SHADER_STAGE_VERTEX | GPU2_SHADER_STAGE_FRAGMENT,	
			},
		},
	};
	Gpu2BindGroupLayout global_bind_group_layout;
	//FCS TODO: Need one of these per-frame
	assert(gpu2_create_bind_group_layout(&gpu2_device, &global_bind_group_layout_create_info, &global_bind_group_layout));

	// Create Global Bind Group
	Gpu2BindGroupCreateInfo global_bind_group_create_info = {
		.index = 0,
		.layout = &global_bind_group_layout,
	};
	Gpu2BindGroup global_bind_group;
	assert(gpu2_create_bind_group(&gpu2_device, &global_bind_group_create_info, &global_bind_group));

	//  Write updates to our global bind group
	const Gpu2BindGroupUpdateInfo global_bind_group_update_info = {
		.bind_group = &global_bind_group,
		.num_writes = 1,
		.writes = (Gpu2ResourceWrite[1]){
			{
				.binding_index = 0,
				.type = GPU2_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &global_uniform_buffer,
				},
			},
		},
	};
	gpu2_update_bind_group(&gpu2_device, &global_bind_group_update_info);

	Gpu2BindGroupLayout* pipeline_bind_group_layouts[] = { &global_bind_group_layout, &per_object_bind_group_layout };

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
	assert(gpu2_create_texture(&gpu2_device, &depth_texture_create_info, &depth_texture));

	const u32 swapchain_count = gpu2_get_swapchain_count(&gpu2_device);
	u64 time = time_now();
    u32 current_frame = 0;

    i32 mouse_x, mouse_y;
	window_get_mouse_pos(&window, &mouse_x, &mouse_y);

	bool show_mouse = false;
	window_show_mouse_cursor(&window, show_mouse);

	while (window_handle_messages(&window))
	{
		if (window_input_pressed(&window, KEY_ESCAPE))
		{
			break;
		}
		
        const u64 new_time = time_now();
        const double delta_time = time_seconds(new_time - time);
        time = new_time;

		i32 old_mouse_x = mouse_x;
		i32 old_mouse_y = mouse_y;
        window_get_mouse_pos(&window, &mouse_x, &mouse_y);

		Vec2 mouse_pos = vec2_new(mouse_x, mouse_y);
		Vec2 old_mouse_pos = vec2_new(old_mouse_x, old_mouse_y);
		i32 mouse_delta_x, mouse_delta_y;
		window_get_mouse_delta(&window, &mouse_delta_x, &mouse_delta_y);
		Vec2 mouse_delta = {
			.x = mouse_delta_x,
			.y = mouse_delta_y,
		};

		if (!show_mouse)
		{
			{   // Player Movement
				CameraComponent* cam_component = OBJECT_GET_COMPONENT(CameraComponent, &game_object_manager, camera_object_handle);
				assert(cam_component);
				TransformComponent* root_transform = OBJECT_GET_COMPONENT(TransformComponent, &game_object_manager, root_object_handle);
				PlayerControlComponent* player_control = OBJECT_GET_COMPONENT(PlayerControlComponent, &game_object_manager, root_object_handle);
				assert(root_transform && player_control);
				const Vec3 old_translation = root_transform->trs.translation; 
				Vec3 up_vec = vec3_new(0,1,0);
				Vec3 forward_vec = vec3_normalize(vec3_plane_projection(cam_component->camera_forward, vec3_new(0,1,0)));
				Vec3 right_vec = vec3_cross(up_vec, forward_vec);

				Vec3 move_vec = vec3_zero;
				if (window_input_pressed(&window, 'W'))
				{
					move_vec = vec3_add(move_vec, vec3_scale(forward_vec, player_control->move_speed ));
				}
				if (window_input_pressed(&window, 'S'))
				{
					move_vec = vec3_add(move_vec, vec3_scale(forward_vec, -player_control->move_speed ));
				}
				if (window_input_pressed(&window, 'A'))
				{
					move_vec = vec3_add(move_vec, vec3_scale(right_vec, player_control->move_speed ));
				}
				if (window_input_pressed(&window, 'D'))
				{
					move_vec = vec3_add(move_vec, vec3_scale(right_vec, -player_control->move_speed ));
				}
				if (window_input_pressed(&window, 'E') || window_input_pressed(&window, KEY_SPACE))
				{
					move_vec = vec3_add(move_vec, vec3_scale(up_vec, player_control->move_speed ));
				}
				if (window_input_pressed(&window, 'Q'))
				{
					move_vec = vec3_add(move_vec, vec3_scale(up_vec, -player_control->move_speed ));
				}

				if (window_input_pressed(&window, KEY_SHIFT))
				{
					move_vec = vec3_scale(move_vec, 2.5f);
				}

				// Finally scale everything by delta time
				move_vec = vec3_scale(move_vec, delta_time);

				{
					const float body_rot_lerp_speed = 10.0 * delta_time;

					root_transform->trs.translation = vec3_add(old_translation, move_vec);
					Quat old_body_rotation = root_transform->trs.rotation;
					Quat new_body_rotation = mat4_to_quat(mat4_from_axes(forward_vec, up_vec));
					root_transform->trs.rotation = quat_slerp(body_rot_lerp_speed, old_body_rotation, new_body_rotation);
				}

				TransformComponent* cam_root_transform = OBJECT_GET_COMPONENT(TransformComponent, &game_object_manager, camera_root_object_handle);
				assert(cam_root_transform);
				//FCS TODO: Lerp for lazy cam
				cam_root_transform->trs.translation = root_transform->trs.translation;

				TransformComponent* legs_transform = OBJECT_GET_COMPONENT(TransformComponent, &game_object_manager, legs_object_handle);
				assert(legs_transform);


				if (!vec3_nearly_equal(vec3_zero, move_vec)) 
				{
					const Vec3 legs_up = vec3_new(0,1,0);
					const f32 legs_up_dot_move_vec = fabsf(vec3_dot(legs_up, vec3_normalize(move_vec)));
					if (!f32_nearly_equal(legs_up_dot_move_vec, 1.0f))
					{
						const float legs_rot_lerp_speed = 10.0 * delta_time;
						const Quat old_rotation = legs_transform->trs.rotation;
						const Quat desired_rotation = mat4_to_quat(mat4_from_axes(vec3_normalize(move_vec), legs_up));
						legs_transform->trs.rotation = quat_slerp(legs_rot_lerp_speed, old_rotation, desired_rotation);
					}
				}
			}

			{	// Camera Control
				// currently assumes cam root has no parent (we manually move it into place)
				TransformComponent* cam_root_transform = OBJECT_GET_COMPONENT(TransformComponent, &game_object_manager, camera_root_object_handle);
				assert(cam_root_transform);

				const float cam_rotation_speed = 1.0f;

				if (!f32_nearly_zero(mouse_delta.x))
				{
					Quat rotation_quat = quat_new(vec3_new(0,1,0), -mouse_delta.x * cam_rotation_speed * delta_time);	
					cam_root_transform->trs.rotation = quat_mul(rotation_quat, cam_root_transform->trs.rotation);
				}

				if (!f32_nearly_zero(mouse_delta.y))
				{
					//FCS TODO: need to get current xform right vector
					const Vec3 root_right = quat_rotate_vec3(cam_root_transform->trs.rotation, vec3_new(1, 0, 0));
					Quat rotation_quat = quat_new(root_right, -mouse_delta.y * cam_rotation_speed * delta_time);	
					cam_root_transform->trs.rotation = quat_mul(rotation_quat, cam_root_transform->trs.rotation);
				}
			}
		}

		{	// Update Global Uniforms...	
			Mat4 root_transform = trs_to_mat4(game_object_compute_global_transform(&game_object_manager, root_object_handle));
			Vec3 root_position = mat4_get_translation(root_transform);
			Mat4 cam_transform = trs_to_mat4(game_object_compute_global_transform(&game_object_manager, camera_object_handle));
			Vec3 cam_position = mat4_get_translation(cam_transform);

			CameraComponent* cam_component = OBJECT_GET_COMPONENT(CameraComponent, &game_object_manager, camera_object_handle);
			assert(cam_component);
			cam_component->camera_forward = vec3_normalize(vec3_sub(root_position, cam_position));
			const Vec3 cam_target = vec3_add(cam_position, cam_component->camera_forward);
			Mat4 view = mat4_look_at(cam_position, cam_target, cam_component->camera_up);
			Mat4 proj = mat4_perspective(cam_component->fov, (float)window_width / (float)window_height, 0.01f, 4000.0f);

			global_uniform_data.view = view;
			global_uniform_data.projection = proj;

			memcpy(&global_uniform_data.eye, &cam_position, sizeof(float) * 3);

			Gpu2BufferWriteInfo global_uniform_buffer_write_info = {
				.size = sizeof(GlobalUniformStruct),
				.data = &global_uniform_data,
			};
			gpu2_write_buffer(&gpu2_device, &global_uniform_buffer, &global_uniform_buffer_write_info);
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
			gpu2_render_pass_set_bind_group(&render_pass, &gpu2_render_pipeline, &global_bind_group);

			for (i64 obj_idx = 0; obj_idx < sb_count(game_object_manager.game_object_array); ++obj_idx)
			{
				GameObjectHandle object_handle = {.idx = obj_idx, };
				if (!OBJECT_IS_VALID(&game_object_manager, object_handle)) 
				{
					continue;
				}

				TransformComponent* transform_component = OBJECT_GET_COMPONENT(TransformComponent, &game_object_manager, object_handle);
				ObjectRenderDataComponent* render_data_component = OBJECT_GET_COMPONENT(ObjectRenderDataComponent, &game_object_manager, object_handle);
				if (transform_component && render_data_component)
				{
					StaticModelComponent* static_model_component = OBJECT_GET_COMPONENT(StaticModelComponent, &game_object_manager, object_handle);
					AnimatedModelComponent* animated_model_component = OBJECT_GET_COMPONENT(AnimatedModelComponent, &game_object_manager, object_handle);

					// Need one of these to draw a mesh
					if (!static_model_component && !animated_model_component)
					{
						continue;
					}

					//Assign to our persistently mapped storage
					render_data_component->uniform_data[current_frame]->model = trs_to_mat4(game_object_compute_global_transform(&game_object_manager, object_handle)); 
					gpu2_render_pass_set_bind_group(&render_pass, &gpu2_render_pipeline, &render_data_component->bind_groups[current_frame]);	

					if (static_model_component)
					{
						gpu2_render_pass_draw(&render_pass, 0, static_model_component->static_model.num_indices);
					}
					else if (animated_model_component)
					{
						animated_model_component->current_anim_time += (delta_time * animated_model_component->animation_rate);
						if (animated_model_component->current_anim_time > animated_model.baked_animation.end_time)
						{ 
							animated_model_component->current_anim_time = animated_model.baked_animation.start_time;
						}
						if (animated_model_component->current_anim_time < animated_model.baked_animation.start_time)
						{ 
							animated_model_component->current_anim_time = animated_model.baked_animation.end_time;
						}
						animated_model_update_animation(
							&animated_model, 
							animated_model_component->current_anim_time,
							animated_model_component->mapped_buffer_data
						);

						gpu2_render_pass_draw(&render_pass, 0, animated_model_component->animated_model.num_indices);
					}
				}
			}
		}

		gpu2_end_render_pass(&render_pass);
		gpu2_present_drawable(&gpu2_device, &command_buffer, &drawable);
		assert(gpu2_commit_command_buffer(&gpu2_device, &command_buffer));

		current_frame = (current_frame + 1) % swapchain_count;
	}

	// Cleanup
	static_model_free(&gpu2_device, &static_model);
	gpu2_destroy_buffer(&gpu2_device, &global_uniform_buffer);

	gpu2_destroy_texture(&gpu2_device, &depth_texture);

	gpu2_destroy_bind_group(&gpu2_device, &global_bind_group);
	gpu2_destroy_bind_group_layout(&gpu2_device, &global_bind_group_layout);

	gpu2_destroy_device(&gpu2_device);

	return 0;
}

