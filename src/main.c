#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// #define DISABLE_LOG
#ifdef DISABLE_LOG
#define printf(...) 
#endif

#include "stretchy_buffer.h"
#include "vec.h"
#include "matrix.h"
#include "quat.h"
#include "input.h"

#include "window.c"
#include "gpu.c"

#include "gltf.h"
#include "physics.h"
#include "gui.h"

bool read_file(const char* filename, size_t* out_file_size, uint32_t** out_data) {
	
	FILE *file = fopen (filename, "rb");
	if(!file) return false;

	fseek(file, 0L, SEEK_END);
	const size_t file_size = *out_file_size = ftell(file);
	rewind(file);

	*out_data = calloc(1, file_size+1);
	if (!*out_data) {
		fclose(file);
		return false;
	}

	if (fread(*out_data, 1, file_size, file) != file_size) {
		fclose(file);
		free(*out_data);
		return false;
	}

	fclose(file);
	return true;
}

GpuShaderModule create_shader_module_from_file(GpuContext* gpu_context, const char* filename) {
	size_t shader_size = 0;
	uint32_t* shader_code = NULL;
	if (!read_file(filename, &shader_size, &shader_code)) {
		exit(1);
	}

	GpuShaderModule module = gpu_create_shader_module(gpu_context, shader_size, shader_code);
	free(shader_code);
	return module;
}

typedef struct Vertex {
	Vec3 position;
	Vec3 normal;
	Vec4 color;
	float uv[2];
} Vertex;

//Computes all possible triangles (as vertices) for a convex solid, returned in a stretchy buffer
Vertex* compute_all_triangles(Vec3* in_vertices, Vec4 in_color) {
    Vertex* out_verts = NULL;
	uint32_t num_verts = sb_count(in_vertices);

	Vec3 center = vec3_new(0,0,0);
    for (size_t i = 0; i < num_verts; i++) {
        center.x += in_vertices[i].x;
        center.y += in_vertices[i].y;
        center.z += in_vertices[i].z;
    }
    center.x /= num_verts;
    center.y /= num_verts;
    center.z /= num_verts;

    for (int i = 0; i < num_verts - 2; ++i) {
        for (int j = i + 1; j < num_verts - 1; ++j) {
            for (int k = j + 1; k < num_verts; ++k) {
            	Vec3 pos_i = in_vertices[i];
                Vec3 pos_j = in_vertices[j];
                Vec3 pos_k = in_vertices[k];

				//Compute normal
				const Vec3 j_min_i = vec3_sub(pos_j, pos_i);
				const Vec3 k_min_i = vec3_sub(pos_k, pos_i);
				Vec3 normal = vec3_cross(j_min_i, k_min_i);
				normal = vec3_normalize(normal); //FIXME: make normal always face outwards

				Vec3 center_to_i = vec3_sub(pos_i, center);
				center_to_i = vec3_normalize(center_to_i);

				if (vec3_dot(normal, center_to_i) < 0) {
					normal = vec3_negate(normal);
				}

				Vertex v = {
					.position = pos_i,
					.normal = normal,
					.color = in_color,
					.uv = {0,0},
				};
				sb_push(out_verts, v);
				v.position = pos_j;
				v.uv[0] = 1;
				sb_push(out_verts, v);
				v.position = pos_k;
				v.uv[1] = 1;
				sb_push(out_verts, v);
            }
        }
    }
	return out_verts;
}

float rand_float(float lower_bound, float upper_bound) {
	return lower_bound + (float)(rand()) /( (float)(RAND_MAX/(upper_bound-lower_bound)));
}

