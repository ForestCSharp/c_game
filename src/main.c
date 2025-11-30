
#include <stdio.h>
#include <stdlib.h>

#include "app/app.h"
#include "threading/threading.h"
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
#include "game_object/game_object.h"
#include "uniforms.h"
#include "gui.h"
#include "debug_draw.h"
#include "task/task.h"

typedef struct AnimationUpdateTaskData
{
	AnimatedModel* animated_model;
	float delta_time;
	float global_animation_rate;
	sbuffer(AnimatedModelComponent*) components_to_update;
} AnimationUpdateTaskData;

void animation_update_task(void* in_arg)
{
	AnimationUpdateTaskData* task_data = (AnimationUpdateTaskData*) in_arg;
	AnimatedModel* animated_model = task_data->animated_model;
	float delta_time = task_data->delta_time;
	float global_animation_rate = task_data->global_animation_rate;

	for (i32 component_idx = 0; component_idx < sb_count(task_data->components_to_update); ++component_idx)
	{
		AnimatedModelComponent* animated_model_component = task_data->components_to_update[component_idx];	
		if (animated_model_component)
		{
			animated_model_component->current_anim_time += (delta_time * animated_model_component->animation_rate * global_animation_rate);
			if (animated_model_component->current_anim_time > animated_model->baked_animation.end_time)
			{ 
				animated_model_component->current_anim_time = animated_model->baked_animation.start_time;
			}
			if (animated_model_component->current_anim_time < animated_model->baked_animation.start_time)
			{ 
				animated_model_component->current_anim_time = animated_model->baked_animation.end_time;
			}
			animated_model_update_animation(
				animated_model, 
				animated_model_component->current_anim_time,
				animated_model_component->mapped_buffer_data
			);
		}
	}

	sb_free(task_data->components_to_update);
	free(task_data);
}

typedef struct Character
{
	GameObjectHandle root_object_handle;
	GameObjectHandle body_object_handle;
	GameObjectHandle head_object_handle;
	GameObjectHandle legs_object_handle;	
	GameObjectHandle left_arm_object_handle;
	GameObjectHandle right_arm_object_handle;
	GameObjectHandle camera_root_object_handle;
	GameObjectHandle camera_object_handle;

} Character;

void character_create(GameObjectManager* game_object_manager_ptr, GpuDevice* in_gpu_device, Character* out_character)
{
	assert(out_character != NULL);

	GameObjectHandle root_object_handle			= ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle body_object_handle			= ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle head_object_handle			= ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle legs_object_handle			= ADD_OBJECT(game_object_manager_ptr);	
	GameObjectHandle left_arm_object_handle		= ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle right_arm_object_handle	= ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle camera_root_object_handle	= ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle camera_object_handle		= ADD_OBJECT(game_object_manager_ptr);
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
		assert(static_model_load("data/meshes/StarterMech_Body.glb", in_gpu_device, &body_static_model));
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
		assert(static_model_load("data/meshes/StarterMech_Head.glb", in_gpu_device, &head_static_model));
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
		assert(static_model_load("data/meshes/StarterMech_Arm.glb", in_gpu_device, &arm_static_model));
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
		assert(static_model_load("data/meshes/StarterMech_Legs.glb", in_gpu_device, &legs_static_model));
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

	//FCS TODO: Leaking the StaticModels by not adding them to this struct and cleaning them up later
	*out_character = (Character) {
		.root_object_handle	= root_object_handle,
		.body_object_handle	= body_object_handle,
		.head_object_handle = head_object_handle,
		.legs_object_handle = legs_object_handle,
		.left_arm_object_handle	= left_arm_object_handle,
		.right_arm_object_handle = right_arm_object_handle,
		.camera_root_object_handle = camera_root_object_handle,
		.camera_object_handle =	camera_object_handle,
	};
}

typedef struct CharacterUpdateData
{
	Character* character;

	Window* window;
	GameObjectManager* game_object_manager;

	float delta_time;
	Vec2 mouse_delta;

} CharacterUpdateData;

