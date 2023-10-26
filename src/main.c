#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>

#include "timer.h"

// #define DISABLE_LOG
#ifdef DISABLE_LOG
#define printf(...)
#endif

#include "matrix.h"
#include "quat.h"
#include "stretchy_buffer.h"
#include "vec.h"

#include "gpu.c"
#include "window/window.h"

#include "gltf.h"
#include "animated_model.h"
#include "gui.h"
#include "physics.h"

// FCS TODO: Testing collision
#include "collision.h"

// FCS TODO: Testing truetype
#include "truetype.h"

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

typedef struct Vertex
{
    Vec3 position;
    Vec3 normal;
    Vec4 color;
    Vec2 uv;
} Vertex;

// Computes all possible triangles (as vertices) for a convex solid, returned in a stretchy buffer
Vertex *compute_all_triangles(Vec3 *in_vertices, Vec4 in_color)
{
    sbuffer(Vertex) out_verts = NULL;
    u32 num_verts = sb_count(in_vertices);

    Vec3 center = vec3_new(0, 0, 0);
    for (size_t i = 0; i < num_verts; i++)
    {
        center.x += in_vertices[i].x;
        center.y += in_vertices[i].y;
        center.z += in_vertices[i].z;
    }
    center.x /= num_verts;
    center.y /= num_verts;
    center.z /= num_verts;

    for (int i = 0; i < num_verts - 2; ++i)
    {
        for (int j = i + 1; j < num_verts - 1; ++j)
        {
            for (int k = j + 1; k < num_verts; ++k)
            {
                Vec3 pos_i = in_vertices[i];
                Vec3 pos_j = in_vertices[j];
                Vec3 pos_k = in_vertices[k];

                // Compute normal
                const Vec3 j_min_i = vec3_sub(pos_j, pos_i);
                const Vec3 k_min_i = vec3_sub(pos_k, pos_i);
                Vec3 normal = vec3_cross(j_min_i, k_min_i);
                normal = vec3_normalize(normal); // FIXME: make normal always face outwards

                Vec3 center_to_i = vec3_sub(pos_i, center);
                center_to_i = vec3_normalize(center_to_i);

                if (vec3_dot(normal, center_to_i) < 0)
                {
                    normal = vec3_negate(normal);
                }

                Vertex v = {
                    .position = pos_i,
                    .normal = normal,
                    .color = in_color,
                    .uv = vec2_new(0, 0),
                };
                sb_push(out_verts, v);
                v.position = pos_j;
                v.uv.x = 1;
                sb_push(out_verts, v);
                v.position = pos_k;
                v.uv.y = 1;
                sb_push(out_verts, v);
            }
        }
    }
    return out_verts;
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

    test_collision(); // FCS TODO: See Collision.h

    Collider ground_collider = make_cube_collider();

    Collider *colliders = NULL;

    // Ground
    ground_collider.scale = vec3_new(100, 1, 100);
    ground_collider.position = vec3_new(0, -5, 0);
    ground_collider.is_kinematic = false;
    sb_push(colliders, ground_collider);

    const u32 dimensions = 3;
    for (i32 x = 0; x < dimensions; ++x)
    {
        for (i32 y = 0; y < dimensions; ++y)
        {
            for (i32 z = 0; z < dimensions; ++z)
            {
                Collider collider = make_cube_collider();

                // collider.rotation = quat_new(vec3_new(1,0,0), 3.14/4);

                const float spacing = 2.05f;
                const float y_offset = 2.0f;
                collider.position = vec3_scale(vec3_new(x + 5, y + y_offset, z), spacing);
                sb_push(colliders, collider);
            }
        }
    }

	AnimatedModel animated_model;
	if (!animated_model_load("data/meshes/cesium_man.glb", &animated_model))
	{
		printf("Failed to Load Animated Model\n");
		return 1;
	}

	printf("Successfully loaded Animated Model\n");
	//exit(0);
	//FCS TODO: !!!
	//1. [DONE] set up new pipeline and get cesium man rendering properly as a static model
	//2. bake out animation in animated_model.h
	//3. Test primitive collapsing code on skinned and non-skinned meshes with multiple primitives

    Window window = window_create("C Game", 1920, 1080);

    GpuContext gpu_context = gpu_create_context(&window);

    size_t vertices_size = sizeof(AnimatedVertex) * animated_model.num_vertices;
    GpuBufferUsageFlags vertex_buffer_usage = GPU_BUFFER_USAGE_VERTEX_BUFFER | GPU_BUFFER_USAGE_TRANSFER_DST;
    GpuBuffer vertex_buffer = gpu_create_buffer(&gpu_context, vertex_buffer_usage, GPU_MEMORY_PROPERTY_DEVICE_LOCAL,
                                                vertices_size, "mesh vertex buffer");
    gpu_upload_buffer(&gpu_context, &vertex_buffer, vertices_size, animated_model.vertices);

    size_t indices_size = sizeof(u32) * animated_model.num_indices;
    GpuBufferUsageFlags index_buffer_usage = GPU_BUFFER_USAGE_INDEX_BUFFER | GPU_BUFFER_USAGE_TRANSFER_DST;
    GpuBuffer index_buffer = gpu_create_buffer(&gpu_context, index_buffer_usage, GPU_MEMORY_PROPERTY_DEVICE_LOCAL,
                                               indices_size, "mesh index buffer");
    gpu_upload_buffer(&gpu_context, &index_buffer, indices_size, animated_model.indices);

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

    GpuShaderModule gui_vertex_module = create_shader_module_from_file(&gpu_context, "data/shaders/gui.vert.spv");
    GpuShaderModule gui_fragment_module = create_shader_module_from_file(&gpu_context, "data/shaders/gui.frag.spv");

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

    size_t max_gui_vertices = sizeof(GuiVert) * 10000; // TODO: need to support gpu buffer resizing
    GpuBuffer gui_vertex_buffer = gpu_create_buffer(
        &gpu_context, GPU_BUFFER_USAGE_VERTEX_BUFFER,
        GPU_MEMORY_PROPERTY_DEVICE_LOCAL | GPU_MEMORY_PROPERTY_HOST_VISIBLE | GPU_MEMORY_PROPERTY_HOST_COHERENT,
        max_gui_vertices, "gui vertex buffer");

    size_t max_gui_indices = sizeof(u32) * 30000; // TODO: need to support gpu buffer resizing
    GpuBuffer gui_index_buffer = gpu_create_buffer(&gpu_context, GPU_BUFFER_USAGE_INDEX_BUFFER,
                                                   GPU_MEMORY_PROPERTY_DEVICE_LOCAL | GPU_MEMORY_PROPERTY_HOST_VISIBLE |
                                                       GPU_MEMORY_PROPERTY_HOST_COHERENT,
                                                   max_gui_indices, "gui index buffer");

    // END GUI SETUP

    typedef struct UniformStruct
    {
        Mat4 model;
        Mat4 mvp;
        Vec4 eye;
        Vec4 light_dir;
    } UniformStruct;

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
                                               sizeof(UniformStruct), "uniform_buffer");
    }

    GpuShaderModule vertex_module = create_shader_module_from_file(&gpu_context, "data/shaders/basic.vert.spv");
	GpuShaderModule skinned_vertex_module = create_shader_module_from_file(&gpu_context, "data/shaders/skinned.vert.spv");
    GpuShaderModule fragment_module = create_shader_module_from_file(&gpu_context, "data/shaders/basic.frag.spv");

    GpuDescriptorLayout descriptor_layout = {
        .binding_count = 2,
        .bindings =
            (GpuDescriptorBinding[2]){
                {
                    .binding = 0,
                    .type = GPU_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .stage_flags = GPU_SHADER_STAGE_ALL_GRAPHICS,
                },
                {
                    .binding = 1,
                    .type = GPU_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .stage_flags = GPU_SHADER_STAGE_ALL_GRAPHICS,
                },
            },
    };

    GpuPipelineLayout pipeline_layout = gpu_create_pipeline_layout(&gpu_context, &descriptor_layout);
    GpuDescriptorSet descriptor_sets[gpu_context.swapchain_image_count];

    for (u32 i = 0; i < gpu_context.swapchain_image_count; ++i)
    {
        descriptor_sets[i] = gpu_create_descriptor_set(&gpu_context, &pipeline_layout);

        GpuDescriptorWrite descriptor_writes[2] = {{.binding_desc = &pipeline_layout.descriptor_layout.bindings[0],
                                                    .buffer_write =
                                                        &(GpuDescriptorWriteBuffer){
                                                            .buffer = &uniform_buffers[i],
                                                            .offset = 0,
                                                            .range = sizeof(uniform_data),
                                                        }},
                                                   {
                                                       .binding_desc = &pipeline_layout.descriptor_layout.bindings[1],
                                                       .image_write =
                                                           &(GpuDescriptorWriteImage){
                                                               .image_view = &font_image_view,
                                                               .sampler = &font_sampler,
                                                               .layout = GPU_IMAGE_LAYOUT_SHADER_READ_ONLY,
                                                           },
                                                   }};

        gpu_write_descriptor_set(&gpu_context, &descriptor_sets[i], 2, descriptor_writes);
    }

    // BEGIN COLLIDERS GPU DATA SETUP
    sbuffer(Vertex) cube_render_vertices =
        compute_all_triangles(ground_collider.convex_points, vec4_new(0, 0.5, 0.0, 1.0));
    u32 cube_vertices_count = sb_count(cube_render_vertices);
    size_t cube_vertices_size = sizeof(Vertex) * cube_vertices_count;
    GpuBuffer cube_vertex_buffer = gpu_create_buffer(
        &gpu_context, vertex_buffer_usage, GPU_MEMORY_PROPERTY_DEVICE_LOCAL, cube_vertices_size, "cube vertex buffer");
    gpu_upload_buffer(&gpu_context, &cube_vertex_buffer, cube_vertices_size, cube_render_vertices);
    sb_free(cube_render_vertices);

    GpuBuffer *collider_uniform_buffers = NULL;
    sb_add(collider_uniform_buffers, sb_count(colliders));
    GpuDescriptorSet *collider_descriptor_sets = NULL;
    sb_add(collider_descriptor_sets, sb_count(colliders));

    for (u32 i = 0; i < sb_count(collider_uniform_buffers); ++i)
    {
        char debug_name[50];
        sprintf(debug_name, "collider uniform buffer #%i", i);
        collider_uniform_buffers[i] = gpu_create_buffer(
            &gpu_context, GPU_BUFFER_USAGE_UNIFORM_BUFFER,
            GPU_MEMORY_PROPERTY_DEVICE_LOCAL | GPU_MEMORY_PROPERTY_HOST_VISIBLE | GPU_MEMORY_PROPERTY_HOST_COHERENT,
            sizeof(UniformStruct), debug_name);

        collider_descriptor_sets[i] = gpu_create_descriptor_set(&gpu_context, &pipeline_layout);

        GpuDescriptorWrite collider_descriptor_writes[2] = {
            {.binding_desc = &pipeline_layout.descriptor_layout.bindings[0],
             .buffer_write =
                 &(GpuDescriptorWriteBuffer){
                     .buffer = &collider_uniform_buffers[i],
                     .offset = 0,
                     .range = sizeof(uniform_data),
                 }},
            {
                .binding_desc = &pipeline_layout.descriptor_layout.bindings[1],
                .image_write =
                    &(GpuDescriptorWriteImage){
                        .image_view = &font_image_view,
                        .sampler = &font_sampler,
                        .layout = GPU_IMAGE_LAYOUT_SHADER_READ_ONLY,
                    },
            }};

        gpu_write_descriptor_set(&gpu_context, &collider_descriptor_sets[i], 2, collider_descriptor_writes);
    }
    // END COLLIDERS GPU DATA SETUP

    GpuFormat static_attribute_formats[] = {
        GPU_FORMAT_RGB32_SFLOAT,
        GPU_FORMAT_RGB32_SFLOAT,
        GPU_FORMAT_RGBA32_SFLOAT,
        GPU_FORMAT_RG32_SFLOAT,
    };

    GpuGraphicsPipelineCreateInfo static_pipeline_create_info = {.vertex_module = &vertex_module,
                                                                   .fragment_module = &fragment_module,
                                                                   .rendering_info = &dynamic_rendering_info,
                                                                   .layout = &pipeline_layout,
                                                                   .num_attributes = 4,
                                                                   .attribute_formats = static_attribute_formats,
                                                                   .depth_stencil = {
                                                                       .depth_test = true,
                                                                       .depth_write = true,
                                                                   }};

    GpuPipeline static_pipeline = gpu_create_graphics_pipeline(&gpu_context, &static_pipeline_create_info);

	GpuFormat skinned_attribute_formats[] = {
		        GPU_FORMAT_RGB32_SFLOAT,
		        GPU_FORMAT_RGB32_SFLOAT,
		        GPU_FORMAT_RGBA32_SFLOAT,
		        GPU_FORMAT_RG32_SFLOAT,
				GPU_FORMAT_RGBA32_SFLOAT, //should be int?
				GPU_FORMAT_RGBA32_SFLOAT  //should be int?
		    };

    GpuGraphicsPipelineCreateInfo skinned_pipeline_create_info = {.vertex_module = &skinned_vertex_module,
                                                                   .fragment_module = &fragment_module,
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
    gpu_destroy_shader_module(&gpu_context, &vertex_module);
    gpu_destroy_shader_module(&gpu_context, &fragment_module);

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

    Vec3 position = vec3_new(0, 5, -30);
    Vec3 target = vec3_new(0, 5, -50);
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

        static float rotation_rate = 1.25f;
        gui_window_slider_float(&gui_context, &gui_window_1, &rotation_rate, vec2_new(-25.0, 25.0), "Rot Rate");
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

		{	// Draw Our Animated Model
			gpu_cmd_bind_pipeline(&command_buffers[current_frame], &skinned_pipeline);
			gpu_cmd_set_viewport(&command_buffers[current_frame], &(GpuViewport){
				.x = 0,
				.y = 0,
				.width = width,
				.height = height,
				.min_depth = 0.0,
				.max_depth = 1.0,
			});
			gpu_cmd_bind_index_buffer(&command_buffers[current_frame], &index_buffer);
			gpu_cmd_bind_vertex_buffer(&command_buffers[current_frame], &vertex_buffer);
			gpu_cmd_bind_descriptor_set(&command_buffers[current_frame], &pipeline_layout, &descriptor_sets[current_frame]);
			gpu_cmd_draw_indexed(&command_buffers[current_frame], animated_model.num_indices);
		}

		{	// Draw Our Static Geometry
			gpu_cmd_bind_pipeline(&command_buffers[current_frame], &static_pipeline);
			for (u32 i = 0; i < sb_count(colliders); ++i)
			{
				// FIXME: one vertex buffer per collider (we're assuming all same-data (cubes) currently)
				gpu_cmd_bind_vertex_buffer(&command_buffers[current_frame], &cube_vertex_buffer);
				gpu_cmd_bind_descriptor_set(&command_buffers[current_frame], &pipeline_layout,
								&collider_descriptor_sets[i]);
				gpu_cmd_draw(&command_buffers[current_frame], cube_vertices_count);
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
        gpu_cmd_draw_indexed(&command_buffers[current_frame], sb_count(gui_context.draw_data.indices));
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

        {
            // float rotation_rate = 1.0f;
            //Quat q1 = quat_new(vec3_new(0, 1, 0), delta_time * rotation_rate);
            //Quat q2 = quat_new(vec3_new(1, 0, 0), delta_time * rotation_rate);

            Quat q1 = quat_new(vec3_new(0, 1, 0), 90 * DEGREES_TO_RADIANS);
            Quat q2 = quat_new(vec3_new(1, 0, 0), 180 * DEGREES_TO_RADIANS);
			Quat q3 = quat_new(vec3_new(0, 0, 1), 90 * DEGREES_TO_RADIANS);

            Quat q = quat_normalize(quat_mult(quat_mult(q1, q2), q3));

            orientation = q; //quat_mult(orientation, q);
            orientation = quat_normalize(orientation);

			Vec3 scale = vec3_new(5,5,5);
			Mat4 scale_matrix = mat4_scale(scale);
			Mat4 orientation_matrix = quat_to_mat4(orientation);

            Mat4 model = mat4_mult_mat4(scale_matrix, orientation_matrix); 
            Mat4 view = mat4_look_at(position, target, up);
            Mat4 proj = mat4_perspective(60.0f, (float)width / (float)height, 0.01f, 1000.0f);

            Mat4 mv = mat4_mult_mat4(model, view);
            uniform_data.model = model;
            uniform_data.mvp = mat4_mult_mat4(mv, proj);

            memcpy(&uniform_data.eye, &position, sizeof(float) * 3);

            gpu_upload_buffer(&gpu_context, &uniform_buffers[current_frame], sizeof(uniform_data), &uniform_data);

            // BEGIN COLLIDERS GPU DATA UPDATE
            if (input_pressed(KEY_SPACE))
            {
                for (i32 i = 1; i < sb_count(colliders); ++i)
                {
                    Vec3 collider_position = colliders[i].position;
                    // Vec3 force = vec3_scale(vec3_negate(vec3_normalize(collider_position)), 10.0f);
                    // if (input_pressed(KEY_SPACE)) {
                    // 	physics_add_force(&colliders[i], force);
                    // }

                    physics_add_force(&colliders[i], vec3_new(0, -10 * colliders[i].mass, 0));
                }
                physics_run_simulation(colliders, delta_time);
            }

            for (u32 i = 0; i < sb_count(collider_uniform_buffers); ++i)
            {
                Mat4 collider_scale = mat4_scale(colliders[i].scale);
                Mat4 collider_rotation = quat_to_mat4(colliders[i].rotation);
                Mat4 collider_translation = mat4_translation(colliders[i].position);
                Mat4 scale_rot = mat4_mult_mat4(collider_scale, collider_rotation);
                Mat4 collider_transform = mat4_mult_mat4(scale_rot, collider_translation);
                Mat4 collider_mv = mat4_mult_mat4(collider_transform, view);
                Mat4 collider_mvp = mat4_mult_mat4(collider_mv, proj);

                UniformStruct collider_uniform_data = {
                    .model = collider_transform,
                    .mvp = collider_mvp,
                    .eye = uniform_data.eye,
                    .light_dir = uniform_data.light_dir,
                };

                gpu_upload_buffer(&gpu_context, &collider_uniform_buffers[i], sizeof(collider_uniform_data),
                                  &collider_uniform_data);
            }
            // END COLLIDERS GPU DATA UPDATE
        }

        current_frame = (current_frame + 1) % gpu_context.swapchain_image_count;

        gpu_wait_idle(&gpu_context); // TODO: Need per-frame resources (gui vbuf,ibuf all uniform buffers)
    }

    gpu_wait_idle(&gpu_context);

    for (u32 i = 0; i < gpu_context.swapchain_image_count; ++i)
    {
        gpu_free_command_buffer(&gpu_context, &command_buffers[i]);
        gpu_destroy_semaphore(&gpu_context, &image_acquired_semaphores[i]);
        gpu_destroy_semaphore(&gpu_context, &render_complete_semaphores[i]);
        gpu_destroy_fence(&gpu_context, &in_flight_fences[i]);
    }

    for (u32 i = 0; i < sb_count(colliders); ++i)
    {
        gpu_destroy_descriptor_set(&gpu_context, &collider_descriptor_sets[i]);
        gpu_destroy_buffer(&gpu_context, &collider_uniform_buffers[i]);
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
	gpu_destroy_pipeline(&gpu_context, &skinned_pipeline);

    gpu_destroy_pipeline_layout(&gpu_context, &pipeline_layout);
    gpu_destroy_image_view(&gpu_context, &depth_view);
    gpu_destroy_image(&gpu_context, &depth_image);
    gpu_destroy_buffer(&gpu_context, &vertex_buffer);
    gpu_destroy_buffer(&gpu_context, &index_buffer);
    for (u32 i = 0; i < gpu_context.swapchain_image_count; ++i)
    {
        gpu_destroy_buffer(&gpu_context, &uniform_buffers[i]);
        gpu_destroy_descriptor_set(&gpu_context, &descriptor_sets[i]);
    }
    gpu_destroy_buffer(&gpu_context, &cube_vertex_buffer);
    gpu_destroy_context(&gpu_context);

    gui_shutdown(&gui_context);

	//gltf_free_asset(&gltf_asset);
	animated_model_free(&animated_model);

    return 0;
}
