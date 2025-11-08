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
#include "gui.h"
#include "debug_draw.h"

// FCS TODO: Gpu2 Porting Checklist
// Delete all old shaders (fragment, vertex, include directories)
// Rename all 'Gpu2' to just 'Gpu'

int main()
{
	// Create our window	
	String window_title = string_new("C Game (");
	string_append(&window_title, gpu2_get_api_name());
	string_append(&window_title, ")");

	i32 window_width = 1280;
	i32 window_height = 720;
    Window window = window_create(window_title.data, window_width, window_height);

	string_free(&window_title);

	Gpu2Device gpu2_device;
	gpu2_create_device(&window, &gpu2_device);

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
	//if (!animated_model_load("data/meshes/blender_simple.glb", &gpu2_device, &animated_model))
	//if (!animated_model_load("data/meshes/running.glb", &gpu2_device, &animated_model))
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

		const bool create_animated_model = (i % 4) != 0;
		if (create_animated_model)
		{
			Gpu2BufferCreateInfo joints_buffer_create_info = {
				.usage = GPU2_BUFFER_USAGE_STORAGE_BUFFER,
				.is_cpu_visible = true,
				.size = animated_model.joints_buffer_size,
				.data = NULL,
			};
			Gpu2Buffer joint_matrices_buffer;
			gpu2_create_buffer(&gpu2_device, &joints_buffer_create_info, &joint_matrices_buffer);

			AnimatedModelComponent animated_model_component_data = {
				.animated_model = animated_model,
				.joint_matrices_buffer = joint_matrices_buffer,
				.mapped_buffer_data = gpu2_map_buffer(&gpu2_device, &joint_matrices_buffer),
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
	Gpu2Shader geometry_vertex_shader;
	gpu2_create_shader(&gpu2_device, &vertex_shader_create_info, &geometry_vertex_shader);

	Gpu2ShaderCreateInfo fragment_shader_create_info = {
		.filename = "bin/shaders/mesh_render.frag",
	};
	Gpu2Shader geometry_fragment_shader;
	gpu2_create_shader(&gpu2_device, &fragment_shader_create_info, &geometry_fragment_shader);

	GlobalUniformStruct global_uniform_data = {
		.view = mat4_identity,
		.projection = mat4_identity,
		.eye = vec4_zero,
	};
	
	Gpu2BufferCreateInfo global_uniform_buffer_create_info = {
		.usage = GPU2_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = sizeof(GlobalUniformStruct),
		.data = NULL,
	};
	Gpu2Buffer global_uniform_buffer;
	gpu2_create_buffer(&gpu2_device, &global_uniform_buffer_create_info, &global_uniform_buffer);
	
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

	Gpu2BindGroupLayout* geometry_pipeline_bind_group_layouts[] = { &global_bind_group_layout, &per_object_bind_group_layout };

	Gpu2RenderPipelineCreateInfo geometry_render_pipeline_create_info = {
		.vertex_shader = &geometry_vertex_shader,
		.fragment_shader = &geometry_fragment_shader,
		.num_bind_group_layouts = ARRAY_COUNT(geometry_pipeline_bind_group_layouts),
		.bind_group_layouts = geometry_pipeline_bind_group_layouts,
		.depth_test_enabled = true,
	};
	Gpu2RenderPipeline geometry_render_pipeline;
	assert(gpu2_create_render_pipeline(&gpu2_device, &geometry_render_pipeline_create_info, &geometry_render_pipeline));	

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

	// FCS TODO: move gui.h into gui/ add gui_render.h to that directory for all gui-rendering code
    // BEGIN GUI SETUP
    GuiContext gui_context;
    gui_init(&gui_context);

    assert(gui_context.default_font.font_type == FONT_TYPE_RGBA);

	// Gui Vertex Buffer
    size_t max_gui_vertices_size = sizeof(GuiVert) * 10000;
	Gpu2BufferCreateInfo gui_vertex_buffer_create_info = {
		.usage = GPU2_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = max_gui_vertices_size,
		.data = NULL,
	};
	Gpu2Buffer gui_vertex_buffer;
	gpu2_create_buffer(&gpu2_device, &gui_vertex_buffer_create_info, &gui_vertex_buffer);

	// Gui Index Buffer
    size_t max_gui_indices_size = sizeof(u32) * 30000;
	Gpu2BufferCreateInfo gui_index_buffer_create_info = {
		.usage = GPU2_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = max_gui_indices_size,
		.data = NULL,
	};
	Gpu2Buffer gui_index_buffer;
	gpu2_create_buffer(&gpu2_device, &gui_index_buffer_create_info, &gui_index_buffer);

    // Create Font Texture
	Gpu2TextureCreateInfo font_texture_create_info = {
		.format = GPU2_FORMAT_RGBA8_UNORM,	
		.extent = {
			.width = gui_context.default_font.image_width,
			.height = gui_context.default_font.image_height,
			.depth = 1,
		},
		.usage = GPU2_TEXTURE_USAGE_SAMPLED,
		.is_cpu_visible = false,
	};
	Gpu2Texture font_texture;
	gpu2_create_texture(&gpu2_device, &font_texture_create_info, &font_texture);

	Gpu2TextureWriteInfo font_texture_write_info = {
		.texture = &font_texture,
		.width = gui_context.default_font.image_width,
		.height = gui_context.default_font.image_height,
		.data = gui_context.default_font.image_data,
	};
    gpu2_write_texture(&gpu2_device, &font_texture_write_info);

	Gpu2SamplerCreateInfo font_sampler_create_info = {
		.filters = {
			.min = GPU2_FILTER_LINEAR,
			.mag = GPU2_FILTER_LINEAR,
		},
		.address_modes = {
			.u = GPU2_SAMPLER_ADDRESS_MODE_REPEAT,
			.v = GPU2_SAMPLER_ADDRESS_MODE_REPEAT,
			.w = GPU2_SAMPLER_ADDRESS_MODE_REPEAT,
		},
		.max_anisotropy = 16,
	};
	Gpu2Sampler font_sampler;
	gpu2_create_sampler(&gpu2_device, &font_sampler_create_info, &font_sampler);

	// Create GUI Bind Group Layout, Bind Group, and Update it
	Gpu2BindGroupLayoutCreateInfo gui_bind_group_layout_create_info = {
		.index = 0,
		.num_bindings = 4,
		.bindings = (Gpu2ResourceBinding[4]){
			{
				.type = GPU2_BINDING_TYPE_BUFFER,
				.shader_stages = GPU2_SHADER_STAGE_VERTEX ,	
			},
			{
				.type = GPU2_BINDING_TYPE_BUFFER,
				.shader_stages = GPU2_SHADER_STAGE_VERTEX ,	
			},		
			{
				.type = GPU2_BINDING_TYPE_TEXTURE,
				.shader_stages = GPU2_SHADER_STAGE_FRAGMENT,	
			},
			{
				.type = GPU2_BINDING_TYPE_SAMPLER,
				.shader_stages = GPU2_SHADER_STAGE_FRAGMENT,	
			},
		},
	};
	Gpu2BindGroupLayout gui_bind_group_layout;
	assert(gpu2_create_bind_group_layout(&gpu2_device, &gui_bind_group_layout_create_info, &gui_bind_group_layout));

	Gpu2BindGroupCreateInfo gui_bind_group_create_info = {
		.layout = &gui_bind_group_layout,
	};
	Gpu2BindGroup gui_bind_group;
	assert(gpu2_create_bind_group(&gpu2_device, &gui_bind_group_create_info, &gui_bind_group));

	const Gpu2BindGroupUpdateInfo gui_bind_group_update_info = {
		.bind_group = &gui_bind_group,
		.num_writes = 4,
		.writes = (Gpu2ResourceWrite[4]){
			{
				.binding_index = 0,
				.type = GPU2_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &gui_index_buffer,
				},
			},
			{
				.binding_index = 1,
				.type = GPU2_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &gui_vertex_buffer,
				},
			},
			{
				.binding_index = 2,
				.type = GPU2_BINDING_TYPE_TEXTURE,
				.texture_binding = {
					.texture = &font_texture,
				},
			},
			{
				.binding_index = 3,
				.type = GPU2_BINDING_TYPE_SAMPLER,
				.sampler_binding = {
					.sampler = &font_sampler,
				},
			},
		},
	};
	gpu2_update_bind_group(&gpu2_device, &gui_bind_group_update_info);

	Gpu2ShaderCreateInfo gui_vertex_shader_create_info = {
			.filename = "bin/shaders/gui.vert",
		};
	Gpu2Shader gui_vertex_shader;
	gpu2_create_shader(&gpu2_device, &gui_vertex_shader_create_info, &gui_vertex_shader);

	Gpu2ShaderCreateInfo gui_fragment_shader_create_info = {
			.filename = "bin/shaders/gui.frag",
		};
	Gpu2Shader gui_fragment_shader;
	gpu2_create_shader(&gpu2_device, &gui_fragment_shader_create_info, &gui_fragment_shader);

	Gpu2BindGroupLayout* gui_pipeline_bind_group_layouts[] = { &gui_bind_group_layout };
	Gpu2RenderPipelineCreateInfo gui_render_pipeline_create_info = {
		.vertex_shader = &gui_vertex_shader,
		.fragment_shader = &gui_fragment_shader,
		.num_bind_group_layouts = ARRAY_COUNT(gui_pipeline_bind_group_layouts),
		.bind_group_layouts = gui_pipeline_bind_group_layouts,
		.depth_test_enabled = false,
	};
	Gpu2RenderPipeline gui_render_pipeline;
	assert(gpu2_create_render_pipeline(&gpu2_device, &gui_render_pipeline_create_info, &gui_render_pipeline));

	gpu2_destroy_shader(&gpu2_device, &gui_vertex_shader);
	gpu2_destroy_shader(&gpu2_device, &gui_fragment_shader);

	// END GUI SETUP

	// BEGIN DEBUG DRAW SETUP
	DebugDrawContext debug_draw_context;
	debug_draw_init(&gpu2_device, &debug_draw_context);
	// END DEBUG DRAW SETUP

	const u32 swapchain_count = gpu2_get_swapchain_count(&gpu2_device);
	u64 time = time_now();
    u32 current_frame = 0;

    double accumulated_delta_time = 0.0f;
    size_t frames_rendered = 0;

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
		if (window_input_pressed(&window, 'M') && !show_mouse_pressed_last_frame)
		{
			show_mouse = !show_mouse;
			window_show_mouse_cursor(&window, show_mouse);
			show_mouse_pressed_last_frame = true;
		}
		else if (!window_input_pressed(&window, 'M'))
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
				const float padding = 5.f;
				const float fps_button_size = 155.f;
				if (gui_button(&gui_context, buffer, vec2_new(window_width - fps_button_size - padding, padding),
				   vec2_new(fps_button_size, 30)) == GUI_CLICKED)
				{
					accumulated_delta_time = 0.0f;
					frames_rendered = 0;
				}
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

			Gpu2BufferWriteInfo gui_index_buffer_write_info = {
				.buffer = &gui_index_buffer,
				.size = sizeof(u32) * sb_count(gui_context.draw_data.indices),
				.data = gui_context.draw_data.indices,
			};
			gpu2_write_buffer(&gpu2_device, &gui_index_buffer_write_info);

			Gpu2BufferWriteInfo gui_vertex_buffer_write_info = {
				.buffer = &gui_vertex_buffer,
				.size = sizeof(GuiVert) * sb_count(gui_context.draw_data.vertices),
				.data = gui_context.draw_data.vertices,
			};
			gpu2_write_buffer(&gpu2_device, &gui_vertex_buffer_write_info);
		}

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
				.buffer = &global_uniform_buffer,
				.size = sizeof(GlobalUniformStruct),
				.data = &global_uniform_data,
			};
			gpu2_write_buffer(&gpu2_device, &global_uniform_buffer_write_info);
		}

		Gpu2CommandBuffer command_buffer;
		assert(gpu2_create_command_buffer(&gpu2_device, &command_buffer));

		Gpu2Drawable drawable;
		assert(gpu2_get_next_drawable(&gpu2_device, &command_buffer, &drawable));
		Gpu2Texture drawable_texture;
		assert(gpu2_drawable_get_texture(&drawable, &drawable_texture));

		// Geometry Pass
		{
			Gpu2ColorAttachmentDescriptor geometry_pass_color_attachments[] = {
				{
					.texture = &drawable_texture, 
					.clear_color = {0.392f, 0.584f, 0.929f, 0.f},
					.load_action = GPU2_LOAD_ACTION_CLEAR,
					.store_action = GPU2_STORE_ACTION_STORE,
				},
			};

			Gpu2DepthAttachmentDescriptor geometry_pass_depth_attachment = {
				.texture = &depth_texture,
				.clear_depth = 1.0f,
				.load_action = GPU2_LOAD_ACTION_CLEAR,
				.store_action = GPU2_STORE_ACTION_STORE,
			};

			Gpu2RenderPassCreateInfo geometry_render_pass_create_info = {
				.num_color_attachments = ARRAY_COUNT(geometry_pass_color_attachments), 
				.color_attachments = geometry_pass_color_attachments,
				.depth_attachment = &geometry_pass_depth_attachment,
				.command_buffer = &command_buffer,
			};
			Gpu2RenderPass geometry_render_pass;
			gpu2_begin_render_pass(&gpu2_device, &geometry_render_pass_create_info, &geometry_render_pass);
			gpu2_render_pass_set_render_pipeline(&geometry_render_pass, &geometry_render_pipeline);
			gpu2_render_pass_set_bind_group(&geometry_render_pass, &geometry_render_pipeline, &global_bind_group);

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
					gpu2_render_pass_set_bind_group(&geometry_render_pass, &geometry_render_pipeline, &render_data_component->bind_groups[current_frame]);	

					if (static_model_component)
					{
						gpu2_render_pass_draw(&geometry_render_pass, 0, static_model_component->static_model.num_indices);
					}
					else if (animated_model_component)
					{
						animated_model_component->current_anim_time += (delta_time * animated_model_component->animation_rate * global_animation_rate);
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
						gpu2_render_pass_draw(&geometry_render_pass, 0, animated_model_component->animated_model.num_indices);
					}
				}
			}
			gpu2_end_render_pass(&geometry_render_pass);
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

			debug_draw_end_frame(&debug_draw_context, &gpu2_device, &global_uniform_data.view, &global_uniform_data.projection);

			DebugDrawRecordInfo debug_draw_record_info = {
				.gpu_device = &gpu2_device,
				.command_buffer = &command_buffer,
				.color_texture = &drawable_texture,
				.depth_texture = &depth_texture,
			};
			debug_draw_record_commands(&debug_draw_context, &debug_draw_record_info);
		}

		// GUI Pass
		{
			Gpu2ColorAttachmentDescriptor gui_color_attachments[] = {
				{
					.texture = &drawable_texture, 
					.load_action = GPU2_LOAD_ACTION_LOAD,
					.store_action = GPU2_STORE_ACTION_STORE,
				},
			};

			Gpu2RenderPassCreateInfo gui_render_pass_create_info = {
				.num_color_attachments = ARRAY_COUNT(gui_color_attachments), 
				.color_attachments = gui_color_attachments,
				.depth_attachment = NULL, 
				.command_buffer = &command_buffer,
			};
			Gpu2RenderPass gui_render_pass;
			gpu2_begin_render_pass(&gpu2_device, &gui_render_pass_create_info, &gui_render_pass);
			gpu2_render_pass_set_render_pipeline(&gui_render_pass, &gui_render_pipeline);
			gpu2_render_pass_set_bind_group(&gui_render_pass, &gui_render_pipeline, &gui_bind_group);

			gpu2_render_pass_draw(&gui_render_pass, 0, sb_count(gui_context.draw_data.indices));		

			gpu2_end_render_pass(&gui_render_pass);
		}



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

    gui_shutdown(&gui_context);

	gpu2_destroy_device(&gpu2_device);

	return 0;
}