void character_update(CharacterUpdateData* in_update_data)
{
	Character* character = in_update_data->character;
	Window* window = in_update_data->window;
	GameObjectManager* game_object_manager = in_update_data->game_object_manager;
	float delta_time = in_update_data->delta_time;
	Vec2 mouse_delta = in_update_data->mouse_delta;

	{   // Player Movement
		CameraComponent* cam_component = OBJECT_GET_COMPONENT(CameraComponent, game_object_manager, character->camera_object_handle);
		assert(cam_component);
		TransformComponent* root_transform = OBJECT_GET_COMPONENT(TransformComponent, game_object_manager, character->root_object_handle);
		PlayerControlComponent* player_control = OBJECT_GET_COMPONENT(PlayerControlComponent, game_object_manager, character->root_object_handle);
		assert(root_transform && player_control);
		const Vec3 old_translation = root_transform->trs.translation; 
		Vec3 up_vec = vec3_new(0,1,0);
		Vec3 forward_vec = vec3_normalize(vec3_plane_projection(cam_component->camera_forward, vec3_new(0,1,0)));
		Vec3 right_vec = vec3_cross(up_vec, forward_vec);

		Vec3 move_vec = vec3_zero;
		if (window_input_pressed(window, 'W'))
		{
			move_vec = vec3_add(move_vec, vec3_scale(forward_vec, player_control->move_speed ));
		}
		if (window_input_pressed(window, 'S'))
		{
			move_vec = vec3_add(move_vec, vec3_scale(forward_vec, -player_control->move_speed ));
		}
		if (window_input_pressed(window, 'A'))
		{
			move_vec = vec3_add(move_vec, vec3_scale(right_vec, player_control->move_speed ));
		}
		if (window_input_pressed(window, 'D'))
		{
			move_vec = vec3_add(move_vec, vec3_scale(right_vec, -player_control->move_speed ));
		}
		if (window_input_pressed(window, 'E') || window_input_pressed(window, KEY_SPACE))
		{
			move_vec = vec3_add(move_vec, vec3_scale(up_vec, player_control->move_speed ));
		}
		if (window_input_pressed(window, 'Q'))
		{
			move_vec = vec3_add(move_vec, vec3_scale(up_vec, -player_control->move_speed ));
		}

		if (window_input_pressed(window, KEY_SHIFT))
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

		TransformComponent* cam_root_transform = OBJECT_GET_COMPONENT(TransformComponent, game_object_manager, character->camera_root_object_handle);
		assert(cam_root_transform);

		//FCS TODO: Lerp for lazy cam
		cam_root_transform->trs.translation = root_transform->trs.translation;

		TransformComponent* legs_transform = OBJECT_GET_COMPONENT(TransformComponent, game_object_manager, character->legs_object_handle);
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
		TransformComponent* cam_root_transform = OBJECT_GET_COMPONENT(TransformComponent, game_object_manager, character->camera_root_object_handle);
		assert(cam_root_transform);

		const float cam_rotation_speed = 1.0f;

		if (!f32_nearly_zero(mouse_delta.x))
		{
			Quat rotation_quat = quat_new(vec3_new(0,1,0), -mouse_delta.x * cam_rotation_speed * delta_time);	
			cam_root_transform->trs.rotation = quat_mul(rotation_quat, cam_root_transform->trs.rotation);
		}

		if (!f32_nearly_zero(mouse_delta.y))
		{
			const Vec3 root_right = quat_rotate_vec3(cam_root_transform->trs.rotation, vec3_new(1, 0, 0));
			Quat rotation_quat = quat_new(root_right, -mouse_delta.y * cam_rotation_speed * delta_time);	
			cam_root_transform->trs.rotation = quat_mul(rotation_quat, cam_root_transform->trs.rotation);
		}
	}
}

// FCS TODO:
// need to n-buffer resources for frames in flight... (joint matrices still left to do)

