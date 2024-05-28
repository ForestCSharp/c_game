#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>

#include "timer.h"
#include "uniforms.h"

// #define DISABLE_LOG
#ifdef DISABLE_LOG
#define printf(...)
#endif

// GPU2_IMPLEMENTATION_<BACKEND> set up in build.sh	
#include "gpu2/gpu2.c"

#include "math/matrix.h"
#include "math/quat.h"
#include "math/vec.h"
#include "stretchy_buffer.h"

#include "gpu/gpu.c"
#include "gpu/bindless.h"
#include "gpu/debug_draw.h"
#include "app/app.h"

#include "gltf.h"
#include "model/static_model.h"
#include "model/animated_model.h"
#include "gui.h"

#include "game_object/game_object.h"

// FCS TODO: Testing truetype
#include "truetype.h"

//FCS TODO: No mesh/cube on gpu2 metal backend...

int main()
{
    TrueTypeFont true_type_font = {};
    if (truetype_load_file("data/fonts/Menlo-Regular.ttf", &true_type_font))
    {
        printf("Font Loaded \n");
    }

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

	sbuffer(StaticVertex) gpu2_vertices = NULL;
	sbuffer(u32) gpu2_indices = NULL;

	Vec3 box_axes[3] = {
		vec3_new(1,0,0),
		vec3_new(0,1,0),
		vec3_new(0,0,1)
	};
	float box_halfwidths[3] = {1.0f, 1.0f, 1.0f};
	append_box(box_axes, box_halfwidths, &gpu2_vertices, &gpu2_indices);	

	Gpu2BufferCreateInfo vertex_buffer_create_info = {
		.is_cpu_visible = true,
		.size = sb_count(gpu2_vertices) * sizeof(StaticVertex),
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
	};
	Gpu2RenderPipeline gpu2_render_pipeline;
	assert(gpu2_create_render_pipeline(&gpu2_device, &render_pipeline_create_info, &gpu2_render_pipeline));	

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

		Gpu2RenderPassCreateInfo render_pass_create_info = {
			.num_color_attachments = ARRAY_COUNT(color_attachments), 
			.color_attachments = color_attachments,
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

	//
    //
	//BEGIN OLD GPU CODE
	//
	//

	// Create our gpu context
    GpuContext gpu_context = gpu_create_context(&window);

	// Create bindless resource manager
	BindlessResourceManager bindless_resource_manager = {};
	bindless_resource_manager_create(&gpu_context, &bindless_resource_manager);

	const GpuDescriptorLayoutCreateInfo global_descriptor_layout_create_info = {
		.set_number = 1,
		.binding_count = 2,
		.bindings = (GpuDescriptorBinding[2]){
			DESCRIPTOR_BINDING_UNIFORM_BUFFER(0, 1, GPU_SHADER_STAGE_ALL_GRAPHICS),
			DESCRIPTOR_BINDING_COMBINED_IMAGE_SAMPLER(1, 1, GPU_SHADER_STAGE_FRAGMENT),
		},
	};

	const GpuDescriptorLayoutCreateInfo per_object_descriptor_layout_create_info = {
		.set_number = 2,
		.binding_count = 3,
		.bindings = (GpuDescriptorBinding[3]) {
			DESCRIPTOR_BINDING_UNIFORM_BUFFER(0, 1, GPU_SHADER_STAGE_ALL_GRAPHICS),
			DESCRIPTOR_BINDING_STORAGE_BUFFER(1, 1, GPU_SHADER_STAGE_VERTEX),
			DESCRIPTOR_BINDING_STORAGE_BUFFER(2, 1, GPU_SHADER_STAGE_VERTEX),
		},
	};

	GpuDescriptorLayout global_descriptor_layout = gpu_create_descriptor_layout(&gpu_context, &global_descriptor_layout_create_info);
	GpuDescriptorLayout per_object_descriptor_layout = gpu_create_descriptor_layout(&gpu_context, &per_object_descriptor_layout_create_info);
	GpuDescriptorLayout main_pipeline_descriptor_layouts[] = { bindless_resource_manager.descriptor_layout, global_descriptor_layout, per_object_descriptor_layout };
	i32 main_pipeline_descriptor_layout_count = sizeof(main_pipeline_descriptor_layouts) / sizeof(main_pipeline_descriptor_layouts[0]);
	GpuPipelineLayout main_pipeline_layout = gpu_create_pipeline_layout(&gpu_context, main_pipeline_descriptor_layout_count, main_pipeline_descriptor_layouts);

	//GameObject + Components Setup
	GameObjectManager game_object_manager = {};
	GameObjectManager* game_object_manager_ptr = &game_object_manager;
	game_object_manager_init(game_object_manager_ptr);

	// Set up Static Model
	StaticModel static_model;
	if (!static_model_load("data/meshes/Monkey.glb", &gpu_context, &bindless_resource_manager, &static_model))
	{
		printf("Failed to Load Static Model\n");
		return 1;
	}

	// Set up Animated Model. Components will be added later as they contain animated joint data.
	AnimatedModel animated_model;
	if (!animated_model_load("data/meshes/cesium_man.glb", &gpu_context, &bindless_resource_manager, &animated_model))
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
		assert(static_model_load("data/meshes/StarterMech_Body.glb", &gpu_context, &bindless_resource_manager, &body_static_model));
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
		assert(static_model_load("data/meshes/StarterMech_Head.glb", &gpu_context, &bindless_resource_manager, &head_static_model));
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
		assert(static_model_load("data/meshes/StarterMech_Arm.glb", &gpu_context, &bindless_resource_manager, &arm_static_model));
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
		assert(static_model_load("data/meshes/StarterMech_Legs.glb", &gpu_context, &bindless_resource_manager, &legs_static_model));
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
			GpuBufferCreateInfo joint_buffers_create_info = {
				.size = animated_model.joints_buffer_size,
				.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
				.memory_properties = GPU_MEMORY_PROPERTY_DEVICE_LOCAL | GPU_MEMORY_PROPERTY_HOST_VISIBLE | GPU_MEMORY_PROPERTY_HOST_COHERENT,
				.debug_name = "animated model joint buffer",
			};

			GpuBuffer joint_matrices_buffer = gpu_create_buffer(&gpu_context, &joint_buffers_create_info);

			AnimatedModelComponent animated_model_component_data = {
				.animated_model = animated_model,
				.joint_matrices_buffer = joint_matrices_buffer,
				.mapped_buffer_data = gpu_map_buffer(&gpu_context, &joint_matrices_buffer),
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
			&gpu_context, 
			object_handle, 
			per_object_descriptor_layout
		);
	}

	// BEGIN DEBUG DRAW SETUP
	DebugDrawContext debug_draw_context;
	debug_draw_init(&gpu_context, &debug_draw_context);

    // BEGIN GUI SETUP
    GuiContext gui_context;
    gui_init(&gui_context);

    assert(gui_context.default_font.font_type == FONT_TYPE_RGBA);

    // Create Font Texture
    GpuImage font_image = gpu_create_image(
        &gpu_context,
        &(GpuImageCreateInfo){
            .dimensions = {gui_context.default_font.image_width, gui_context.default_font.image_height, 1},
            .format = GPU_FORMAT_RGBA8_UNORM, 
            .mip_levels = 1,
            .usage = GPU_IMAGE_USAGE_SAMPLED | GPU_IMAGE_USAGE_TRANSFER_DST,
            .memory_properties = GPU_MEMORY_PROPERTY_DEVICE_LOCAL,
			.debug_name = "font_image",
        }
	);

    gpu_upload_image(&gpu_context, &font_image, gui_context.default_font.image_width,
                     gui_context.default_font.image_height, gui_context.default_font.image_data);

    GpuImageView font_image_view = gpu_create_image_view(&gpu_context, 
														 &(GpuImageViewCreateInfo){
															 .image = &font_image,
															 .type = GPU_IMAGE_VIEW_2D,
															 .format = GPU_FORMAT_RGBA8_UNORM,
															 .aspect = GPU_IMAGE_ASPECT_COLOR,
														 });

    GpuSampler font_sampler = gpu_create_sampler(&gpu_context, 
												 &(GpuSamplerCreateInfo){
													 .mag_filter = GPU_FILTER_LINEAR,
													 .min_filter = GPU_FILTER_LINEAR,
													 .max_anisotropy = 16,
												 }); 

	const GpuDescriptorLayoutCreateInfo gui_descriptor_layout_create_info = {
		.set_number = 0,
		.binding_count = 1,
		.bindings =
		(GpuDescriptorBinding[1]){
			DESCRIPTOR_BINDING_COMBINED_IMAGE_SAMPLER(0, 1, GPU_SHADER_STAGE_ALL_GRAPHICS),	
		},
	};
	GpuDescriptorLayout gui_descriptor_layout = gpu_create_descriptor_layout(&gpu_context, &gui_descriptor_layout_create_info);
    GpuPipelineLayout gui_pipeline_layout = gpu_create_pipeline_layout(&gpu_context, 1, &gui_descriptor_layout);
    GpuDescriptorSet gui_descriptor_set = gpu_create_descriptor_set(&gpu_context, &gui_descriptor_layout);

    GpuDescriptorWrite gui_descriptor_writes[1] = {{
        .binding_desc = &gui_descriptor_layout.bindings[0],
		.index = 0,
        .image_write =
            &(GpuDescriptorWriteImage){
                .image_view = &font_image_view,
                .sampler = &font_sampler,
                .layout = GPU_IMAGE_LAYOUT_SHADER_READ_ONLY,
            },
    }};

    gpu_update_descriptor_set(&gpu_context, &gui_descriptor_set, 1, gui_descriptor_writes);

    GpuShaderModule gui_vertex_module = gpu_create_shader_module_from_file(&gpu_context, "data/shaders/vertex/gui.vert.spv");
    GpuShaderModule gui_fragment_module = gpu_create_shader_module_from_file(&gpu_context, "data/shaders/fragment/gui.frag.spv");

    GpuFormat gui_attribute_formats[] = {
        GPU_FORMAT_RG32_SFLOAT,   // Position
        GPU_FORMAT_RG32_SFLOAT,   // UVs
        GPU_FORMAT_RGBA32_SFLOAT, // Color
        GPU_FORMAT_R32_SINT,      // Has Texture?
    };
    size_t num_gui_attributes = sizeof(gui_attribute_formats) / sizeof(gui_attribute_formats[0]);

    GpuPipelineRenderingCreateInfo dynamic_rendering_info = {
        .color_attachment_count = 1,
        .color_formats =
            (GpuFormat *)&gpu_context.surface_format.format, // FIXME: Store and Get from Context (as a GpuFormat)
        .depth_format = GPU_FORMAT_D32_SFLOAT,
    };

    GpuGraphicsPipelineCreateInfo gui_pipeline_create_info = {
        .vertex_module = &gui_vertex_module,
        .fragment_module = &gui_fragment_module,
        .rendering_info = &dynamic_rendering_info,
        .layout = &gui_pipeline_layout,
        .num_attributes = num_gui_attributes,
        .attribute_formats = gui_attribute_formats,
        .depth_stencil =
            {
                .depth_test = false,
                .depth_write = false,
            },
        .enable_color_blending = true,
    };

    GpuPipeline gui_pipeline = gpu_create_graphics_pipeline(&gpu_context, &gui_pipeline_create_info);

    // Can destroy shader modules after creating pipeline
    gpu_destroy_shader_module(&gpu_context, &gui_vertex_module);
    gpu_destroy_shader_module(&gpu_context, &gui_fragment_module);

    size_t max_gui_vertices_size = sizeof(GuiVert) * 10000; // TODO: need to support gpu buffer resizing
	GpuBufferCreateInfo gui_vertex_buffer_create_info = {
		.size = max_gui_vertices_size,
		.usage = GPU_BUFFER_USAGE_VERTEX_BUFFER,
		.memory_properties = GPU_MEMORY_PROPERTY_DEVICE_LOCAL | GPU_MEMORY_PROPERTY_HOST_VISIBLE | GPU_MEMORY_PROPERTY_HOST_COHERENT,
		.debug_name = "gui vertex buffer",
	};
    GpuBuffer gui_vertex_buffer = gpu_create_buffer(&gpu_context, &gui_vertex_buffer_create_info);

    size_t max_gui_indices_size = sizeof(u32) * 30000; // TODO: need to support gpu buffer resizing
	GpuBufferCreateInfo gui_index_buffer_create_info = {
		.size = max_gui_indices_size,
		.usage = GPU_BUFFER_USAGE_INDEX_BUFFER,
		.memory_properties = GPU_MEMORY_PROPERTY_DEVICE_LOCAL | GPU_MEMORY_PROPERTY_HOST_VISIBLE | GPU_MEMORY_PROPERTY_HOST_COHERENT,
		.debug_name = "gui index buffer",
	};
    GpuBuffer gui_index_buffer = gpu_create_buffer(&gpu_context, &gui_index_buffer_create_info);

    // END GUI SETUP

    GlobalUniformStruct uniform_data = {
        .eye = vec4_new(0.0f, 0.0f, 0.0f, 1.0f),
        .light_dir = vec4_new(0.0f, -1.0f, 1.0f, 0.0f),
    };

    GpuBuffer uniform_buffers[gpu_context.swapchain_image_count];
    for (u32 i = 0; i < gpu_context.swapchain_image_count; ++i)
    {
		GpuBufferCreateInfo global_uniform_buffer_create_info = {
			.size = sizeof(GlobalUniformStruct),
			.usage = GPU_BUFFER_USAGE_UNIFORM_BUFFER,
			.memory_properties = GPU_MEMORY_PROPERTY_DEVICE_LOCAL | GPU_MEMORY_PROPERTY_HOST_VISIBLE | GPU_MEMORY_PROPERTY_HOST_COHERENT,
			.debug_name = "global_uniform_buffer",
		};
        uniform_buffers[i] = gpu_create_buffer(&gpu_context, &global_uniform_buffer_create_info); 
    }

    GpuShaderModule static_vertex_module = gpu_create_shader_module_from_file(&gpu_context, "data/shaders/vertex/static.vert.spv");
	GpuShaderModule skinned_vertex_module = gpu_create_shader_module_from_file(&gpu_context, "data/shaders/vertex/skinned.vert.spv");
    GpuShaderModule shaded_fragment_module = gpu_create_shader_module_from_file(&gpu_context, "data/shaders/fragment/basic_shaded.frag.spv");
	GpuShaderModule unshaded_fragment_module = gpu_create_shader_module_from_file(&gpu_context, "data/shaders/fragment/basic_unshaded.frag.spv"); 

    GpuDescriptorSet global_descriptor_sets[gpu_context.swapchain_image_count];

    for (u32 i = 0; i < gpu_context.swapchain_image_count; ++i)
    {
        global_descriptor_sets[i] = gpu_create_descriptor_set(&gpu_context, &global_descriptor_layout);

        GpuDescriptorWrite descriptor_writes[2] = {
			{
				.binding_desc = &global_descriptor_layout.bindings[0],
				.index = 0,
				.buffer_write = &(GpuDescriptorWriteBuffer) {
						.buffer = &uniform_buffers[i],
						.offset = 0,
						.range = sizeof(uniform_data),
				}
			},
			{
				.binding_desc = &global_descriptor_layout.bindings[1],
				.index = 0,
				.image_write = &(GpuDescriptorWriteImage){
					.image_view = &font_image_view,
					.sampler = &font_sampler,
					.layout = GPU_IMAGE_LAYOUT_SHADER_READ_ONLY,
				},
			},	
		};

        gpu_update_descriptor_set(&gpu_context, &global_descriptor_sets[i], 2, descriptor_writes);
    }

    GpuGraphicsPipelineCreateInfo static_pipeline_create_info = {
		.vertex_module = &static_vertex_module,
	   .fragment_module = &shaded_fragment_module,
	   .rendering_info = &dynamic_rendering_info,
	   .layout = &main_pipeline_layout,
	   .num_attributes = 0,
	   .attribute_formats = NULL,
	   .depth_stencil = {
		   .depth_test = true,
		   .depth_write = true,
	}};

    GpuPipeline static_pipeline = gpu_create_graphics_pipeline(&gpu_context, &static_pipeline_create_info);

    GpuGraphicsPipelineCreateInfo skinned_pipeline_create_info = {
		.vertex_module = &skinned_vertex_module,
	   .fragment_module = &shaded_fragment_module,
	   .rendering_info = &dynamic_rendering_info,
	   .layout = &main_pipeline_layout,
	   .num_attributes = 0,
	   .attribute_formats = NULL,
	   .depth_stencil = {
		   .depth_test = true,
		   .depth_write = true,
	}};
	GpuPipeline skinned_pipeline = gpu_create_graphics_pipeline(&gpu_context, &skinned_pipeline_create_info);

    // destroy shader modules after creating pipeline
    gpu_destroy_shader_module(&gpu_context, &static_vertex_module);
    gpu_destroy_shader_module(&gpu_context, &skinned_vertex_module);
    gpu_destroy_shader_module(&gpu_context, &shaded_fragment_module);
    gpu_destroy_shader_module(&gpu_context, &unshaded_fragment_module);

    u64 time = time_now();
    double accumulated_delta_time = 0.0f;
    size_t frames_rendered = 0;

    // Setup Per-Frame Resources
    GpuCommandBuffer command_buffers[gpu_context.swapchain_image_count];
    GpuSemaphore image_acquired_semaphores[gpu_context.swapchain_image_count];
    GpuSemaphore render_complete_semaphores[gpu_context.swapchain_image_count];
    GpuFence in_flight_fences[gpu_context.swapchain_image_count];
    for (u32 i = 0; i < gpu_context.swapchain_image_count; ++i)
    {
        command_buffers[i] = gpu_create_command_buffer(&gpu_context);
        image_acquired_semaphores[i] = gpu_create_semaphore(&gpu_context);
        render_complete_semaphores[i] = gpu_create_semaphore(&gpu_context);
        in_flight_fences[i] = gpu_create_fence(&gpu_context, true);
    }

    // These are set up on startup / resize
    GpuImage depth_image = {};
    GpuImageView depth_view = {};

    int width = 0;
    int height = 0;

    i32 mouse_x, mouse_y;
	window_get_mouse_pos(&window, &mouse_x, &mouse_y);

	bool show_mouse = false;
	window_show_mouse_cursor(&window, show_mouse);

    u32 current_frame = 0;
    while (window_handle_messages(&window))
    {
        if (window_input_pressed(&window, KEY_ESCAPE))
        {
            break;
        }

        int new_width, new_height;
        window_get_dimensions(&window, &new_width, &new_height);

        if (new_width == 0 || new_height == 0)
        {
            continue;
        }

        if (new_width != width || new_height != height)
        {
            gpu_resize_swapchain(&gpu_context, &window);
            width = new_width;
            height = new_height;

            gpu_destroy_image_view(&gpu_context, &depth_view);
            gpu_destroy_image(&gpu_context, &depth_image);

			depth_image = gpu_create_image(
				&gpu_context,
				&(GpuImageCreateInfo){
				  .dimensions = {width, height, 1},
				  .format = GPU_FORMAT_D32_SFLOAT,
				  .mip_levels = 1,
				  .usage = GPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT,
				  .memory_properties = GPU_MEMORY_PROPERTY_DEVICE_LOCAL,
				  .debug_name = "depth_image",
				}
			);

            depth_view = gpu_create_image_view(
				&gpu_context, 
				&(GpuImageViewCreateInfo){
					.image = &depth_image,
					.type = GPU_IMAGE_VIEW_2D,
					.format = GPU_FORMAT_D32_SFLOAT,
					.aspect = GPU_IMAGE_ASPECT_DEPTH,
				}
			);

			current_frame = 0;
			gpu_wait_idle(&gpu_context);
        }

        u64 new_time = time_now();
        double delta_time = time_seconds(new_time - time);
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

		if (!show_mouse) 
		{
			int x,y;
			window_get_position(&window, &x, &y);

			int width, height;
			window_get_dimensions(&window, &width, &height);

			window_set_mouse_pos(&window, x + width/2, y + height/2);
		}

        // BEGIN Gui
		static float global_animation_rate = 1.0f;

		GuiFrameState gui_frame_state = {
			.screen_size = vec2_new(width, height),
			.mouse_pos = mouse_pos,
			.mouse_buttons = {
				window_input_pressed(&window, KEY_LEFT_MOUSE),
				window_input_pressed(&window, KEY_RIGHT_MOUSE),
				window_input_pressed(&window, KEY_MIDDLE_MOUSE)
			},
		};
		gui_begin_frame(&gui_context, gui_frame_state);
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
				if (gui_button(&gui_context, buffer, vec2_new(width - fps_button_size - padding, padding),
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

			gpu_upload_buffer(
				&gpu_context, 
				&gui_vertex_buffer, 
				sizeof(GuiVert) * sb_count(gui_context.draw_data.vertices),
				gui_context.draw_data.vertices
			);
			gpu_upload_buffer(
				&gpu_context, 
				&gui_index_buffer, 
				sizeof(u32) * sb_count(gui_context.draw_data.indices),
				gui_context.draw_data.indices
			);
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
			Mat4 proj = mat4_perspective(cam_component->fov, (float)width / (float)height, 0.01f, 4000.0f);

			uniform_data.view = view;
			uniform_data.projection = proj;

			memcpy(&uniform_data.eye, &cam_position, sizeof(float) * 3);

			gpu_upload_buffer(&gpu_context, &uniform_buffers[current_frame], sizeof(uniform_data), &uniform_data);
		}

        gpu_wait_for_fence(&gpu_context, &in_flight_fences[current_frame]);
        gpu_reset_fence(&gpu_context, &in_flight_fences[current_frame]);

        u32 image_index = gpu_acquire_next_image(&gpu_context, &image_acquired_semaphores[current_frame]);

        gpu_begin_command_buffer(&command_buffers[current_frame]);

        // FCS TODO: optimize src+dst stages
        gpu_cmd_image_barrier(
			&command_buffers[current_frame],
			&(GpuImageBarrier){
				.image = &gpu_context.swapchain_images[current_frame],
				.src_stage = GPU_PIPELINE_STAGE_TOP_OF_PIPE,
				.dst_stage = GPU_PIPELINE_STAGE_TOP_OF_PIPE,
				.old_layout = GPU_IMAGE_LAYOUT_UNDEFINED,
				.new_layout = GPU_IMAGE_LAYOUT_COLOR_ATACHMENT,
			}
		);

        gpu_cmd_begin_rendering(
            &command_buffers[current_frame],
            &(GpuRenderingInfo){
                .render_width = width,
                .render_height = height,
                .color_attachment_count = 1,
                .color_attachments = (GpuRenderingAttachmentInfo[1]){
					{
						.image_view = &gpu_context.swapchain_image_views[current_frame],
						.image_layout = GPU_IMAGE_LAYOUT_COLOR_ATACHMENT,
						.load_op = GPU_LOAD_OP_CLEAR,
						.store_op = GPU_STORE_OP_STORE,
						.clear_value =
						{
							.clear_color = {0.392f, 0.584f, 0.929f, 0.0f},
						}
					}
				},
                .depth_attachment = &(GpuRenderingAttachmentInfo){
					.image_view = &depth_view,
					.image_layout = GPU_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT,
					.load_op = GPU_LOAD_OP_CLEAR,
					.store_op = GPU_STORE_OP_DONT_CARE,
					.clear_value = {
						.depth_stencil =
						{
							.depth = 1.0f,
							.stencil = 0,
						}
					}
				},
                .stencil_attachment = NULL, // TODO:
            }
		);

		gpu_cmd_set_viewport(&command_buffers[current_frame], &(GpuViewport){
			.x = 0,
			.y = 0,
			.width = width,
			.height = height,
			.min_depth = 0.0,
			.max_depth = 1.0,
		});

		{	//Draw Our Game Objects with Static Models
			//gpu_cmd_bind_pipeline(&command_buffers[current_frame], &static_pipeline);
			gpu_cmd_bind_descriptor_set(&command_buffers[current_frame], &main_pipeline_layout, &bindless_resource_manager.descriptor_set);
			gpu_cmd_bind_descriptor_set(&command_buffers[current_frame], &main_pipeline_layout, &global_descriptor_sets[current_frame]);

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
					//Assign to our persistently mapped storage
					render_data_component->uniform_data[current_frame]->model = trs_to_mat4(game_object_compute_global_transform(&game_object_manager, object_handle)); 
					gpu_cmd_bind_descriptor_set(&command_buffers[current_frame], &main_pipeline_layout, &render_data_component->descriptor_sets[current_frame]);

					StaticModelComponent* static_model_component = OBJECT_GET_COMPONENT(StaticModelComponent, &game_object_manager, object_handle);
					if (static_model_component)
					{
						//FCS TODO: Don't need to do this every frame... can do on setup once we have req. components
						render_data_component->uniform_data[current_frame]->vertex_buffer_idx = static_model_component->static_model.vertex_buffer_handle.idx;
						render_data_component->uniform_data[current_frame]->index_buffer_idx = static_model_component->static_model.index_buffer_handle.idx;

						//FCS TODO: Avoid all these pipeline binds draw all static models, then draw all animated models...
						gpu_cmd_bind_pipeline(&command_buffers[current_frame], &static_pipeline);
						gpu_cmd_draw(&command_buffers[current_frame], static_model_component->static_model.num_indices, 1);
					}

					AnimatedModelComponent* animated_model_component = OBJECT_GET_COMPONENT(AnimatedModelComponent, &game_object_manager, object_handle);
					if (animated_model_component)
					{
						//FCS TODO: Don't need to do this every frame... can do on setup once we have req. components
						render_data_component->uniform_data[current_frame]->vertex_buffer_idx = animated_model_component->animated_model.vertex_buffer_handle.idx;
						render_data_component->uniform_data[current_frame]->index_buffer_idx = animated_model_component->animated_model.index_buffer_handle.idx;
						//FCS TODO: Pass joint and inverse bind matrix buffers bindlessly as well...

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
							&gpu_context, 
							&animated_model, 
							animated_model_component->current_anim_time,
							animated_model_component->mapped_buffer_data
						);

						//FCS TODO: Avoid all these pipeline binds
						gpu_cmd_bind_pipeline(&command_buffers[current_frame], &skinned_pipeline);
						gpu_cmd_draw(&command_buffers[current_frame], animated_model_component->animated_model.num_indices, 1);
					}
				}
			}
		}

		//FCS TODO: Move to top of main loop 
		debug_draw_begin_frame(&debug_draw_context, current_frame);

		debug_draw_sphere(&debug_draw_context, &(DebugDrawSphere){
			.center = vec3_new(0,0,0),
			.radius = 10,
			.latitudes = 12,
			.longitudes = 12,
			.color = vec4_new(1,0,0,1),
		});

		debug_draw_end_frame(&debug_draw_context, &gpu_context, &uniform_data.view, &uniform_data.projection);
		debug_draw_record_commands(&debug_draw_context, &command_buffers[current_frame]);

        // BEGIN gui rendering
        gpu_cmd_bind_pipeline(&command_buffers[current_frame], &gui_pipeline);
        gpu_cmd_set_viewport(
			&command_buffers[current_frame], 
			&(GpuViewport){
				.x = 0,
				.y = 0,
				.width = width,
				.height = height,
				.min_depth = 0.0,
				.max_depth = 1.0,
			}
		);
        gpu_cmd_bind_descriptor_set(&command_buffers[current_frame], &gui_pipeline_layout, &gui_descriptor_set);
        gpu_cmd_bind_vertex_buffer(&command_buffers[current_frame], &gui_vertex_buffer);
        gpu_cmd_bind_index_buffer(&command_buffers[current_frame], &gui_index_buffer);
        gpu_cmd_draw_indexed(&command_buffers[current_frame], sb_count(gui_context.draw_data.indices), 1);
        // END gui rendering

        gpu_cmd_end_rendering(&command_buffers[current_frame]);

        // FCS TODO: Optimize stages
        gpu_cmd_image_barrier(
			&command_buffers[current_frame],
			&(GpuImageBarrier){
				.image = &gpu_context.swapchain_images[current_frame],
				.src_stage = GPU_PIPELINE_STAGE_TOP_OF_PIPE,
				.dst_stage = GPU_PIPELINE_STAGE_TOP_OF_PIPE,
				.old_layout = GPU_IMAGE_LAYOUT_COLOR_ATACHMENT,
				.new_layout = GPU_IMAGE_LAYOUT_PRESENT_SRC,
			}
		);
        gpu_end_command_buffer(&command_buffers[current_frame]);

        gpu_queue_submit(&gpu_context, &command_buffers[current_frame], &image_acquired_semaphores[current_frame],
                         &render_complete_semaphores[current_frame], &in_flight_fences[current_frame]);

        gpu_queue_present(&gpu_context, image_index, &render_complete_semaphores[current_frame]);

        current_frame = (current_frame + 1) % gpu_context.swapchain_image_count;

        gpu_wait_idle(&gpu_context); // TODO: Need per-frame resources (gui vbuf,ibuf all uniform buffers)
    }

    gpu_wait_idle(&gpu_context);

	//FCS TODO: require component cleanup functions...
	for (i64 obj_idx = 0; obj_idx < sb_count(game_object_manager.game_object_array); ++obj_idx)
	{
		GameObjectHandle object_handle = {.idx = obj_idx, };
		if (!OBJECT_IS_VALID(&game_object_manager, object_handle)) 
		{
			continue;
		}		

		ObjectRenderDataComponent* render_data_component = OBJECT_GET_COMPONENT(ObjectRenderDataComponent, &game_object_manager, object_handle);
		if (render_data_component)
		{
			for (i32 swapchain_idx = 0; swapchain_idx < gpu_context.swapchain_image_count; ++swapchain_idx)
			{
				gpu_destroy_descriptor_set(&gpu_context, &render_data_component->descriptor_sets[swapchain_idx]);
				gpu_destroy_buffer(&gpu_context, &render_data_component->uniform_buffers[swapchain_idx]);
			}
			sb_free(render_data_component->descriptor_sets);
			sb_free(render_data_component->uniform_buffers);
		}

		AnimatedModelComponent* animated_model_component = OBJECT_GET_COMPONENT(AnimatedModelComponent, &game_object_manager, object_handle);
		if (animated_model_component)
		{
			gpu_destroy_buffer(&gpu_context, &animated_model_component->joint_matrices_buffer);
		}
	}
	game_object_manager_destroy(&game_object_manager);

    for (u32 i = 0; i < gpu_context.swapchain_image_count; ++i)
    {
        gpu_free_command_buffer(&gpu_context, &command_buffers[i]);
        gpu_destroy_semaphore(&gpu_context, &image_acquired_semaphores[i]);
        gpu_destroy_semaphore(&gpu_context, &render_complete_semaphores[i]);
        gpu_destroy_fence(&gpu_context, &in_flight_fences[i]);
    }

    gpu_destroy_buffer(&gpu_context, &gui_vertex_buffer);
    gpu_destroy_buffer(&gpu_context, &gui_index_buffer);

    // BEGIN Gui Cleanup
    gpu_destroy_pipeline(&gpu_context, &gui_pipeline);
    gpu_destroy_descriptor_set(&gpu_context, &gui_descriptor_set);
    gpu_destroy_pipeline_layout(&gpu_context, &gui_pipeline_layout);
	gpu_destroy_descriptor_layout(&gpu_context, &gui_descriptor_layout);
    gpu_destroy_sampler(&gpu_context, &font_sampler);
    gpu_destroy_image_view(&gpu_context, &font_image_view);
    gpu_destroy_image(&gpu_context, &font_image);
    // END Gui Cleanup

    gpu_destroy_pipeline(&gpu_context, &static_pipeline);
	gpu_destroy_pipeline(&gpu_context, &skinned_pipeline);

    gpu_destroy_pipeline_layout(&gpu_context, &main_pipeline_layout);

	gpu_destroy_descriptor_layout(&gpu_context, &global_descriptor_layout);
	gpu_destroy_descriptor_layout(&gpu_context, &per_object_descriptor_layout);	

    gpu_destroy_image_view(&gpu_context, &depth_view);
    gpu_destroy_image(&gpu_context, &depth_image);
    for (u32 i = 0; i < gpu_context.swapchain_image_count; ++i)
    {
        gpu_destroy_buffer(&gpu_context, &uniform_buffers[i]);
        gpu_destroy_descriptor_set(&gpu_context, &global_descriptor_sets[i]);
    }
	static_model_free(&gpu_context, &bindless_resource_manager, &static_model);
	animated_model_free(&gpu_context, &bindless_resource_manager, &animated_model);

    gui_shutdown(&gui_context);
	debug_draw_shutdown(&gpu_context, &debug_draw_context);

	bindless_resource_manager_destroy(&gpu_context, &bindless_resource_manager);
    gpu_destroy_context(&gpu_context);

    return 0;
}
