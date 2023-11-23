#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>

#include "timer.h"

// #define DISABLE_LOG
#ifdef DISABLE_LOG
#define printf(...)
#endif

#include "math/matrix.h"
#include "math/quat.h"
#include "math/vec.h"
#include "stretchy_buffer.h"

#include "gpu.c"
#include "window/window.h"

#include "gltf.h"
#include "model/static_model.h"
#include "model/animated_model.h"
#include "gui.h"

// FCS TODO: Testing collision
#include "collision/collision.h"
#include "collision/collision_render.h"

// FCS TODO: Testing truetype
#include "truetype.h"

typedef struct UniformStruct
{
	Mat4 model;
	Mat4 view;
	Mat4 projection;
	Mat4 mvp;
	Vec4 eye;
	Vec4 light_dir;
	bool is_colliding;
} UniformStruct;

bool read_file(const char *filename, size_t *out_file_size, u32 **out_data)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
	{
        return false;
	}
	
    fseek(file, 0L, SEEK_END);
    const size_t file_size = *out_file_size = ftell(file);
    rewind(file);

    *out_data = calloc(1, file_size + 1);
    if (!*out_data)
    {
        fclose(file);
        return false;
    }

    if (fread(*out_data, 1, file_size, file) != file_size)
    {
        fclose(file);
        free(*out_data);
        return false;
    }

    fclose(file);
    return true;
}

GpuShaderModule create_shader_module_from_file(GpuContext *gpu_context, const char *filename)
{
    size_t shader_size = 0;
    u32 *shader_code = NULL;
    if (!read_file(filename, &shader_size, &shader_code))
    {
        exit(1);
    }

    GpuShaderModule module = gpu_create_shader_module(gpu_context, shader_size, shader_code);
    free(shader_code);
    return module;
}

float rand_float(float lower_bound, float upper_bound)
{
    return lower_bound + (float)(rand()) / ((float)(RAND_MAX / (upper_bound - lower_bound)));
}