int main() {

	Collider ground_collider = make_cube_collider();
	
	Collider* colliders = NULL;

	//Ground
	ground_collider.scale = vec3_new(100,1,100);
	ground_collider.position = vec3_new(0, -5, 0);
	ground_collider.is_kinematic = false;
	sb_push(colliders, ground_collider);

	const uint32_t dimensions = 4;
	for (int32_t x = 0; x < dimensions; ++x) {
		for (int32_t y = 0; y < dimensions; ++y) {
			for (int32_t z = 0; z < dimensions; ++z) {		
				Collider collider = make_cube_collider();

				// collider.rotation = quat_new(vec3_new(1,0,0), 3.14/4);

				const float spacing = 2.05f;
				const float y_offset = 2.0f;
				collider.position = vec3_scale(vec3_new(x, y + y_offset, z), spacing);
				sb_push(colliders, collider);
			}
		}
	}

    GltfAsset gltf_asset = {};
    if (!gltf_load_asset("data/meshes/monkey.glb", &gltf_asset)) { 
		printf("Failed to Load GLTF Asset\n");
		return 1; 
	}

	if (gltf_asset.num_meshes <= 0 || gltf_asset.meshes[0].num_primitives <= 0) {
		exit(0);
	}

	GltfPrimitive* primitive = &gltf_asset.meshes[0].primitives[0];

	uint8_t* positions_buffer = primitive->positions->buffer_view->buffer->data;
	positions_buffer += gltf_accessor_get_initial_offset(primitive->positions);
	uint32_t positions_byte_stride = gltf_accessor_get_stride(primitive->positions);

	uint8_t* normals_buffer = primitive->normals->buffer_view->buffer->data;
	normals_buffer += gltf_accessor_get_initial_offset(primitive->normals);
	uint32_t normals_byte_stride = gltf_accessor_get_stride(primitive->normals);
	
	uint8_t* uvs_buffer = primitive->texcoord0->buffer_view->buffer->data;
	uvs_buffer += gltf_accessor_get_initial_offset(primitive->texcoord0);
	uint32_t uvs_byte_stride = gltf_accessor_get_stride(primitive->texcoord0);

	uint32_t vertices_count = primitive->positions->count;
	Vertex* vertices = calloc(vertices_count, sizeof(Vertex));

	for (int i = 0; i < vertices_count; ++i) {
		memcpy(&vertices[i].position, positions_buffer, positions_byte_stride);
		memcpy(&vertices[i].normal, normals_buffer, normals_byte_stride);
		memcpy(&vertices[i].color, (float[4]){0.8f, 0.2f, 0.2f, 1.0f}, sizeof(float) * 4);
		memcpy(vertices[i].uv, uvs_buffer, uvs_byte_stride);
		
		positions_buffer += positions_byte_stride;
		normals_buffer += normals_byte_stride;
		uvs_buffer += uvs_byte_stride;
	}

	uint8_t* indices_buffer = primitive->indices->buffer_view->buffer->data;
	indices_buffer += gltf_accessor_get_initial_offset(primitive->indices);
	uint32_t indices_byte_stride = gltf_accessor_get_stride(primitive->indices);

	uint32_t indices_count = primitive->indices->count;
	uint32_t* indices = calloc(indices_count, sizeof(uint32_t));

	for (int i = 0; i < indices_count; ++i) {
		memcpy(&indices[i], indices_buffer, indices_byte_stride);
		indices_buffer += indices_byte_stride;
	}

	Window window = window_create("C Game", 1280, 720);

	GpuContext gpu_context = gpu_create_context(&window);

	//BEGIN GUI SETUP 

	GuiContext gui_context;

	GuiFont gui_font = {};
	if (!gui_load_font("data/fonts/JetBrainsMonoLight.bff", &gui_font)) {
		printf("failed to load font\n");
		exit(0);
	}

	char c = 'A';
	CharUVs char_uvs;
	if (gui_font_get_uvs(&gui_font, c, &char_uvs)) {
		printf("FOUND UVs: char: %c u1: %f, v1: %f, u2: %f, v2: %f,\n", 
			c, char_uvs.min_u, char_uvs.min_v, char_uvs.max_u, char_uvs.max_v
		);
	}

	assert(gui_font.font_type == FONT_TYPE_RGBA);

	//FCS TODO: Create Font Texture
	GpuImage font_image = gpu_create_image(&gpu_context, &(GpuImageCreateInfo) {
		.dimensions = { gui_font.image_width, gui_font.image_height, 1},
		.format = GPU_FORMAT_RGBA8_UNORM, //FCS TODO: Check from gui_font (remove assert on font type above)
		.mip_levels = 1,
		.usage = GPU_IMAGE_USAGE_SAMPLED | GPU_IMAGE_USAGE_TRANSFER_DST,
		.memory_properties = GPU_MEMORY_PROPERTY_DEVICE_LOCAL,
	}, "font_image");

	gpu_upload_image(&gpu_context, &font_image, gui_font.image_width, gui_font.image_height, gui_font.image_data_sb);

	GpuImageView font_image_view = gpu_create_image_view(&gpu_context, &(GpuImageViewCreateInfo) {
		.image = &font_image,
		.type = GPU_IMAGE_VIEW_2D,
		.format = GPU_FORMAT_RGBA8_UNORM,
		.aspect = GPU_IMAGE_ASPECT_COLOR,
	});

	GpuSampler font_sampler = gpu_create_sampler(&gpu_context, &(GpuSamplerCreateInfo) {
		.mag_filter = GPU_FILTER_NEAREST,
		.min_filter = GPU_FILTER_NEAREST,
		.max_anisotropy = 16,
	});

	GpuDescriptorLayout gui_descriptor_layout = {
		.binding_count = 1,
		.bindings = (GpuDescriptorBinding[1]){
			{
				.binding = 1,
				.type = GPU_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.stage_flags = GPU_SHADER_STAGE_ALL_GRAPHICS,
			},
		},
	};

	GpuPipelineLayout gui_pipeline_layout = gpu_create_pipeline_layout(&gpu_context, &gui_descriptor_layout);
	GpuDescriptorSet gui_descriptor_set = gpu_create_descriptor_set(&gpu_context, &gui_pipeline_layout);

	GpuDescriptorWrite gui_descriptor_writes[1] = {
		{
			.binding_desc = &gui_pipeline_layout.descriptor_layout.bindings[0],
			.image_write = &(GpuDescriptorWriteImage) {
				.image_view = &font_image_view,
				.sampler = &font_sampler,
				.layout = GPU_IMAGE_LAYOUT_SHADER_READ_ONLY,
			},
		}
	};

	gpu_write_descriptor_set(&gpu_context, &gui_descriptor_set, 1, gui_descriptor_writes);

	GpuRenderPass gui_render_pass = gpu_create_render_pass(&gpu_context, 1, 
		(GpuAttachmentDesc[1]){{	
			.format = (GpuFormat) gpu_context.surface_format.format, //FIXME: Store and Get from Context (as a GpuFormat)
			.load_op = GPU_LOAD_OP_CLEAR,
			.store_op = GPU_STORE_OP_STORE,
			.initial_layout = GPU_IMAGE_LAYOUT_UNDEFINED,
			.final_layout = GPU_IMAGE_LAYOUT_PRESENT_SRC,
		}},
		&(GpuAttachmentDesc){ //Depth Attachment FCS TODO: Does GUI Need depth?
			.format = GPU_FORMAT_D32_SFLOAT,
			.load_op = GPU_LOAD_OP_CLEAR,
			.store_op = GPU_STORE_OP_DONT_CARE,
			.initial_layout = GPU_IMAGE_LAYOUT_UNDEFINED,
			.final_layout = GPU_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT,
		});

	GpuShaderModule gui_vertex_module = create_shader_module_from_file(&gpu_context, "data/shaders/gui.vert.spv");
	GpuShaderModule gui_fragment_module = create_shader_module_from_file(&gpu_context, "data/shaders/gui.frag.spv");

	GpuFormat gui_attribute_formats[] = {
		GPU_FORMAT_RG32_SFLOAT,   // Position
		GPU_FORMAT_RG32_SFLOAT,   // UVs
		GPU_FORMAT_RGBA32_SFLOAT, // Color
	};

	GpuGraphicsPipelineCreateInfo gui_graphics_pipeline_create_info = {
		.vertex_module = &gui_vertex_module,
		.fragment_module = &gui_fragment_module,
		.render_pass = &gui_render_pass,
		.layout = &gui_pipeline_layout,
		.num_attributes = 3,
		.attribute_formats = gui_attribute_formats,
		.depth_stencil = {
			.depth_test = true,
			.depth_write = true,
		}
	};

	GpuPipeline gui_pipeline = gpu_create_graphics_pipeline(&gpu_context, &gui_graphics_pipeline_create_info);

	//Can destroy shader modules after creating pipeline
	gpu_destroy_shader_module(&gpu_context, &gui_vertex_module);
	gpu_destroy_shader_module(&gpu_context, &gui_fragment_module);

	//TODO: Generate a test quad (using gui_font_get_uvs)

	//TODO: gui_init function (takes in font)

	//END GUI SETUP

	size_t vertices_size = sizeof(Vertex) * vertices_count;
	GpuBufferUsageFlags vertex_buffer_usage = GPU_BUFFER_USAGE_VERTEX_BUFFER | GPU_BUFFER_USAGE_TRANSFER_DST;
	GpuBuffer vertex_buffer = gpu_create_buffer(&gpu_context, vertex_buffer_usage, GPU_MEMORY_PROPERTY_DEVICE_LOCAL, vertices_size, "mesh vertex buffer");
	gpu_upload_buffer(&gpu_context, &vertex_buffer, vertices_size, vertices);
	free(vertices);

	size_t indices_size = sizeof(uint32_t) * indices_count;
	GpuBufferUsageFlags index_buffer_usage = GPU_BUFFER_USAGE_INDEX_BUFFER | GPU_BUFFER_USAGE_TRANSFER_DST;
	GpuBuffer index_buffer = gpu_create_buffer(&gpu_context, index_buffer_usage, GPU_MEMORY_PROPERTY_DEVICE_LOCAL, indices_size, "mesh index buffer");
	gpu_upload_buffer(&gpu_context, &index_buffer, indices_size, indices);
	free(indices);

	typedef struct UniformStruct {
		Mat4  model;
		Mat4  mvp;
		Vec4 eye;
		Vec4 light_dir;
	} UniformStruct;

	UniformStruct uniform_data = {
		.eye = vec4_new(0.0f, 0.0f, 0.0f, 1.0f),
		.light_dir = vec4_new(0.0f, -1.0f, 1.0f, 0.0f),
	};

	GpuBuffer uniform_buffers[gpu_context.swapchain_image_count];
	for (uint32_t i = 0; i < gpu_context.swapchain_image_count; ++i)
	{
		uniform_buffers[i] = gpu_create_buffer(&gpu_context, GPU_BUFFER_USAGE_UNIFORM_BUFFER
											    , GPU_MEMORY_PROPERTY_DEVICE_LOCAL | GPU_MEMORY_PROPERTY_HOST_VISIBLE | GPU_MEMORY_PROPERTY_HOST_COHERENT
												, sizeof(UniformStruct)
												, "uniform_buffer");
	}

	GpuRenderPass render_pass = gpu_create_render_pass(&gpu_context, 1, 
		(GpuAttachmentDesc[1]){{	
			.format = (GpuFormat) gpu_context.surface_format.format, //FIXME: Store and Get from Context (as a GpuFormat)
			.load_op = GPU_LOAD_OP_CLEAR,
			.store_op = GPU_STORE_OP_STORE,
			.initial_layout = GPU_IMAGE_LAYOUT_UNDEFINED,
			.final_layout = GPU_IMAGE_LAYOUT_PRESENT_SRC,
		}},
		&(GpuAttachmentDesc){ //Depth Attachment
			.format = GPU_FORMAT_D32_SFLOAT,
			.load_op = GPU_LOAD_OP_CLEAR,
			.store_op = GPU_STORE_OP_DONT_CARE,
			.initial_layout = GPU_IMAGE_LAYOUT_UNDEFINED,
			.final_layout = GPU_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT,
		});

	GpuShaderModule vertex_module = create_shader_module_from_file(&gpu_context, "data/shaders/basic.vert.spv");
	GpuShaderModule fragment_module = create_shader_module_from_file(&gpu_context, "data/shaders/basic.frag.spv");
	
	GpuDescriptorLayout descriptor_layout = {
		.binding_count = 2,
		.bindings = (GpuDescriptorBinding[2]){
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

	for (uint32_t i = 0; i < gpu_context.swapchain_image_count; ++i)
	{
		descriptor_sets[i] = gpu_create_descriptor_set(&gpu_context, &pipeline_layout);

		GpuDescriptorWrite descriptor_writes[2] = {
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
				.image_write = &(GpuDescriptorWriteImage) {
					.image_view = &font_image_view,
					.sampler = &font_sampler,
					.layout = GPU_IMAGE_LAYOUT_SHADER_READ_ONLY,
				},
			}
		};

		gpu_write_descriptor_set(&gpu_context, &descriptor_sets[i], 2, descriptor_writes);
	}


	//BEGIN COLLIDERS GPU DATA SETUP
	Vertex* cube_render_vertices = compute_all_triangles(ground_collider.convex_points, vec4_new(0,0.5, 0.0, 1.0));
	uint32_t cube_vertices_count = sb_count(cube_render_vertices);
	size_t cube_vertices_size = sizeof(Vertex) * cube_vertices_count;
	GpuBuffer cube_vertex_buffer = gpu_create_buffer(&gpu_context, vertex_buffer_usage, GPU_MEMORY_PROPERTY_DEVICE_LOCAL, cube_vertices_size, "cube vertex buffer");
	gpu_upload_buffer(&gpu_context, &cube_vertex_buffer, cube_vertices_size, cube_render_vertices);
	sb_free(cube_render_vertices);
	
	GpuBuffer* collider_uniform_buffers = NULL;
	sb_add(collider_uniform_buffers, sb_count(colliders));
	GpuDescriptorSet* collider_descriptor_sets = NULL;
	sb_add(collider_descriptor_sets, sb_count(colliders));
	
	for (uint32_t i = 0; i < sb_count(collider_uniform_buffers); ++i) {
		char debug_name[50];
		sprintf(debug_name, "collider uniform buffer #%i", i);
		collider_uniform_buffers[i] = gpu_create_buffer(&gpu_context, GPU_BUFFER_USAGE_UNIFORM_BUFFER
											, GPU_MEMORY_PROPERTY_DEVICE_LOCAL | GPU_MEMORY_PROPERTY_HOST_VISIBLE | GPU_MEMORY_PROPERTY_HOST_COHERENT
											, sizeof(UniformStruct)
											, debug_name);

		collider_descriptor_sets[i] = gpu_create_descriptor_set(&gpu_context, &pipeline_layout);

		GpuDescriptorWrite collider_descriptor_writes[2] = {
			{
				.binding_desc = &pipeline_layout.descriptor_layout.bindings[0],
				.buffer_write = &(GpuDescriptorWriteBuffer) {
					.buffer = &collider_uniform_buffers[i],
					.offset = 0,
					.range = sizeof(uniform_data),
				}
			},
			{
				.binding_desc = &pipeline_layout.descriptor_layout.bindings[1],
				.image_write = &(GpuDescriptorWriteImage) {
					.image_view = &font_image_view,
					.sampler = &font_sampler,
					.layout = GPU_IMAGE_LAYOUT_SHADER_READ_ONLY,
				},
			}
		};

		gpu_write_descriptor_set(&gpu_context, &collider_descriptor_sets[i], 2, collider_descriptor_writes);
	}
	//END COLLIDERS GPU DATA SETUP

	GpuFormat attribute_formats[] = {
		GPU_FORMAT_RGB32_SFLOAT,
		GPU_FORMAT_RGB32_SFLOAT,
		GPU_FORMAT_RGBA32_SFLOAT,
		GPU_FORMAT_RG32_SFLOAT,
	};

	GpuGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
		.vertex_module = &vertex_module,
		.fragment_module = &fragment_module,
		.render_pass = &render_pass,
		.layout = &pipeline_layout,
		.num_attributes = 4,
		.attribute_formats = attribute_formats,
		.depth_stencil = {
			.depth_test = true,
			.depth_write = true,
		}
	};

	GpuPipeline pipeline = gpu_create_graphics_pipeline(&gpu_context, &graphics_pipeline_create_info);

	//destroy shader modules after creating pipeline
	gpu_destroy_shader_module(&gpu_context, &vertex_module);
	gpu_destroy_shader_module(&gpu_context, &fragment_module);

    clock_t time = clock();
	double accumulated_delta_time = 0.0f;
	size_t frames_rendered = 0;

	//Setup Per-Frame Resources
	GpuCommandBuffer command_buffers[gpu_context.swapchain_image_count];
	GpuSemaphore image_acquired_semaphores[gpu_context.swapchain_image_count];
	GpuSemaphore render_complete_semaphores[gpu_context.swapchain_image_count];
	GpuFence in_flight_fences[gpu_context.swapchain_image_count];
	for (uint32_t i = 0; i < gpu_context.swapchain_image_count; ++i) {
		command_buffers[i] = gpu_create_command_buffer(&gpu_context);
		image_acquired_semaphores[i] = gpu_create_semaphore(&gpu_context);
		render_complete_semaphores[i] = gpu_create_semaphore(&gpu_context);
		in_flight_fences[i] = gpu_create_fence(&gpu_context, true);
	}

	Quat orientation = quat_identity;

	Vec3 position = vec3_new(0,5,-30);
	Vec3 target = vec3_new(0,5,-50);
	Vec3 up = vec3_new(0,1,0);

	//These are set up on startup / resize
	GpuImage depth_image = {};
	GpuImageView depth_view = {};
	GpuFramebuffer framebuffers[gpu_context.swapchain_image_count];
	memset(framebuffers, 0, sizeof(framebuffers));

	int width = 0;
	int height = 0;

	uint32_t current_frame = 0;
	while (window_handle_messages(&window)) {

		if (input_pressed(KEY_ESCAPE)) break;

		int new_width, new_height;
		window_get_dimensions(&window, &new_width, &new_height);

		if (new_width == 0 || new_height == 0) {
			continue;
		}
		
		if (new_width != width || new_height != height) {
			gpu_resize_swapchain(&gpu_context, &window);
			width = new_width;
			height = new_height;
			
			for (size_t i = 0; i < gpu_context.swapchain_image_count; ++i) {
				gpu_destroy_framebuffer(&gpu_context, &framebuffers[i]);
			}

			gpu_destroy_image_view(&gpu_context, &depth_view);
			gpu_destroy_image(&gpu_context, &depth_image);

			depth_image = gpu_create_image(&gpu_context, &(GpuImageCreateInfo) {
				.dimensions = {width, height, 1},
				.format = GPU_FORMAT_D32_SFLOAT,
				.mip_levels = 1,
				.usage = GPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT,
				.memory_properties = GPU_MEMORY_PROPERTY_DEVICE_LOCAL,
			}, "depth_image");

			depth_view = gpu_create_image_view(&gpu_context, &(GpuImageViewCreateInfo) {
				.image = &depth_image,
				.type = GPU_IMAGE_VIEW_2D,
				.format = GPU_FORMAT_D32_SFLOAT,
				.aspect = GPU_IMAGE_ASPECT_DEPTH,
			});

			for (size_t i = 0; i < gpu_context.swapchain_image_count; ++i) {
				GpuFramebufferCreateInfo new_framebuffer_create_info = {
					.render_pass = &render_pass,
					.width = width,
					.height = height,
					.attachment_count = 2,
					.attachments = (GpuImageView[2]){gpu_context.swapchain_image_views[i], depth_view},
				};

				framebuffers[i] = gpu_create_framebuffer(&gpu_context, &new_framebuffer_create_info);
			}
		}

		//BEGIN Gui Test

		int32_t mouse_x, mouse_y;
		window_get_mouse_pos(&window, &mouse_x, &mouse_y);

		GuiFrameState gui_frame_state = {
			.res_x = width,
			.res_y = height,
			.mouse_x= mouse_x,
			.mouse_y = mouse_y,
			.mouse_buttons = {input_pressed(KEY_LEFT_MOUSE), input_pressed(KEY_RIGHT_MOUSE), input_pressed(KEY_MIDDLE_MOUSE)}
		};

		gui_begin_frame(&gui_context, gui_frame_state);

		if (gui_button(&gui_context, "My Button", 0, 0, 40, 40)) {
			printf("CLICKED BUTTON!\n");
		} else {
			printf("NO CLICK\n");
		}

		//END Gui Test

		gpu_wait_for_fence(&gpu_context, &in_flight_fences[current_frame]);
		gpu_reset_fence(&gpu_context, &in_flight_fences[current_frame]);

		uint32_t image_index = gpu_acquire_next_image(&gpu_context, &image_acquired_semaphores[current_frame]);

		gpu_begin_command_buffer(&command_buffers[current_frame]);
		gpu_cmd_begin_render_pass(&command_buffers[current_frame], &(GpuRenderPassBeginInfo) {
			.render_pass = &render_pass,
			.framebuffer = &framebuffers[image_index],
			.num_clear_values = 2,
			.clear_values = (GpuClearValue[2]){
				{
				.clear_color = { 0.392f, 0.584f, 0.929f, 0.0f},
				},
				{
					.depth_stencil = {
						.depth = 1.0f,
						.stencil = 0,
					}
				}
			},
		});
		gpu_cmd_bind_pipeline(&command_buffers[current_frame], &pipeline);
		gpu_cmd_set_viewport(&command_buffers[current_frame], &(GpuViewport) {
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
		gpu_cmd_draw_indexed(&command_buffers[current_frame], indices_count);

		for (uint32_t i = 0; i < sb_count(colliders); ++i) {
			//FIXME: one vertex buffer per collider (we're assuming all same-data (cubes) currently)
			gpu_cmd_bind_vertex_buffer(&command_buffers[current_frame], &cube_vertex_buffer);
			gpu_cmd_bind_descriptor_set(&command_buffers[current_frame], &pipeline_layout, &collider_descriptor_sets[i]);
			gpu_cmd_draw(&command_buffers[current_frame], cube_vertices_count);
		}

		gpu_cmd_end_render_pass(&command_buffers[current_frame]);
		gpu_end_command_buffer(&command_buffers[current_frame]);

		gpu_queue_submit(&gpu_context,
						 &command_buffers[current_frame], 
						 &image_acquired_semaphores[current_frame], 
						 &render_complete_semaphores[current_frame], 
						 &in_flight_fences[current_frame]);

		gpu_queue_present(&gpu_context, image_index, &render_complete_semaphores[current_frame]);

		clock_t new_time = clock();
		double delta_time = ((double) (new_time - time)) / CLOCKS_PER_SEC;
		time = new_time;

		accumulated_delta_time += delta_time;
		frames_rendered++;

		float move_speed = (input_pressed(KEY_SHIFT) ? 36.0 : 12.0) * delta_time;

		if (input_pressed('W')) {
			position.z += move_speed;
			target.z += move_speed;
		}
		if (input_pressed('S')) {
			position.z -= move_speed;
			target.z -= move_speed;
		}
		if (input_pressed('A')) {
			position.x -= move_speed;
			target.x -= move_speed;
		}
		if (input_pressed('D')) {
			position.x += move_speed;
			target.x += move_speed;
		}
		if (input_pressed('Q')) {
			position.y -= move_speed;
			target.y -= move_speed;
		}
		if (input_pressed('E')) {
			position.y += move_speed;
			target.y += move_speed;
		}

		{
			float rotation_speed = 1.0f;
			Quat q1 = quat_new(vec3_new(0,1,0),delta_time * rotation_speed);
			Quat q2 = quat_new(vec3_new(1,0,0),delta_time * rotation_speed);

			Quat q = quat_normalize(quat_mult(q1, q2));

			orientation = quat_mult(orientation, q);
			orientation = quat_normalize(orientation);

			Mat4 model = quat_to_mat4(orientation);
			// Mat4 model = mat4_mult_mat4(model, rotation);
			Mat4 view = mat4_look_at(position, target, up);
			Mat4 proj = mat4_perspective(60.0f, (float) width / (float) height, 0.01f, 1000.0f);

			Mat4 mv = mat4_mult_mat4(model, view);
			uniform_data.model = model;
			uniform_data.mvp = mat4_mult_mat4(mv, proj);

			memcpy(&uniform_data.eye, &position, sizeof(float) * 3);

			gpu_upload_buffer(&gpu_context, &uniform_buffers[current_frame], sizeof(uniform_data), &uniform_data);

			//BEGIN COLLIDERS GPU DATA UPDATE
			if (input_pressed(KEY_SPACE)) {
				for (int32_t i = 1; i < sb_count(colliders); ++i) {
					Vec3 collider_position = colliders[i].position;
					// Vec3 force = vec3_scale(vec3_negate(vec3_normalize(collider_position)), 10.0f);
					// if (input_pressed(KEY_SPACE)) {
					// 	physics_add_force(&colliders[i], force);
					// }

					physics_add_force(&colliders[i], vec3_new(0,-10 * colliders[i].mass, 0));
				}
				physics_run_simulation(colliders, delta_time);
			}

			for (uint32_t i = 0; i < sb_count(collider_uniform_buffers); ++i) {
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

				gpu_upload_buffer(&gpu_context, &collider_uniform_buffers[i], sizeof(collider_uniform_data), &collider_uniform_data);
			}
			//END COLLIDERS GPU DATA UPDATE
		}

		current_frame = (current_frame + 1) % gpu_context.swapchain_image_count;
	}

	double average_delta_time = accumulated_delta_time / (double) frames_rendered;
	printf("Average Delta Time = %f \n", average_delta_time);
	printf("Average FPS = %f \n", 1 / average_delta_time);

	gpu_wait_idle(&gpu_context);

	for (uint32_t i = 0; i < gpu_context.swapchain_image_count; ++i) {
		gpu_free_command_buffer(&gpu_context, &command_buffers[i]);
		gpu_destroy_semaphore(&gpu_context, &image_acquired_semaphores[i]);
		gpu_destroy_semaphore(&gpu_context, &render_complete_semaphores[i]);
		gpu_destroy_fence(&gpu_context, &in_flight_fences[i]);
	}

	for (size_t i = 0; i < gpu_context.swapchain_image_count; ++i)
	{
		gpu_destroy_framebuffer(&gpu_context, &framebuffers[i]);
	}

	for (uint32_t i = 0; i < sb_count(colliders); ++i) {
		gpu_destroy_descriptor_set(&gpu_context, &collider_descriptor_sets[i]);
		gpu_destroy_buffer(&gpu_context, &collider_uniform_buffers[i]);
	}

	//BEGIN Gui Cleanup
	gpu_destroy_pipeline(&gpu_context, &gui_pipeline);
	gpu_destroy_descriptor_set(&gpu_context, &gui_descriptor_set);
	gpu_destroy_pipeline_layout(&gpu_context, &gui_pipeline_layout);
	gpu_destroy_render_pass(&gpu_context, &gui_render_pass);
	gpu_destroy_sampler(&gpu_context, &font_sampler);
	gpu_destroy_image_view(&gpu_context, &font_image_view);
	gpu_destroy_image(&gpu_context, &font_image);
	//END Gui Cleanup

	gpu_destroy_pipeline(&gpu_context, &pipeline);
	
	gpu_destroy_pipeline_layout(&gpu_context, &pipeline_layout);
	gpu_destroy_render_pass(&gpu_context, &render_pass);
	gpu_destroy_image_view(&gpu_context, &depth_view);
	gpu_destroy_image(&gpu_context, &depth_image);
	gpu_destroy_buffer(&gpu_context, &vertex_buffer);
	gpu_destroy_buffer(&gpu_context, &index_buffer);
	for (uint32_t i = 0; i < gpu_context.swapchain_image_count; ++i)
	{
		gpu_destroy_buffer(&gpu_context, &uniform_buffers[i]);
		gpu_destroy_descriptor_set(&gpu_context, &descriptor_sets[i]);
	}
	gpu_destroy_buffer(&gpu_context, &cube_vertex_buffer);
	gpu_destroy_context(&gpu_context);

	return 0;
}
