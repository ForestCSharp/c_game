
#include "../gltf.h"
#include "../gpu.h"
#include "../types.h"
#include "assert.h"

typedef struct BakedAnimationKeyframe
{
    float time;
    // FCS TODO: Flat Array of Bone Matrices that we can just copy straight to the GPU
} BakedAnimationKeyframe;

typedef struct BakedAnimation
{
    float timestep; // baked frames all have equal timesteps, used for fast access
    i32 num_keyframes;
    BakedAnimationKeyframe* keyframes;
} BakedAnimation;

typedef struct AnimatedVertex
{
    Vec3 position;
    Vec3 normal;
    Vec4 color;
    Vec2 uv;
    Vec4 joint_indices;
    Vec4 joint_weights;
} AnimatedVertex;

typedef struct AnimatedModel
{
    i32 num_vertices;
    AnimatedVertex* vertices;

    i32 num_indices;
    u32* indices;

    GpuBuffer vertex_buffer;
    GpuBuffer index_buffer;

    // FCS TODO: JOINTS

} AnimatedModel;

bool animated_model_load(const char* gltf_path, GpuContext* gpu_context, AnimatedModel* out_model)
{
    assert(out_model);
    memset(out_model, 0, sizeof(AnimatedModel));

    GltfAsset gltf_asset = {};
    if (!gltf_load_asset(gltf_path, &gltf_asset))
    {
        printf("Failed to Load GLTF Asset\n");
        return false;
    }

    // TODO: create animated models for all nodes with mesh + skin...

    // Find first animated node
    GltfNode* animated_node = NULL;
    for (i32 node_idx = 0; node_idx < gltf_asset.num_nodes; ++node_idx)
    {
        GltfNode* current_node = &gltf_asset.nodes[node_idx];
        if (current_node->mesh && current_node->skin)
        {
            animated_node = current_node;
            break;
        }
    }

    if (animated_node == NULL)
    {
        return false;
    }

    GltfSkin* skin = animated_node->skin;
    assert(skin);

    GltfMesh* mesh = animated_node->mesh;
    assert(mesh);

    // Count up total verts and indices so we can preallocate
    for (i32 prim_idx = 0; prim_idx < mesh->num_primitives; ++prim_idx)
    {
        GltfPrimitive* primitive = &mesh->primitives[prim_idx];
        out_model->num_vertices += primitive->positions->count;
        out_model->num_indices += primitive->indices->count;
    }
    if (out_model->num_vertices <= 0 || out_model->num_indices <= 0)
    {
        return false;
    }

    out_model->vertices = calloc(out_model->num_vertices, sizeof(AnimatedVertex));
    out_model->indices  = calloc(out_model->num_indices, sizeof(u32));

    // Flatten all primitives into a single vertex/index array pair
    i32 vertex_offset = 0; // Incremented after each primitive
    i32 index_offset  = 0; // Incremented after each primitive
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

        u8* joint_indices_buffer = primitive->joint_indices->buffer_view->buffer->data;
        joint_indices_buffer += gltf_accessor_get_initial_offset(primitive->joint_indices);
        u32 joint_indices_byte_stride = gltf_accessor_get_stride(primitive->joint_indices);

        u8* joint_weights_buffer = primitive->joint_weights->buffer_view->buffer->data;
        joint_weights_buffer += gltf_accessor_get_initial_offset(primitive->joint_weights);
        u32 joint_weights_byte_stride = gltf_accessor_get_stride(primitive->joint_weights);

        u32 primitive_vertices_count = primitive->positions->count;
        for (int i = 0; i < primitive_vertices_count; ++i)
        {
            i32 out_index = i + vertex_offset;
            assert(out_index < out_model->num_vertices);

            memcpy(&out_model->vertices[out_index].position, positions_buffer, positions_byte_stride);
            memcpy(&out_model->vertices[out_index].normal, normals_buffer, normals_byte_stride);
            memcpy(&out_model->vertices[out_index].color, (float[4]){0.8f, 0.2f, 0.2f, 1.0f}, sizeof(float) * 4);
            memcpy(&out_model->vertices[out_index].uv, uvs_buffer, uvs_byte_stride);
            memcpy(&out_model->vertices[out_index].joint_indices, joint_indices_buffer, joint_indices_byte_stride);
            memcpy(&out_model->vertices[out_index].joint_weights, joint_weights_buffer, joint_weights_byte_stride);

            positions_buffer += positions_byte_stride;
            normals_buffer += normals_byte_stride;
            // colors_buffer += colors_byte_stride; //FCS TODO: Load real colors
            uvs_buffer += uvs_byte_stride;

            joint_indices_buffer += joint_indices_byte_stride;
            joint_weights_buffer += joint_weights_byte_stride;
        }

        u8* indices_buffer = primitive->indices->buffer_view->buffer->data;
        indices_buffer += gltf_accessor_get_initial_offset(primitive->indices);
        u32 indices_byte_stride = gltf_accessor_get_stride(primitive->indices);

        u32 primitive_indices_count = primitive->indices->count;
        for (int i = 0; i < primitive_indices_count; ++i)
        {
            i32 out_index = i + index_offset;
            assert(out_index < out_model->num_indices);

            memcpy(&out_model->indices[out_index], indices_buffer, indices_byte_stride);
            // Offset our index buffer indices because we're flattening all primitives
            out_model->indices[out_index] += index_offset;

            indices_buffer += indices_byte_stride;
        }

        // Update our offsets that we use to flatten our primitives into one pair of vertices + indices
        vertex_offset += primitive_vertices_count;
        index_offset += primitive_indices_count;
    }

    // Skeleton Setup


    // GPU Data Setup
    {
        size_t vertices_size                    = sizeof(AnimatedVertex) * out_model->num_vertices;
        GpuBufferUsageFlags vertex_buffer_usage = GPU_BUFFER_USAGE_VERTEX_BUFFER | GPU_BUFFER_USAGE_TRANSFER_DST;
        out_model->vertex_buffer                = gpu_create_buffer(gpu_context, vertex_buffer_usage, GPU_MEMORY_PROPERTY_DEVICE_LOCAL, vertices_size, "mesh vertex buffer");
        gpu_upload_buffer(gpu_context, &out_model->vertex_buffer, vertices_size, out_model->vertices);

        size_t indices_size                    = sizeof(u32) * out_model->num_indices;
        GpuBufferUsageFlags index_buffer_usage = GPU_BUFFER_USAGE_INDEX_BUFFER | GPU_BUFFER_USAGE_TRANSFER_DST;
        out_model->index_buffer                = gpu_create_buffer(gpu_context, index_buffer_usage, GPU_MEMORY_PROPERTY_DEVICE_LOCAL, indices_size, "mesh index buffer");
        gpu_upload_buffer(gpu_context, &out_model->index_buffer, indices_size, out_model->indices);
    }

    return true;
}

void animated_model_free(GpuContext* gpu_context, AnimatedModel* in_model)
{
    assert(in_model);
    free(in_model->vertices);
    free(in_model->indices);

    // FCS TODO: Free gpu resources...
    gpu_destroy_buffer(gpu_context, &in_model->vertex_buffer);
    gpu_destroy_buffer(gpu_context, &in_model->index_buffer);
}
