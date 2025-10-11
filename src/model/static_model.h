#pragma once

#include "gltf.h"
#include "gpu2/gpu2.h"
#include "types.h"
#include "assert.h"

typedef struct StaticVertex
{
    Vec4 position;
    Vec4 normal;
    Vec4 color;
    Vec2 uv;
	Vec2 padding;
} StaticVertex;

typedef struct StaticModel
{
    GltfAsset gltf_asset;

    i32 num_vertices;
    StaticVertex* vertices;

    i32 num_indices;
    u32* indices;

    Gpu2Buffer vertex_buffer;
    Gpu2Buffer index_buffer;
} StaticModel;

Mat4 compute_static_node_transform(GltfNode* target_node, GltfNode* nodes_array, i32 num_nodes)
{
	assert(target_node);

	const Mat4 parent_transform = target_node->parent != NULL 
								? compute_static_node_transform(target_node->parent, nodes_array, num_nodes) 
								: mat4_identity;

	const GltfTransform local_transform = target_node->transform;	
	const Mat4 local_transform_matrix = gltf_transform_to_mat4(&local_transform);
	const Mat4 computed_transform = mat4_mul_mat4(local_transform_matrix, parent_transform); 
	return computed_transform;
}

bool static_model_load(const char* gltf_path, Gpu2Device* in_gpu_device, StaticModel* out_model)
{
    assert(out_model);
    memset(out_model, 0, sizeof(StaticModel));

    if (!gltf_load_asset(gltf_path, &out_model->gltf_asset))
    {
        printf("Failed to Load GLTF Asset\n");
        return false;
    }

	// First count up all vertices and indices
	for (i32 node_idx = 0; node_idx < out_model->gltf_asset.num_nodes; ++node_idx)
	{
        GltfNode* current_node = &out_model->gltf_asset.nodes[node_idx];
		GltfMesh* mesh = current_node->mesh;
        if (mesh)
		{
			// Count up total verts and indices so we can preallocate
			for (i32 prim_idx = 0; prim_idx < mesh->num_primitives; ++prim_idx)
			{
				GltfPrimitive* primitive = &mesh->primitives[prim_idx];
				out_model->num_vertices += primitive->positions->count;
				out_model->num_indices += primitive->indices->count;
			}
		}
	}

	if (out_model->num_vertices <= 0 || out_model->num_indices <= 0)
	{
		return false;
	}

	// Allocate + Zero storage for vertices + indices
	out_model->vertices = calloc(out_model->num_vertices, sizeof(StaticVertex));
	out_model->indices = calloc(out_model->num_indices, sizeof(u32));

	i32 vertex_offset = 0; // Incremented after each primitive
	i32 index_offset = 0; // Incremented after each primitive

    for (i32 node_idx = 0; node_idx < out_model->gltf_asset.num_nodes; ++node_idx)
	{
        GltfNode* current_node = &out_model->gltf_asset.nodes[node_idx];
		GltfMesh* mesh = current_node->mesh;
        if (mesh)
		{
			Mat4 transform = compute_static_node_transform(current_node, out_model->gltf_asset.nodes, out_model->gltf_asset.num_nodes);

			// Flatten all primitives into a single vertex/index array pair
			for (i32 prim_idx = 0; prim_idx < mesh->num_primitives; ++prim_idx)
			{
				GltfPrimitive* primitive = &mesh->primitives[prim_idx];

				// Get all of the vertex-related buffers we'll need
				u8* positions_buffer = primitive->positions->buffer_view->buffer->data;
				positions_buffer += gltf_accessor_get_initial_offset(primitive->positions);
				u32 positions_byte_stride = gltf_accessor_get_stride(primitive->positions);

				u8* normals_buffer = primitive->normals->buffer_view->buffer->data;
				normals_buffer += gltf_accessor_get_initial_offset(primitive->normals);
				u32 normals_byte_stride = gltf_accessor_get_stride(primitive->normals);

				u8* uvs_buffer = primitive->texcoord0->buffer_view->buffer->data;
				uvs_buffer += gltf_accessor_get_initial_offset(primitive->texcoord0);
				u32 uvs_byte_stride = gltf_accessor_get_stride(primitive->texcoord0);

				u32 primitive_vertices_count = primitive->positions->count;
				for (int i = 0; i < primitive_vertices_count; ++i)
				{
					// Compute index in our vertices array
					i32 out_index = i + vertex_offset;
					assert(out_index < out_model->num_vertices);

					Vec4 position = vec4_new(0,0,0,1);
					memcpy(&position, positions_buffer, positions_byte_stride);
					out_model->vertices[out_index].position = mat4_mul_vec4(transform, position);
					out_model->vertices[out_index].position.w = 1.0f;

					Vec4 normal = vec4_new(0,0,0,0);
					memcpy(&normal, normals_buffer, normals_byte_stride);
					out_model->vertices[out_index].normal = mat4_mul_vec4(transform, normal);
					out_model->vertices[out_index].normal.w = 0.0f;

					memcpy(&out_model->vertices[out_index].color, (float[4]){0.8f, 0.2f, 0.2f, 1.0f}, sizeof(float) * 4);
					memcpy(&out_model->vertices[out_index].uv, uvs_buffer, uvs_byte_stride);

					positions_buffer += positions_byte_stride;
					normals_buffer += normals_byte_stride;
					// colors_buffer += colors_byte_stride; //FCS TODO: Load real colors
					uvs_buffer += uvs_byte_stride;
				}

				u8* indices_buffer = primitive->indices->buffer_view->buffer->data;
				indices_buffer += gltf_accessor_get_initial_offset(primitive->indices);
				u32 indices_byte_stride = gltf_accessor_get_stride(primitive->indices);

				u32 primitive_indices_count = primitive->indices->count;
				for (int i = 0; i < primitive_indices_count; ++i)
				{
					// Compute index in our indices array
					i32 out_index = i + index_offset;
					assert(out_index < out_model->num_indices);
					memcpy(&out_model->indices[out_index], indices_buffer, indices_byte_stride);
					// Offset our current index by number of vertices before this primitive
					out_model->indices[out_index] += vertex_offset;

					indices_buffer += indices_byte_stride;
				}

				// Update our offsets that we use to flatten our primitives into one pair of vertices + indices
				vertex_offset += primitive_vertices_count;
				index_offset += primitive_indices_count;
			}
		}
	}


    // GPU Data Setup
    {
		Gpu2BufferCreateInfo vertex_buffer_create_info = {
			.usage = GPU2_BUFFER_USAGE_STORAGE_BUFFER,
			.is_cpu_visible = true,
			.size = sizeof(StaticVertex) * out_model->num_vertices,
			.data = out_model->vertices,
		};
		gpu2_create_buffer(in_gpu_device, &vertex_buffer_create_info, &out_model->vertex_buffer);

		Gpu2BufferCreateInfo index_buffer_create_info = {
			.usage = GPU2_BUFFER_USAGE_STORAGE_BUFFER,
			.is_cpu_visible = true,
			.size = sizeof(u32) * out_model->num_indices,
			.data = out_model->indices,
		};
		Gpu2Buffer index_buffer;
		gpu2_create_buffer(in_gpu_device, &index_buffer_create_info, &out_model->index_buffer);
    }


    return true;
}

void static_model_free(Gpu2Device* in_gpu_device, StaticModel* in_model)
{
    assert(in_model);
    gltf_free_asset(&in_model->gltf_asset);
    free(in_model->vertices);
    free(in_model->indices);
	gpu2_destroy_buffer(in_gpu_device, &in_model->vertex_buffer);	
	gpu2_destroy_buffer(in_gpu_device, &in_model->index_buffer);
}