int main()
{
	// Init multithreaded task system
	TaskSystem task_system;
	task_system_init(&task_system);

	// Create our window	
	String window_title = string_new("C Game (");
	string_append(&window_title, gpu_get_api_name());
	string_append(&window_title, ")");

	i32 window_width = 1280;
	i32 window_height = 720;
    Window window = window_create(window_title.data, window_width, window_height);

	string_free(&window_title);

	GpuDevice gpu_device;
	gpu_create_device(&window, &gpu_device);

	const u32 swapchain_count = gpu_get_swapchain_count(&gpu_device);

	//GameObject + Components Setup
	GameObjectManager game_object_manager = {};
	GameObjectManager* game_object_manager_ptr = &game_object_manager;
	game_object_manager_init(game_object_manager_ptr);

	Character character;
	character_create(game_object_manager_ptr, &gpu_device, &character);

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
	//if (!animated_model_load("data/meshes/blender_simple.glb", &gpu_device, &animated_model))
	//if (!animated_model_load("data/meshes/running.glb", &gpu_device, &animated_model))
	{
		printf("Failed to Load Animated Model\n");
		return 1;
	}


	// Generate some random animated + static meshes
	const i32 OBJECTS_TO_CREATE = 1000;
	for (i32 i = 0; i < OBJECTS_TO_CREATE; ++i)
	{
		GameObjectHandle new_object_handle = ADD_OBJECT(game_object_manager_ptr);

		const bool create_animated_model = true; //(i % 2) != 0;
		if (create_animated_model)
		{
			GpuBufferCreateInfo joints_buffer_create_info = {
				.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
				.is_cpu_visible = true,
				.size = animated_model.joints_buffer_size,
				.data = NULL,
			};
			GpuBuffer joint_matrices_buffer;
			gpu_create_buffer(&gpu_device, &joints_buffer_create_info, &joint_matrices_buffer);

			AnimatedModelComponent animated_model_component_data = {
				.animated_model = animated_model,
				.joint_matrices_buffer = joint_matrices_buffer,
				.mapped_buffer_data = gpu_map_buffer(&gpu_device, &joint_matrices_buffer),
				.animation_rate = rand_f32(0.0001f, 5.0f),
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

		const float spawn_scale = create_animated_model ? OBJECTS_TO_CREATE / 25.0f : OBJECTS_TO_CREATE / 100.0f;
		const Vec3 scale = vec3_new(spawn_scale, spawn_scale, spawn_scale);
		
		const float spawn_span = OBJECTS_TO_CREATE / 2.0f;
		Vec3 translation = vec3_new(
			rand_f32(-spawn_span, spawn_span),
			rand_f32(-spawn_span, spawn_span),
			rand_f32(-spawn_span, spawn_span)
		);

		const Quat rotation	=	create_animated_model 
							?	quat_identity
							:	quat_new(
									vec3_normalize(vec3_new(
										rand_f32(-1, 1),
										rand_f32(-1, 1),
										rand_f32(-1, 1)
									)), 
									rand_f32(-180.0f, 180.0f) * DEGREES_TO_RADIANS
								);

		TransformComponent t = {
			.trs = {
				.scale = scale,
				.rotation = rotation,
				.translation = translation, 
			},
			.parent = {},
		};


		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, new_object_handle, t);
	}

	// Create Per Object Bind Group Layout at set index 1
	GpuBindGroupLayoutCreateInfo per_object_bind_group_layout_create_info = {
		.index = 1,
		.num_bindings = 6,
		.bindings = (GpuResourceBinding[6]){
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
	GpuBindGroupLayout per_object_bind_group_layout;
	gpu_create_bind_group_layout(&gpu_device, &per_object_bind_group_layout_create_info, &per_object_bind_group_layout);

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
			&gpu_device, 
			object_handle, 
			&per_object_bind_group_layout
		);
	}

	GpuShaderCreateInfo vertex_shader_create_info = {
		.filename = "bin/shaders/mesh_render.vert",
	};
	GpuShader geometry_vertex_shader;
	gpu_create_shader(&gpu_device, &vertex_shader_create_info, &geometry_vertex_shader);

	GpuShaderCreateInfo fragment_shader_create_info = {
		.filename = "bin/shaders/mesh_render.frag",
	};
	GpuShader geometry_fragment_shader;
	gpu_create_shader(&gpu_device, &fragment_shader_create_info, &geometry_fragment_shader);

	GlobalUniformStruct global_uniform_data = {
		.view = mat4_identity,
		.projection = mat4_identity,
		.eye = vec4_zero,
	};
	
	GpuBufferCreateInfo global_uniform_buffer_create_info = {
		.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = sizeof(GlobalUniformStruct),
		.data = NULL,
	};

	// Create Bind Group Layout
	GpuBindGroupLayoutCreateInfo global_bind_group_layout_create_info = {
		.index = 0,
		.num_bindings = 1,
		.bindings = (GpuResourceBinding[1]){
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX | GPU_SHADER_STAGE_FRAGMENT,	
			},
		},
	};
	GpuBindGroupLayout global_bind_group_layout;
	gpu_create_bind_group_layout(&gpu_device, &global_bind_group_layout_create_info, &global_bind_group_layout);

	// Create Global Bind Group
	GpuBindGroupCreateInfo global_bind_group_create_info = {
		.layout = &global_bind_group_layout,
	};

	GpuBuffer global_uniform_buffers[swapchain_count];
	GpuBindGroup global_bind_groups[swapchain_count];
	for (i32 global_uniform_idx = 0; global_uniform_idx < swapchain_count; ++global_uniform_idx)
	{
		gpu_create_buffer(&gpu_device, &global_uniform_buffer_create_info, &global_uniform_buffers[global_uniform_idx]);
		gpu_create_bind_group(&gpu_device, &global_bind_group_create_info, &global_bind_groups[global_uniform_idx]);

		//  Write updates to our global bind group
		const GpuBindGroupUpdateInfo global_bind_group_update_info = {
			.bind_group = &global_bind_groups[global_uniform_idx],
			.num_writes = 1,
			.writes = (GpuResourceWrite[1]){
				{
					.binding_index = 0,
					.type = GPU_BINDING_TYPE_BUFFER,
					.buffer_binding = {
						.buffer = &global_uniform_buffers[global_uniform_idx],
					},
				},
			},
		};
		gpu_update_bind_group(&gpu_device, &global_bind_group_update_info);
	}

	GpuBindGroupLayout* geometry_pipeline_bind_group_layouts[] = { &global_bind_group_layout, &per_object_bind_group_layout };

	GpuRenderPipelineCreateInfo geometry_render_pipeline_create_info = {
		.vertex_shader = &geometry_vertex_shader,
		.fragment_shader = &geometry_fragment_shader,
		.num_bind_group_layouts = ARRAY_COUNT(geometry_pipeline_bind_group_layouts),
		.bind_group_layouts = geometry_pipeline_bind_group_layouts,
		.depth_test_enabled = true,
	};
	GpuRenderPipeline geometry_render_pipeline;
	gpu_create_render_pipeline(&gpu_device, &geometry_render_pipeline_create_info, &geometry_render_pipeline);

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

	// FCS TODO: move gui.h into gui/ add gui_render.h to that directory for all gui-rendering code
    // BEGIN GUI SETUP
    GuiContext gui_context;
    gui_init(&gui_context);

    assert(gui_context.default_font.font_type == FONT_TYPE_RGBA);

	// Gui Vertex Buffer
    size_t max_gui_vertices_size = sizeof(GuiVert) * 10000;
	GpuBufferCreateInfo gui_vertex_buffer_create_info = {
		.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = max_gui_vertices_size,
		.data = NULL,
	};
	GpuBuffer gui_vertex_buffer;
	gpu_create_buffer(&gpu_device, &gui_vertex_buffer_create_info, &gui_vertex_buffer);

	// Gui Index Buffer
    size_t max_gui_indices_size = sizeof(u32) * 30000;
	GpuBufferCreateInfo gui_index_buffer_create_info = {
		.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = max_gui_indices_size,
		.data = NULL,
	};
	GpuBuffer gui_index_buffer;
	gpu_create_buffer(&gpu_device, &gui_index_buffer_create_info, &gui_index_buffer);

    // Create Font Texture
	GpuTextureCreateInfo font_texture_create_info = {
		.format = GPU_FORMAT_RGBA8_UNORM,	
		.extent = {
			.width = gui_context.default_font.image_width,
			.height = gui_context.default_font.image_height,
			.depth = 1,
		},
		.usage = GPU_TEXTURE_USAGE_SAMPLED,
		.is_cpu_visible = false,
	};
	GpuTexture font_texture;
	gpu_create_texture(&gpu_device, &font_texture_create_info, &font_texture);

	GpuTextureWriteInfo font_texture_write_info = {
		.texture = &font_texture,
		.width = gui_context.default_font.image_width,
		.height = gui_context.default_font.image_height,
		.data = gui_context.default_font.image_data,
	};
    gpu_write_texture(&gpu_device, &font_texture_write_info);

	GpuSamplerCreateInfo font_sampler_create_info = {
		.filters = {
			.min = GPU_FILTER_LINEAR,
			.mag = GPU_FILTER_LINEAR,
		},
		.address_modes = {
			.u = GPU_SAMPLER_ADDRESS_MODE_REPEAT,
			.v = GPU_SAMPLER_ADDRESS_MODE_REPEAT,
			.w = GPU_SAMPLER_ADDRESS_MODE_REPEAT,
		},
		.max_anisotropy = 16,
	};
	GpuSampler font_sampler;
	gpu_create_sampler(&gpu_device, &font_sampler_create_info, &font_sampler);

	// Create GUI Bind Group Layout, Bind Group, and Update it
	GpuBindGroupLayoutCreateInfo gui_bind_group_layout_create_info = {
		.index = 0,
		.num_bindings = 4,
		.bindings = (GpuResourceBinding[4]){
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX ,	
			},
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX ,	
			},		
			{
				.type = GPU_BINDING_TYPE_TEXTURE,
				.shader_stages = GPU_SHADER_STAGE_FRAGMENT,	
			},
			{
				.type = GPU_BINDING_TYPE_SAMPLER,
				.shader_stages = GPU_SHADER_STAGE_FRAGMENT,	
			},
		},
	};
	GpuBindGroupLayout gui_bind_group_layout;
	gpu_create_bind_group_layout(&gpu_device, &gui_bind_group_layout_create_info, &gui_bind_group_layout);

	GpuBindGroupCreateInfo gui_bind_group_create_info = {
		.layout = &gui_bind_group_layout,
	};
	GpuBindGroup gui_bind_group;
	gpu_create_bind_group(&gpu_device, &gui_bind_group_create_info, &gui_bind_group);

	const GpuBindGroupUpdateInfo gui_bind_group_update_info = {
		.bind_group = &gui_bind_group,
		.num_writes = 4,
		.writes = (GpuResourceWrite[4]){
			{
				.binding_index = 0,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &gui_index_buffer,
				},
			},
			{
				.binding_index = 1,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &gui_vertex_buffer,
				},
			},
			{
				.binding_index = 2,
				.type = GPU_BINDING_TYPE_TEXTURE,
				.texture_binding = {
					.texture = &font_texture,
				},
			},
			{
				.binding_index = 3,
				.type = GPU_BINDING_TYPE_SAMPLER,
				.sampler_binding = {
					.sampler = &font_sampler,
				},
			},
		},
	};
	gpu_update_bind_group(&gpu_device, &gui_bind_group_update_info);

	GpuShaderCreateInfo gui_vertex_shader_create_info = {
			.filename = "bin/shaders/gui.vert",
		};
	GpuShader gui_vertex_shader;
	gpu_create_shader(&gpu_device, &gui_vertex_shader_create_info, &gui_vertex_shader);

	GpuShaderCreateInfo gui_fragment_shader_create_info = {
			.filename = "bin/shaders/gui.frag",
		};
	GpuShader gui_fragment_shader;
	gpu_create_shader(&gpu_device, &gui_fragment_shader_create_info, &gui_fragment_shader);

	GpuBindGroupLayout* gui_pipeline_bind_group_layouts[] = { &gui_bind_group_layout };
	GpuRenderPipelineCreateInfo gui_render_pipeline_create_info = {
		.vertex_shader = &gui_vertex_shader,
		.fragment_shader = &gui_fragment_shader,
		.num_bind_group_layouts = ARRAY_COUNT(gui_pipeline_bind_group_layouts),
		.bind_group_layouts = gui_pipeline_bind_group_layouts,
		.depth_test_enabled = false,
	};
	GpuRenderPipeline gui_render_pipeline;
	gpu_create_render_pipeline(&gpu_device, &gui_render_pipeline_create_info, &gui_render_pipeline);

	gpu_destroy_shader(&gpu_device, &gui_vertex_shader);
	gpu_destroy_shader(&gpu_device, &gui_fragment_shader);

	// END GUI SETUP

	// BEGIN DEBUG DRAW SETUP
	DebugDrawContext debug_draw_context;
	debug_draw_init(&gpu_device, &debug_draw_context);
	// END DEBUG DRAW SETUP

	u64 time = time_now();
    u32 current_frame = 0;

    double accumulated_delta_time = 0.0f;
    size_t frames_rendered = 0;

    i32 mouse_x, mouse_y;
	window_get_mouse_pos(&window, &mouse_x, &mouse_y);

	bool show_mouse = false;
	window_show_mouse_cursor(&window, show_mouse);

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

        accumulated_delta_time += delta_time;
        frames_rendered++;	

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

		// Mouse Lock
		static bool show_mouse_pressed_last_frame = false;
		const bool show_debug_ui_pressed = window_input_pressed(&window, 'I')
										&& window_input_pressed(&window, KEY_LEFT_CONTROL);

		if (show_debug_ui_pressed && !show_mouse_pressed_last_frame)
		{
			show_mouse = !show_mouse;
			window_show_mouse_cursor(&window, show_mouse);
			show_mouse_pressed_last_frame = true;
		}
		else if (!show_debug_ui_pressed)
		{
			show_mouse_pressed_last_frame = false;
		}

        // BEGIN Gui
		static float global_animation_rate = 1.0f;

		GuiFrameState gui_frame_state = {
			.screen_size = vec2_new(window_width, window_height),
			.mouse_pos = mouse_pos,
			.mouse_buttons = {
				window_input_pressed(&window, KEY_LEFT_MOUSE),
				window_input_pressed(&window, KEY_RIGHT_MOUSE),
				window_input_pressed(&window, KEY_MIDDLE_MOUSE)
			},
		};
		gui_begin_frame(&gui_context, gui_frame_state);
		debug_draw_begin_frame(&debug_draw_context, current_frame);

		if (!show_mouse) 
		{
			int x,y;
			window_get_position(&window, &x, &y);

			int width, height;
			window_get_dimensions(&window, &width, &height);

			window_set_mouse_pos(&window, x + width/2, y + height/2);
		}

		if (!show_mouse)
		{
			character_update(&(CharacterUpdateData) {
				.character = &character,
				.window = &window,
				.game_object_manager = &game_object_manager,
				.delta_time = delta_time,
				.mouse_delta = mouse_delta,
			});
		}

		// Animation Update
		sbuffer(Task*) animation_tasks = NULL;

		//FCS TODO: Way of counting components by type in game_object.h, so then num_updates_per_task could equal total_components / num_task_threads
		//const i32 num_task_threads = task_system_num_threads(&task_system);
		const i32 num_updates_per_task = 50;
		AnimationUpdateTaskData* current_task_data = NULL;

		u64 anim_update_start_time = time_now();

		for (i64 obj_idx = 0; obj_idx < sb_count(game_object_manager.game_object_array); ++obj_idx)
		{
			GameObjectHandle object_handle = {.idx = obj_idx, };
			if (!OBJECT_IS_VALID(&game_object_manager, object_handle)) 
			{
				continue;
			}

			AnimatedModelComponent* animated_model_component = OBJECT_GET_COMPONENT(AnimatedModelComponent, &game_object_manager, object_handle);
			if (!animated_model_component)
			{
				continue;
			}

			if (!current_task_data)
			{
				current_task_data = calloc(1, sizeof(AnimationUpdateTaskData));
				*current_task_data = (AnimationUpdateTaskData) {
					.animated_model = &animated_model,
					.delta_time = delta_time,
					.global_animation_rate = global_animation_rate,
				};
			}

			sb_push(current_task_data->components_to_update, animated_model_component);

			if (sb_count(current_task_data->components_to_update) > num_updates_per_task)
			{
				const bool multithread = true;
				if (multithread)
				{
					TaskDesc animation_task_desc = {
						.task_function = animation_update_task,
						.argument = current_task_data,
					};
					Task* new_animation_task = task_system_add_task(&task_system, &animation_task_desc);
					sb_push(animation_tasks, new_animation_task);
				}
				else
				{
					animation_update_task(current_task_data);
				}

				current_task_data = NULL;
			}	
		}

		// Need to wait on animation update
		task_system_wait_tasks(&task_system, animation_tasks);

		u64 anim_update_end_time = time_now();
		const double anim_update_time_ms = time_seconds(anim_update_end_time - anim_update_start_time) * 1000;

		// GUI
		if (show_mouse)
		{
			if (gui_button(&gui_context, "My Btn", vec2_new(15, 50), vec2_new(270, 50)) == GUI_CLICKED)
			{
				printf("Clicked First Button\n");
			}

			if (gui_button(&gui_context, "too much text to display", vec2_new(15, 100), vec2_new(270, 50)) == GUI_HELD)
			{
				printf("Holding Second Button\n");
			}

			if (gui_button(&gui_context, "this is way too much text to display", vec2_new(15, 150), vec2_new(270, 50)) ==
				GUI_HOVERED)
			{
				printf("Hovering Third Button\n");
			}

			static float f = 0.25f;
			gui_slider_float(&gui_context, &f, vec2_new(0, 2), "My Slider", vec2_new(15, 0), vec2_new(270, 50));

			{ // Rolling FPS, resets on click
				double average_delta_time = accumulated_delta_time / (double)frames_rendered;
				float average_fps = 1.0 / average_delta_time;

				char buffer[128];
				snprintf(buffer, sizeof(buffer), "FPS: %.1f", round(average_fps));
				const float horizontal_padding = 5.f;
				const float vertical_padding = 5.f;
				const float fps_button_size = 155.f;
				if (
					gui_button(
						&gui_context,
						buffer, 
						vec2_new(window_width - fps_button_size - horizontal_padding, vertical_padding),
						vec2_new(fps_button_size, 30)) == GUI_CLICKED
					)
				{
					accumulated_delta_time = 0.0f;
					frames_rendered = 0;
				}
			}

			{ // Print Anim Update Time on Screen
				char buffer[256];
				snprintf(buffer, sizeof(buffer), "Anim Update Time: %.6f ms", anim_update_time_ms);
				const float horizontal_padding = 5.f;
				const float vertical_padding = 40.f;
				const float button_size = 360.f;
				gui_button(
					&gui_context, 
					buffer, 
					vec2_new(window_width - button_size - horizontal_padding, vertical_padding), 
					vec2_new(button_size, 30)
				);
			}

			static GuiWindow gui_window_1 = {
				.name = "Window 1",
				.window_rect =
				{
					.position =
					{
						.x = 15,
						.y = 250,
					},
					.size =
					{
						.x = 400,
						.y = 400,
					},
					.z_order = 1,
				},
				.is_expanded = true,
				.is_open = true,
				.is_resizable = true,
				.is_movable = true,
			};

			static GuiWindow gui_window_2 = {
				.name = "Window 2",
				.window_rect =
				{
					.position =
					{
						.x = 430,
						.y = 250,
					},
					.size =
					{
						.x = 400,
						.y = 400,
					},
					.z_order = 2,
				},
				.is_expanded = true,
				.is_open = false,
				.is_resizable = true,
				.is_movable = true,
			};

			gui_window_begin(&gui_context, &gui_window_1);
			if (gui_window_button(&gui_context, &gui_window_1, gui_window_2.is_open ? "Hide Window 2" : "Show Window 2") ==
				GUI_CLICKED)
			{
				gui_window_2.is_open = !gui_window_2.is_open;
			}

			static bool show_bezier = false;
			if (gui_window_button(&gui_context, &gui_window_1, show_bezier ? "Hide Bezier" : "Show Bezier") == GUI_CLICKED)
			{
				show_bezier = !show_bezier;
			}

			gui_window_slider_float(&gui_context, &gui_window_1, &global_animation_rate, vec2_new(-5.0, 5.0), "Anim Rate");

			for (u32 i = 0; i < 12; ++i)
			{
				char buffer[256];
				snprintf(buffer, sizeof(buffer), "Btn %u", i);
				if (gui_window_button(&gui_context, &gui_window_1, buffer) == GUI_CLICKED)
				{
					printf("Clicked %s\n", buffer);
				}
			}
			gui_window_end(&gui_context, &gui_window_1);

			gui_window_begin(&gui_context, &gui_window_2);
			if (gui_window_button(&gui_context, &gui_window_2, "Toggle Movable") == GUI_CLICKED)
			{
				gui_window_2.is_movable = !gui_window_2.is_movable;
			}
			if (gui_window_button(&gui_context, &gui_window_2, "Toggle Resizable") == GUI_CLICKED)
			{
				gui_window_2.is_resizable = !gui_window_2.is_resizable;
			}
			gui_window_end(&gui_context, &gui_window_2);

			if (show_bezier)
			{
				static Vec2 bezier_points[] = {
					{.x = 400, .y = 200},
					{.x = 600, .y = 200},
					{.x = 800, .y = 600},
					{.x = 1000, .y = 600},
				};
				gui_bezier(&gui_context, 4, bezier_points, 25, vec4_new(0.6, 0, 0, 1), 0.01);

				// Bezier Controls
				for (u32 i = 0; i < 4; ++i)
				{
					const Vec2 window_size = gui_context.frame_state.screen_size;
					const Vec2 point_pos_screen_space = {
						.x = bezier_points[i].x * window_size.x,
						.y = bezier_points[i].y * window_size.y,
					};
					const Vec2 bezier_button_size = vec2_new(25, 25);
					if (gui_button(&gui_context, "", vec2_sub(bezier_points[i], vec2_scale(bezier_button_size, 0.5)),
					bezier_button_size) == GUI_HELD)
					{
						bezier_points[i] = vec2_add(bezier_points[i], mouse_delta);
					}
				}
			}

			GpuBufferWriteInfo gui_index_buffer_write_info = {
				.buffer = &gui_index_buffer,
				.size = sizeof(u32) * sb_count(gui_context.draw_data.indices),
				.data = gui_context.draw_data.indices,
			};
			gpu_write_buffer(&gpu_device, &gui_index_buffer_write_info);

			GpuBufferWriteInfo gui_vertex_buffer_write_info = {
				.buffer = &gui_vertex_buffer,
				.size = sizeof(GuiVert) * sb_count(gui_context.draw_data.vertices),
				.data = gui_context.draw_data.vertices,
			};
			gpu_write_buffer(&gpu_device, &gui_vertex_buffer_write_info);
		}

		{	// Update Global Uniforms...
			Mat4 root_transform = trs_to_mat4(game_object_compute_global_transform(&game_object_manager, character.root_object_handle));
			Vec3 root_position = mat4_get_translation(root_transform);
			Mat4 cam_transform = trs_to_mat4(game_object_compute_global_transform(&game_object_manager, character.camera_object_handle));
			Vec3 cam_position = mat4_get_translation(cam_transform);

			CameraComponent* cam_component = OBJECT_GET_COMPONENT(CameraComponent, &game_object_manager, character.camera_object_handle);
			assert(cam_component);
			cam_component->camera_forward = vec3_normalize(vec3_sub(root_position, cam_position));
			const Vec3 cam_target = vec3_add(cam_position, cam_component->camera_forward);
			Mat4 view = mat4_look_at(cam_position, cam_target, cam_component->camera_up);
			Mat4 proj = mat4_perspective(cam_component->fov, (float)window_width / (float)window_height, 0.01f, 4000.0f);

			global_uniform_data.view = view;
			global_uniform_data.projection = proj;

			memcpy(&global_uniform_data.eye, &cam_position, sizeof(float) * 3);

			GpuBufferWriteInfo global_uniform_buffer_write_info = {
				.buffer = &global_uniform_buffers[current_frame],
				.size = sizeof(GlobalUniformStruct),
				.data = &global_uniform_data,
			};
			gpu_write_buffer(&gpu_device, &global_uniform_buffer_write_info);
		}

		// gpu_reset_command_buffer will wait on the command buffer if its still in-flight
		GpuCommandBuffer* command_buffer = &command_buffers[current_frame];
		gpu_reset_command_buffer(&gpu_device, command_buffer);

		GpuBackBuffer backbuffer;
		gpu_get_next_backbuffer(&gpu_device, command_buffer, &backbuffer);
		GpuTexture backbuffer_texture;
		gpu_backbuffer_get_texture(&backbuffer, &backbuffer_texture);

		// Geometry Pass
		{
			GpuColorAttachmentDescriptor geometry_pass_color_attachments[] = {
				{
					.texture = &backbuffer_texture, 
					.clear_color = {0.392f, 0.584f, 0.929f, 0.f},
					.load_action = GPU_LOAD_ACTION_CLEAR,
					.store_action = GPU_STORE_ACTION_STORE,
				},
			};

			GpuDepthAttachmentDescriptor geometry_pass_depth_attachment = {
				.texture = &depth_texture,
				.clear_depth = 1.0f,
				.load_action = GPU_LOAD_ACTION_CLEAR,
				.store_action = GPU_STORE_ACTION_STORE,
			};

			GpuRenderPassCreateInfo geometry_render_pass_create_info = {
				.num_color_attachments = ARRAY_COUNT(geometry_pass_color_attachments), 
				.color_attachments = geometry_pass_color_attachments,
				.depth_attachment = &geometry_pass_depth_attachment,
				.command_buffer = command_buffer,
			};
			GpuRenderPass geometry_render_pass;
			gpu_begin_render_pass(&gpu_device, &geometry_render_pass_create_info, &geometry_render_pass);
			gpu_render_pass_set_render_pipeline(&geometry_render_pass, &geometry_render_pipeline);
			gpu_render_pass_set_bind_group(&geometry_render_pass, &geometry_render_pipeline, &global_bind_groups[current_frame]);

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
					gpu_render_pass_set_bind_group(&geometry_render_pass, &geometry_render_pipeline, &render_data_component->bind_groups[current_frame]);	

					if (static_model_component)
					{
						gpu_render_pass_draw(&geometry_render_pass, 0, static_model_component->static_model.num_indices);
					}
					else if (animated_model_component)
					{
						gpu_render_pass_draw(&geometry_render_pass, 0, animated_model_component->animated_model.num_indices);
					}
				}
			}
			gpu_end_render_pass(&geometry_render_pass);
		}

		// Debug Draw Pass
		{
			debug_draw_sphere(&debug_draw_context, &(DebugDrawSphere){
				.center = vec3_new(0,0,0),
				.radius = 25,
				.latitudes = 12,
				.longitudes = 12,
				.color = vec4_new(1,0,0,1),
			});

			debug_draw_end_frame(&debug_draw_context, &gpu_device, &global_uniform_data.view, &global_uniform_data.projection);

			DebugDrawRecordInfo debug_draw_record_info = {
				.gpu_device = &gpu_device,
				.command_buffer = command_buffer,
				.color_texture = &backbuffer_texture,
				.depth_texture = &depth_texture,
			};
			debug_draw_record_commands(&debug_draw_context, &debug_draw_record_info);
		}

		// GUI Pass
		{
			GpuColorAttachmentDescriptor gui_color_attachments[] = {
				{
					.texture = &backbuffer_texture, 
					.load_action = GPU_LOAD_ACTION_LOAD,
					.store_action = GPU_STORE_ACTION_STORE,
				},
			};

			GpuRenderPassCreateInfo gui_render_pass_create_info = {
				.num_color_attachments = ARRAY_COUNT(gui_color_attachments), 
				.color_attachments = gui_color_attachments,
				.depth_attachment = NULL, 
				.command_buffer = command_buffer,
			};
			GpuRenderPass gui_render_pass;
			gpu_begin_render_pass(&gpu_device, &gui_render_pass_create_info, &gui_render_pass);
			gpu_render_pass_set_render_pipeline(&gui_render_pass, &gui_render_pipeline);
			gpu_render_pass_set_bind_group(&gui_render_pass, &gui_render_pipeline, &gui_bind_group);

			gpu_render_pass_draw(&gui_render_pass, 0, sb_count(gui_context.draw_data.indices));		
			gpu_end_render_pass(&gui_render_pass);
		}

		gpu_present_backbuffer(&gpu_device, command_buffer, &backbuffer);
		gpu_commit_command_buffer(&gpu_device, command_buffer);

		current_frame = (current_frame + 1) % swapchain_count;
	}

	gpu_device_wait_idle(&gpu_device);

	// Cleanup
	static_model_free(&gpu_device, &static_model);

	gpu_destroy_texture(&gpu_device, &depth_texture);


	for (i32 swapchain_idx = 0; swapchain_idx < swapchain_count; ++swapchain_idx)
	{
		gpu_destroy_buffer(&gpu_device, &global_uniform_buffers[swapchain_idx]);
		gpu_destroy_bind_group(&gpu_device, &global_bind_groups[swapchain_idx]);
	}
	gpu_destroy_bind_group_layout(&gpu_device, &global_bind_group_layout);

    gui_shutdown(&gui_context);

	gpu_destroy_device(&gpu_device);

	task_system_shutdown(&task_system);

	return 0;
}