int main()
{

    TrueTypeFont true_type_font = {};
    if (truetype_load_file("data/fonts/Menlo-Regular.ttf", &true_type_font))
    {
        printf("Font Loaded \n");
        // exit(0);
    }

    Window window = window_create("C Game", 1920, 1080);

    GpuContext gpu_context = gpu_create_context(&window);

	AnimatedModel animated_model;
	//if (!animated_model_load("data/meshes/simple_skin.glb", &gpu_context, &animated_model))
	//if (!animated_model_load("data/meshes/blender_simple.glb", &gpu_context, &animated_model))
	//if (!animated_model_load("data/meshes/running.glb", &gpu_context, &animated_model))
	if (!animated_model_load("data/meshes/cesium_man.glb", &gpu_context, &animated_model))
	{
		printf("Failed to Load Animated Model\n");
		return 1;
	}

	//FCS TODO: BEGIN Move this logic to AnimatedModel as well?
	float animation_start = animated_model.baked_animation.start_time;
	float animation_end = animated_model.baked_animation.end_time;
	float current_anim_time = animation_start;
	//FCS TODO: END

	StaticModel static_model;
	if (!static_model_load("data/meshes/monkey.glb", &gpu_context, &static_model))
	{
		printf("Failed to Load Static Model\n");
		return 1;
	}

    // BEGIN GUI SETUP

    GuiContext gui_context;
    gui_init(&gui_context);

    assert(gui_context.default_font.font_type == FONT_TYPE_RGBA);

    // Create Font Texture
    GpuImage font_image = gpu_create_image(
        &gpu_context,
        &(GpuImageCreateInfo){
            .dimensions = {gui_context.default_font.image_width, gui_context.default_font.image_height, 1},
            .format = GPU_FORMAT_RGBA8_UNORM, // FCS TODO: Check from gui_context.default_font (remove assert on font
                                              // type above)
            .mip_levels = 1,
            .usage = GPU_IMAGE_USAGE_SAMPLED | GPU_IMAGE_USAGE_TRANSFER_DST,
            .memory_properties = GPU_MEMORY_PROPERTY_DEVICE_LOCAL,
        },
        "font_image");

    gpu_upload_image(&gpu_context, &font_image, gui_context.default_font.image_width,
                     gui_context.default_font.image_height, gui_context.default_font.image_data);

    GpuImageView font_image_view = gpu_create_image_view(&gpu_context, &(GpuImageViewCreateInfo){
                                                                           .image = &font_image,
                                                                           .type = GPU_IMAGE_VIEW_2D,
                                                                           .format = GPU_FORMAT_RGBA8_UNORM,
                                                                           .aspect = GPU_IMAGE_ASPECT_COLOR,
                                                                       });

    GpuSampler font_sampler = gpu_create_sampler(&gpu_context, &(GpuSamplerCreateInfo){
                                                                   .mag_filter = GPU_FILTER_LINEAR,
                                                                   .min_filter = GPU_FILTER_LINEAR,
                                                                   .max_anisotropy = 16,
                                                               });

    GpuDescriptorLayout gui_descriptor_layout = {
        .binding_count = 1,
        .bindings =
            (GpuDescriptorBinding[1]){
                {
                    .binding = 0,
                    .type = GPU_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .stage_flags = GPU_SHADER_STAGE_ALL_GRAPHICS,
                },
            },
    };

    GpuPipelineLayout gui_pipeline_layout = gpu_create_pipeline_layout(&gpu_context, &gui_descriptor_layout);
    GpuDescriptorSet gui_descriptor_set = gpu_create_descriptor_set(&gpu_context, &gui_pipeline_layout);

    GpuDescriptorWrite gui_descriptor_writes[1] = {{
        .binding_desc = &gui_pipeline_layout.descriptor_layout.bindings[0],
        .image_write =
            &(GpuDescriptorWriteImage){
                .image_view = &font_image_view,
                .sampler = &font_sampler,
                .layout = GPU_IMAGE_LAYOUT_SHADER_READ_ONLY,
            },
    }};

    gpu_write_descriptor_set(&gpu_context, &gui_descriptor_set, 1, gui_descriptor_writes);

    GpuShaderModule gui_vertex_module = create_shader_module_from_file(&gpu_context, "data/shaders/vertex/gui.vert.spv");
    GpuShaderModule gui_fragment_module = create_shader_module_from_file(&gpu_context, "data/shaders/fragment/gui.frag.spv");

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
    GpuBuffer gui_vertex_buffer = gpu_create_buffer(
        &gpu_context, GPU_BUFFER_USAGE_VERTEX_BUFFER,
        GPU_MEMORY_PROPERTY_DEVICE_LOCAL | GPU_MEMORY_PROPERTY_HOST_VISIBLE | GPU_MEMORY_PROPERTY_HOST_COHERENT,
        max_gui_vertices_size, "gui vertex buffer");

    size_t max_gui_indices = sizeof(u32) * 30000; // TODO: need to support gpu buffer resizing
    GpuBuffer gui_index_buffer = gpu_create_buffer(&gpu_context, GPU_BUFFER_USAGE_INDEX_BUFFER,
                                                   GPU_MEMORY_PROPERTY_DEVICE_LOCAL | GPU_MEMORY_PROPERTY_HOST_VISIBLE |
                                                       GPU_MEMORY_PROPERTY_HOST_COHERENT,
                                                   max_gui_indices, "gui index buffer");

    // END GUI SETUP

    UniformStruct uniform_data = {
        .eye = vec4_new(0.0f, 0.0f, 0.0f, 1.0f),
        .light_dir = vec4_new(0.0f, -1.0f, 1.0f, 0.0f),
    };

    GpuBuffer uniform_buffers[gpu_context.swapchain_image_count];
    for (u32 i = 0; i < gpu_context.swapchain_image_count; ++i)
    {
        uniform_buffers[i] = gpu_create_buffer(&gpu_context, GPU_BUFFER_USAGE_UNIFORM_BUFFER,
			GPU_MEMORY_PROPERTY_DEVICE_LOCAL | GPU_MEMORY_PROPERTY_HOST_VISIBLE |
			GPU_MEMORY_PROPERTY_HOST_COHERENT,
			sizeof(UniformStruct), "uniform_buffer"
		);
    }

    GpuShaderModule static_vertex_module = create_shader_module_from_file(&gpu_context, "data/shaders/vertex/static.vert.spv");
    GpuShaderModule collision_vis_vertex_module = create_shader_module_from_file(&gpu_context, "data/shaders/vertex/collision_vis.vert.spv");
	GpuShaderModule skinned_vertex_module = create_shader_module_from_file(&gpu_context, "data/shaders/vertex/skinned.vert.spv");
	GpuShaderModule joint_vis_vertex_module = create_shader_module_from_file(&gpu_context, "data/shaders/vertex/visualize_joints.vert.spv");
    GpuShaderModule shaded_fragment_module = create_shader_module_from_file(&gpu_context, "data/shaders/fragment/basic_shaded.frag.spv");
	GpuShaderModule unshaded_fragment_module = create_shader_module_from_file(&gpu_context, "data/shaders/fragment/basic_unshaded.frag.spv");

    GpuDescriptorLayout descriptor_layout = {
        .binding_count = 4,
        .bindings =
            (GpuDescriptorBinding[4]){
                {
                    .binding = 0,
                    .type = GPU_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .stage_flags = GPU_SHADER_STAGE_ALL_GRAPHICS,
                },
                {
                    .binding = 1,
                    .type = GPU_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .stage_flags = GPU_SHADER_STAGE_FRAGMENT,
                },
				{
					.binding = 2,
                    .type = GPU_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .stage_flags = GPU_SHADER_STAGE_VERTEX,

				},
				{
					.binding = 3,
					.type = GPU_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.stage_flags = GPU_SHADER_STAGE_VERTEX,
				},
            },
    };

    GpuPipelineLayout pipeline_layout = gpu_create_pipeline_layout(&gpu_context, &descriptor_layout);
    GpuDescriptorSet descriptor_sets[gpu_context.swapchain_image_count];

    for (u32 i = 0; i < gpu_context.swapchain_image_count; ++i)
    {
        descriptor_sets[i] = gpu_create_descriptor_set(&gpu_context, &pipeline_layout);

        GpuDescriptorWrite descriptor_writes[4] = {
			{
				.binding_desc = &pipeline_layout.descriptor_layout.bindings[0],
				.buffer_write = &(GpuDescriptorWriteBuffer) {
						.buffer = &uniform_buffers[i],
						.offset = 0,
						.range = sizeof(uniform_data),
				}
			},
			{
				.binding_desc = &pipeline_layout.descriptor_layout.bindings[1],
				.image_write = &(GpuDescriptorWriteImage){
					.image_view = &font_image_view,
					.sampler = &font_sampler,
					.layout = GPU_IMAGE_LAYOUT_SHADER_READ_ONLY,
				},
			},
			{
				.binding_desc = &pipeline_layout.descriptor_layout.bindings[2],
				.buffer_write = &(GpuDescriptorWriteBuffer){
					.buffer = &animated_model.joint_matrices_buffer,
					.offset = 0,
					.range = animated_model.joints_buffer_size,
				}
			},
			{
				.binding_desc = &pipeline_layout.descriptor_layout.bindings[3],
				.buffer_write = &(GpuDescriptorWriteBuffer){
					.buffer = &animated_model.inverse_bind_matrices_buffer,
					.offset = 0,
					.range = animated_model.joints_buffer_size,
				}
			}
		};

        gpu_write_descriptor_set(&gpu_context, &descriptor_sets[i], 4, descriptor_writes);
    }


	//BEGIN TEST COLLISION

	test_collision(); // FCS TODO: See Collision.h

	Collider colliders[] = 
	{
		{
			.type = COLLIDER_TYPE_SPHERE,
			.sphere = {
				.center = vec3_new(15, 0, 0),
				.radius = 2.0f,
			},
		},
		{
			.type = COLLIDER_TYPE_CAPSULE,
			.capsule = {
				.center = vec3_new(10,0,0),
				.orientation = quat_identity,
				.half_height = 5.0f,
				.radius = 1.0f,
			},
		},
		{
			.type = COLLIDER_TYPE_OBB,
			.obb = {
				.center = vec3_new(5,0,0),
				.orientation = quat_identity,
				.halfwidths = {3.f, 2.f, 8.f},
			},
		},
		{
			.type = COLLIDER_TYPE_OBB,
			.obb = {
				.center = vec3_new(-10,0,0),
				.orientation = quat_identity,
				.halfwidths = {3.f, 2.f, 8.f},
			},
		},
	};

	static const i32 NUM_COLLIDERS = sizeof(colliders) / sizeof(colliders[0]);

	ColliderRenderData collider_render_datas[NUM_COLLIDERS];
    GpuBuffer collider_uniform_buffers[NUM_COLLIDERS];
	GpuDescriptorSet collider_descriptor_sets[NUM_COLLIDERS];
    for (u32 collider_idx = 0; collider_idx < NUM_COLLIDERS; ++collider_idx)
    {
		collider_render_data_create(&gpu_context, &colliders[collider_idx], &collider_render_datas[collider_idx]);

        collider_uniform_buffers[collider_idx] = gpu_create_buffer(&gpu_context, GPU_BUFFER_USAGE_UNIFORM_BUFFER,
		   GPU_MEMORY_PROPERTY_DEVICE_LOCAL | GPU_MEMORY_PROPERTY_HOST_VISIBLE |
		   GPU_MEMORY_PROPERTY_HOST_COHERENT,
		   sizeof(UniformStruct), "collider_uniform_buffer"
		);

		collider_descriptor_sets[collider_idx] = gpu_create_descriptor_set(&gpu_context, &pipeline_layout);

        GpuDescriptorWrite collider_descriptor_writes[1] = {
			{
				.binding_desc = &pipeline_layout.descriptor_layout.bindings[0],
				.buffer_write = &(GpuDescriptorWriteBuffer) {
						.buffer = &collider_uniform_buffers[collider_idx],
						.offset = 0,
						.range = sizeof(uniform_data),
				}
			},
		};

        gpu_write_descriptor_set(&gpu_context, &collider_descriptor_sets[collider_idx], 1, collider_descriptor_writes);
    }

	//END TEST COLLISION

	//JOINT VIS DATA

	const i32 cube_vertices_count = 36;
	Vec3 cube_vert_positions[cube_vertices_count] = 
	{
		vec3_new(-1,-1,-1),		vec3_new(1,-1,-1),	vec3_new(-1, 1, -1), 
		vec3_new(-1, 1, -1),	vec3_new(1,-1,-1),	vec3_new(1,1,-1),
		vec3_new(-1,-1,1), 		vec3_new(1,-1,1), 	vec3_new(-1, 1, 1),
		vec3_new(-1, 1, 1), 	vec3_new(1,-1,1), 	vec3_new(1,1,1),
		vec3_new(-1,-1,-1),		vec3_new(-1,1,-1),	vec3_new(-1, -1, 1),
		vec3_new(-1, -1, 1),	vec3_new(-1,1,-1),	vec3_new(-1,1,1),
		vec3_new(1,-1,-1),		vec3_new(1,1,-1),	vec3_new(1, -1, 1),
		vec3_new(1, -1, 1),		vec3_new(1,1,-1),	vec3_new(1,1,1),	
		vec3_new(-1,-1,-1),		vec3_new(-1,-1,1),	vec3_new(1, -1, -1),
		vec3_new(1, -1, -1),	vec3_new(-1,-1,1),	vec3_new(1,-1,1),
		vec3_new(-1,1,-1),		vec3_new(-1,1,1),	vec3_new(1, 1, -1),
		vec3_new(1, 1, -1),		vec3_new(-1,1,1),	vec3_new(1,1,1),	
	};

	StaticVertex cube_vertices[cube_vertices_count];
	for (i32 i = 0; i < cube_vertices_count; ++i)
	{
		cube_vertices[i] = (StaticVertex) {
	 		.position = cube_vert_positions[i],
	  		.normal = vec3_new(0,0,0),
	  		.color = vec4_new(0,0,1,1),
	  		.uv = vec2_new(0,0),
		};
	}

    size_t cube_vertices_size = sizeof(StaticVertex) * cube_vertices_count;
	GpuBufferUsageFlags vertex_buffer_usage = GPU_BUFFER_USAGE_VERTEX_BUFFER | GPU_BUFFER_USAGE_TRANSFER_DST;
    GpuBuffer cube_vertex_buffer = gpu_create_buffer(
        &gpu_context, vertex_buffer_usage, GPU_MEMORY_PROPERTY_DEVICE_LOCAL, cube_vertices_size, "cube vertex buffer");
    gpu_upload_buffer(&gpu_context, &cube_vertex_buffer, cube_vertices_size, cube_vertices);

    GpuFormat static_attribute_formats[] = {
        GPU_FORMAT_RGB32_SFLOAT,
        GPU_FORMAT_RGB32_SFLOAT,
        GPU_FORMAT_RGBA32_SFLOAT,
        GPU_FORMAT_RG32_SFLOAT,
    };

    GpuGraphicsPipelineCreateInfo static_pipeline_create_info = {
		.vertex_module = &static_vertex_module,
	   .fragment_module = &shaded_fragment_module,
	   .rendering_info = &dynamic_rendering_info,
	   .layout = &pipeline_layout,
	   .num_attributes = 4,
	   .attribute_formats = static_attribute_formats,
	   .depth_stencil = {
		   .depth_test = true,
		   .depth_write = true,
	}};

    GpuPipeline static_pipeline = gpu_create_graphics_pipeline(&gpu_context, &static_pipeline_create_info);


    GpuGraphicsPipelineCreateInfo collision_vis_pipeline_create_info = {
		.vertex_module = &collision_vis_vertex_module,
	   .fragment_module = &unshaded_fragment_module,
	   .rendering_info = &dynamic_rendering_info,
	   .layout = &pipeline_layout,
	   .num_attributes = 4,
	   .attribute_formats = static_attribute_formats,
	   .depth_stencil = {
		   .depth_test = true,
		   .depth_write = true,
		},
		.rasterizer = {
			.polygon_mode = GPU_POLYGON_MODE_LINE,
		},
	};

	GpuPipeline collision_vis_pipeline = gpu_create_graphics_pipeline(&gpu_context, &collision_vis_pipeline_create_info);

	GpuGraphicsPipelineCreateInfo joint_vis_pipeline_create_info = {
		.vertex_module = &joint_vis_vertex_module,
	   .fragment_module = &unshaded_fragment_module,
	   .rendering_info = &dynamic_rendering_info,
	   .layout = &pipeline_layout,
	   .num_attributes = 4,
	   .attribute_formats = static_attribute_formats,
	   .depth_stencil = {
		   .depth_test = false,
		   .depth_write = false,
		},
		.rasterizer = {
			.polygon_mode = GPU_POLYGON_MODE_LINE,
		},
	};

	GpuPipeline joint_vis_pipeline = gpu_create_graphics_pipeline(&gpu_context, &joint_vis_pipeline_create_info);

	GpuFormat skinned_attribute_formats[] = 
	{
		GPU_FORMAT_RGB32_SFLOAT,
		GPU_FORMAT_RGB32_SFLOAT,
		GPU_FORMAT_RGBA32_SFLOAT,
		GPU_FORMAT_RG32_SFLOAT,
		GPU_FORMAT_RGBA32_SFLOAT, //should be int?
		GPU_FORMAT_RGBA32_SFLOAT  //should be int?
	};

    GpuGraphicsPipelineCreateInfo skinned_pipeline_create_info = {
		.vertex_module = &skinned_vertex_module,
	   .fragment_module = &shaded_fragment_module,
	   .rendering_info = &dynamic_rendering_info,
	   .layout = &pipeline_layout,
	   .num_attributes = 6,
	   .attribute_formats = skinned_attribute_formats,
	   .depth_stencil = {
		   .depth_test = true,
		   .depth_write = true,
	}};
	GpuPipeline skinned_pipeline = gpu_create_graphics_pipeline(&gpu_context, &skinned_pipeline_create_info);

    // destroy shader modules after creating pipeline
    gpu_destroy_shader_module(&gpu_context, &static_vertex_module);
	gpu_destroy_shader_module(&gpu_context, &collision_vis_vertex_module);
    gpu_destroy_shader_module(&gpu_context, &skinned_vertex_module);
	gpu_destroy_shader_module(&gpu_context, &joint_vis_vertex_module);
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

    Quat orientation = quat_identity;

    Vec3 position = vec3_new(0, 12.5, -80);
    Vec3 target = vec3_new(0, 5, -120);
    Vec3 up = vec3_new(0, 1, 0);

    // These are set up on startup / resize
    GpuImage depth_image = {};
    GpuImageView depth_view = {};

    int width = 0;
    int height = 0;

    u32 current_frame = 0;
    while (window_handle_messages(&window))
    {
        if (input_pressed(KEY_ESCAPE))
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

            depth_image = gpu_create_image(&gpu_context,
                                           &(GpuImageCreateInfo){
                                               .dimensions = {width, height, 1},
                                               .format = GPU_FORMAT_D32_SFLOAT,
                                               .mip_levels = 1,
                                               .usage = GPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                                               .memory_properties = GPU_MEMORY_PROPERTY_DEVICE_LOCAL,
                                           },
                                           "depth_image");

            depth_view = gpu_create_image_view(&gpu_context, &(GpuImageViewCreateInfo){
                                                                 .image = &depth_image,
                                                                 .type = GPU_IMAGE_VIEW_2D,
                                                                 .format = GPU_FORMAT_D32_SFLOAT,
                                                                 .aspect = GPU_IMAGE_ASPECT_DEPTH,
                                                             });
        }

        // BEGIN Gui Test

        i32 mouse_x, mouse_y;
        window_get_mouse_pos(&window, &mouse_x, &mouse_y);

        // FCS TODO: open_windows memory leak here
        GuiFrameState gui_frame_state = {.screen_size = vec2_new(width, height),
                                         .mouse_pos = vec2_new(mouse_x, mouse_y),
                                         .mouse_buttons = {input_pressed(KEY_LEFT_MOUSE),
                                                           input_pressed(KEY_RIGHT_MOUSE),
                                                           input_pressed(KEY_MIDDLE_MOUSE)}};

        gui_begin_frame(&gui_context, gui_frame_state);

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

		static float animation_rate = 1.0f;
        gui_window_slider_float(&gui_context, &gui_window_1, &animation_rate, vec2_new(-5.0, 5.0), "Anim Rate");

        static float model_rotation = 135.0f;
        gui_window_slider_float(&gui_context, &gui_window_1, &model_rotation, vec2_new(-360, 360), "Anim Model Rotation");

		static float collider_rotation = 90.0f;
        gui_window_slider_float(&gui_context, &gui_window_1, &collider_rotation, vec2_new(-360, 360), "Collider Rotation");

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
                    Vec2 mouse_pos = gui_context.frame_state.mouse_pos;
                    Vec2 prev_mouse_pos = gui_context.prev_frame_state.mouse_pos;
                    Vec2 mouse_delta = vec2_sub(mouse_pos, prev_mouse_pos);

                    bezier_points[i] = vec2_add(bezier_points[i], mouse_delta);
                }
            }
        }

        // TODO: should be per-frame resources
        gpu_upload_buffer(&gpu_context, &gui_vertex_buffer, sizeof(GuiVert) * sb_count(gui_context.draw_data.vertices),
                          gui_context.draw_data.vertices);
        gpu_upload_buffer(&gpu_context, &gui_index_buffer, sizeof(u32) * sb_count(gui_context.draw_data.indices),
                          gui_context.draw_data.indices);

        // END Gui Test

        gpu_wait_for_fence(&gpu_context, &in_flight_fences[current_frame]);
        gpu_reset_fence(&gpu_context, &in_flight_fences[current_frame]);

        u32 image_index = gpu_acquire_next_image(&gpu_context, &image_acquired_semaphores[current_frame]);

        gpu_begin_command_buffer(&command_buffers[current_frame]);

        // FCS TODO: optimize src+dst stages
        gpu_cmd_image_barrier(&command_buffers[current_frame],
                              &(GpuImageBarrier){
                                  .image = &gpu_context.swapchain_images[current_frame],
                                  .src_stage = GPU_PIPELINE_STAGE_TOP_OF_PIPE,
                                  .dst_stage = GPU_PIPELINE_STAGE_TOP_OF_PIPE,
                                  .old_layout = GPU_IMAGE_LAYOUT_UNDEFINED,
                                  .new_layout = GPU_IMAGE_LAYOUT_COLOR_ATACHMENT,
                              });

        gpu_cmd_begin_rendering(
            &command_buffers[current_frame],
            &(GpuRenderingInfo){
                .render_width = width,
                .render_height = height,
                .color_attachment_count = 1,
                .color_attachments =
                    (GpuRenderingAttachmentInfo[1]){{.image_view = &gpu_context.swapchain_image_views[current_frame],
                                                     .image_layout = GPU_IMAGE_LAYOUT_COLOR_ATACHMENT,
                                                     .load_op = GPU_LOAD_OP_CLEAR,
                                                     .store_op = GPU_STORE_OP_STORE,
                                                     .clear_value =
                                                         {
                                                             .clear_color = {0.392f, 0.584f, 0.929f, 0.0f},
                                                         }}},
                .depth_attachment =
                    &(GpuRenderingAttachmentInfo){.image_view = &depth_view,
                                                  .image_layout = GPU_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT,
                                                  .load_op = GPU_LOAD_OP_CLEAR,
                                                  .store_op = GPU_STORE_OP_DONT_CARE,
                                                  .clear_value = {.depth_stencil =
                                                                      {
                                                                          .depth = 1.0f,
                                                                          .stencil = 0,
                                                                      }}},
                .stencil_attachment = NULL, // TODO:
            });

		gpu_cmd_set_viewport(&command_buffers[current_frame], &(GpuViewport){
			.x = 0,
			.y = 0,
			.width = width,
			.height = height,
			.min_depth = 0.0,
			.max_depth = 1.0,
		});

		{	// Draw Our Animated Model
			gpu_cmd_bind_pipeline(&command_buffers[current_frame], &skinned_pipeline);
			
			gpu_cmd_bind_index_buffer(&command_buffers[current_frame], &animated_model.index_buffer);
			gpu_cmd_bind_vertex_buffer(&command_buffers[current_frame], &animated_model.vertex_buffer);
			gpu_cmd_bind_descriptor_set(&command_buffers[current_frame], &pipeline_layout, &descriptor_sets[current_frame]);
			gpu_cmd_draw_indexed(&command_buffers[current_frame], animated_model.num_indices, 1);
		}

		{	// Joint Visualization
			gpu_cmd_bind_pipeline(&command_buffers[current_frame], &joint_vis_pipeline);
			gpu_cmd_bind_vertex_buffer(&command_buffers[current_frame], &cube_vertex_buffer);
			gpu_cmd_bind_descriptor_set(&command_buffers[current_frame], &pipeline_layout, &descriptor_sets[current_frame]);
			gpu_cmd_draw(&command_buffers[current_frame], cube_vertices_count, animated_model.num_joints);
		}

		{	// Draw Our Static Geometry
			gpu_cmd_bind_pipeline(&command_buffers[current_frame], &static_pipeline);

			// Static Model	
			//gpu_cmd_bind_index_buffer(&command_buffers[current_frame], &static_model.index_buffer);
			//gpu_cmd_bind_vertex_buffer(&command_buffers[current_frame], &static_model.vertex_buffer);
			//gpu_cmd_bind_descriptor_set(&command_buffers[current_frame], &pipeline_layout, &descriptor_sets[current_frame]);
			//gpu_cmd_draw_indexed(&command_buffers[current_frame], static_model.num_indices, 1);
		}

		{	// Draw our colliders
			gpu_cmd_bind_pipeline(&command_buffers[current_frame], &collision_vis_pipeline);
			
			for (i32 collider_idx = 0; collider_idx < NUM_COLLIDERS; ++collider_idx)
			{
				gpu_cmd_bind_index_buffer(&command_buffers[current_frame], &collider_render_datas[collider_idx].index_buffer);
				gpu_cmd_bind_vertex_buffer(&command_buffers[current_frame], &collider_render_datas[collider_idx].vertex_buffer);
				gpu_cmd_bind_descriptor_set(&command_buffers[current_frame], &pipeline_layout, &collider_descriptor_sets[collider_idx]);
				gpu_cmd_draw_indexed(&command_buffers[current_frame], collider_render_datas[collider_idx].num_indices, 1);
			}			
		}

        // BEGIN gui rendering
        gpu_cmd_bind_pipeline(&command_buffers[current_frame], &gui_pipeline);
        gpu_cmd_set_viewport(&command_buffers[current_frame], &(GpuViewport){
                                                                  .x = 0,
                                                                  .y = 0,
                                                                  .width = width,
                                                                  .height = height,
                                                                  .min_depth = 0.0,
                                                                  .max_depth = 1.0,
                                                              });
        gpu_cmd_bind_descriptor_set(&command_buffers[current_frame], &gui_pipeline_layout, &gui_descriptor_set);
        gpu_cmd_bind_vertex_buffer(&command_buffers[current_frame], &gui_vertex_buffer);
        gpu_cmd_bind_index_buffer(&command_buffers[current_frame], &gui_index_buffer);
        gpu_cmd_draw_indexed(&command_buffers[current_frame], sb_count(gui_context.draw_data.indices), 1);
        // END gui rendering

        gpu_cmd_end_rendering(&command_buffers[current_frame]);

        // FCS TODO: Optimize stages
        gpu_cmd_image_barrier(&command_buffers[current_frame],
                              &(GpuImageBarrier){
                                  .image = &gpu_context.swapchain_images[current_frame],
                                  .src_stage = GPU_PIPELINE_STAGE_TOP_OF_PIPE,
                                  .dst_stage = GPU_PIPELINE_STAGE_TOP_OF_PIPE,
                                  .old_layout = GPU_IMAGE_LAYOUT_COLOR_ATACHMENT,
                                  .new_layout = GPU_IMAGE_LAYOUT_PRESENT_SRC,
                              });
        gpu_end_command_buffer(&command_buffers[current_frame]);

        gpu_queue_submit(&gpu_context, &command_buffers[current_frame], &image_acquired_semaphores[current_frame],
                         &render_complete_semaphores[current_frame], &in_flight_fences[current_frame]);

        gpu_queue_present(&gpu_context, image_index, &render_complete_semaphores[current_frame]);

        u64 new_time = time_now();
        double delta_time = time_seconds(new_time - time);
        time = new_time;

        accumulated_delta_time += delta_time;
        frames_rendered++;

        const float base_move_speed = 36.0f;
        const float move_speed = (input_pressed(KEY_SHIFT) ? base_move_speed * 3 : base_move_speed) * delta_time;

        if (input_pressed('W'))
        {
            position.z += move_speed;
            target.z += move_speed;
        }
        if (input_pressed('S'))
        {
            position.z -= move_speed;
            target.z -= move_speed;
        }
        if (input_pressed('A'))
        {
            position.x -= move_speed;
            target.x -= move_speed;
        }
        if (input_pressed('D'))
        {
            position.x += move_speed;
            target.x += move_speed;
        }
        if (input_pressed('Q'))
        {
            position.y -= move_speed;
            target.y -= move_speed;
        }
        if (input_pressed('E'))
        {
            position.y += move_speed;
            target.y += move_speed;
        }

		{	//Anim Update
			current_anim_time += (delta_time * animation_rate);
			if (current_anim_time <= animation_start)
			{
				current_anim_time = animation_end;
			}
			else if (current_anim_time >= animation_end)
			{
				current_anim_time = animation_start;
			}

			animated_model_update_animation(&gpu_context, &animated_model, current_anim_time);
		}

        {
            Quat q1 = quat_new(vec3_new(0, 1, 0), model_rotation * DEGREES_TO_RADIANS);
            Quat q2 = quat_new(vec3_new(1, 0, 0), 0 * DEGREES_TO_RADIANS);
			Quat q3 = quat_new(vec3_new(0, 0, 1), 0 * DEGREES_TO_RADIANS);

            Quat q = quat_normalize(quat_mul(quat_mul(q1, q2), q3));

            orientation = quat_normalize(q);

			Vec3 scale = vec3_new(15,15,15);
			Mat4 scale_matrix = mat4_scale(scale);
			Mat4 orientation_matrix = quat_to_mat4(orientation);
			Mat4 translation_matrix = mat4_translation(vec3_new(0, 30, 30));

            Mat4 model = mat4_mul_mat4(mat4_mul_mat4(scale_matrix, orientation_matrix), translation_matrix); 
            Mat4 view = mat4_look_at(position, target, up);
            Mat4 proj = mat4_perspective(60.0f, (float)width / (float)height, 0.01f, 1000.0f);

            Mat4 mv = mat4_mul_mat4(model, view);
            uniform_data.model = model;
			uniform_data.view = view;
			uniform_data.projection = proj;
            uniform_data.mvp = mat4_mul_mat4(mv, proj);

            memcpy(&uniform_data.eye, &position, sizeof(float) * 3);

            gpu_upload_buffer(&gpu_context, &uniform_buffers[current_frame], sizeof(uniform_data), &uniform_data);

			for (i32 collider_idx = 0; collider_idx < NUM_COLLIDERS; ++collider_idx)
			{
				Collider* collider = &colliders[collider_idx];

				collider_set_orientation(collider, quat_new(vec3_new(0,0,1), collider_rotation * DEGREES_TO_RADIANS));

				Mat4 collider_transform = collider_compute_transform(collider);
				uniform_data.model = collider_transform; 
				uniform_data.view = view;
            	Mat4 mv = mat4_mul_mat4(uniform_data.model, uniform_data.view);
				uniform_data.projection = proj;
				uniform_data.mvp = mat4_mul_mat4(mv, proj);

				uniform_data.is_colliding = false;
				for (i32 other_idx = 0; other_idx < NUM_COLLIDERS; ++other_idx)
				{
					if (other_idx == collider_idx) continue;

					const Collider* other_collider = &colliders[other_idx];

					if (hit_test_colliders(collider, other_collider, NULL))
					{
						uniform_data.is_colliding = true;
						break;
					}
				}
				gpu_upload_buffer(&gpu_context, &collider_uniform_buffers[collider_idx], sizeof(uniform_data), &uniform_data);
			}
        }

        current_frame = (current_frame + 1) % gpu_context.swapchain_image_count;

        gpu_wait_idle(&gpu_context); // TODO: Need per-frame resources (gui vbuf,ibuf all uniform buffers)
    }

    gpu_wait_idle(&gpu_context);


    for (u32 collider_idx = 0; collider_idx < NUM_COLLIDERS; ++collider_idx)
	{
		collider_render_data_free(&gpu_context, &collider_render_datas[collider_idx]);
		gpu_destroy_buffer(&gpu_context, &collider_uniform_buffers[collider_idx]);
		gpu_destroy_descriptor_set(&gpu_context, &collider_descriptor_sets[collider_idx]);
	}

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
    gpu_destroy_sampler(&gpu_context, &font_sampler);
    gpu_destroy_image_view(&gpu_context, &font_image_view);
    gpu_destroy_image(&gpu_context, &font_image);
    // END Gui Cleanup

    gpu_destroy_pipeline(&gpu_context, &static_pipeline);
    gpu_destroy_pipeline(&gpu_context, &collision_vis_pipeline);
	gpu_destroy_pipeline(&gpu_context, &joint_vis_pipeline);
	gpu_destroy_pipeline(&gpu_context, &skinned_pipeline);

    gpu_destroy_pipeline_layout(&gpu_context, &pipeline_layout);
    gpu_destroy_image_view(&gpu_context, &depth_view);
    gpu_destroy_image(&gpu_context, &depth_image);
    for (u32 i = 0; i < gpu_context.swapchain_image_count; ++i)
    {
        gpu_destroy_buffer(&gpu_context, &uniform_buffers[i]);
        gpu_destroy_descriptor_set(&gpu_context, &descriptor_sets[i]);
    }
    gpu_destroy_buffer(&gpu_context, &cube_vertex_buffer);

	static_model_free(&gpu_context, &static_model);
	animated_model_free(&gpu_context, &animated_model);

    gpu_destroy_context(&gpu_context);

    gui_shutdown(&gui_context);

    return 0;
}
