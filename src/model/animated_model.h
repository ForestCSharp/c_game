
#include "../gltf.h"
#include "../gpu.h"
#include "../types.h"
#include "assert.h"

// FCS TODO: SourceAnimation should have its own representation of bone hierarchy
// FCS TODO: BakedAnimation should only maintain a flat array of bones in the layout they'll be sent to the GPU

// ---- Source Animation: References GLTF Hierarchy Directly... slow to compute poses ---- //
typedef struct SourceAnimationKeyframe
{
    float time;
    Vec4 value; // scale,rot,or translation
    // FCS TODO: Store scale,rot,translation together here (optional values)
} SourceAnimationKeyframe;

typedef struct SourceAnimationChannel
{
    GltfNode* node;
    GltfAnimationPath path;

    i32 num_keyframes;
    SourceAnimationKeyframe* keyframes;
} SourceAnimationChannel;

declare_optional_type(Vec4);

Vec4 source_animation_channel_compute(SourceAnimationChannel* in_channel, float in_time)
{
    // FCS TODO: clamp to first and last kefyrames if outside of time bounds
    assert(in_channel->num_keyframes > 0);
    SourceAnimationKeyframe* first_keyframe = &in_channel->keyframes[0];
    if (in_time < first_keyframe->time)
    {
        return first_keyframe->value;
    }

    i32 last_keyframe_idx = in_channel->num_keyframes - 1;
    int i = 0;
    for (i32 keyframe_idx = 0; keyframe_idx < in_channel->num_keyframes; ++keyframe_idx)
    {
        SourceAnimationKeyframe* current_keyframe = &in_channel->keyframes[keyframe_idx];
        if (in_time >= current_keyframe->time)
        {
            // Last keyframe. Clamp to end
            if (keyframe_idx == last_keyframe_idx)
            {
                return current_keyframe->value;
            }
            else
            {
                SourceAnimationKeyframe* next_keyframe = &in_channel->keyframes[keyframe_idx + 1];
                const float t = (in_time - current_keyframe->time) / (next_keyframe->time - current_keyframe->time);
                // FCS TODO: special lerp for quaternion case?
                return vec4_lerp(t, current_keyframe->value, next_keyframe->value);
                // FCS TODO: Need to support different interpolation types...
            }
        }
    }

    printf("UNREACHABLE\n");
    exit(0);
    return vec4_new(0, 0, 0, 0);
}

typedef struct SourceAnimation
{
    i32 num_channels;
    SourceAnimationChannel* channels;
} SourceAnimation;

// ---- Baked Animation: created by precomputing all bone matrices for each keyframe from a source animation ---- //
typedef struct BakedAnimationKeyframe
{
    float time;
    // FCS TODO: Flat Array of Bone Matrices that we can just lerp 2 keyframes and copy straight to the GPU
} BakedAnimationKeyframe;

typedef struct BakedAnimation
{
    float timestep; // baked frames all have equal timesteps, used for fast access
    i32 num_keyframes;
    BakedAnimationKeyframe* keyframes;
} BakedAnimation;


// ---- Animated Model ---- //
typedef struct SkinnedVertex
{
    Vec3 position;
    Vec3 normal;
    Vec4 color;
    Vec2 uv;
    Vec4 joint_indices;
    Vec4 joint_weights;
} SkinnedVertex;

typedef struct AnimatedModel
{
    GltfAsset gltf_asset;

    i32 num_vertices;
    SkinnedVertex* vertices;

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

    if (!gltf_load_asset(gltf_path, &out_model->gltf_asset))
    {
        printf("Failed to Load GLTF Asset\n");
        return false;
    }

    // TODO: create animated models for all nodes with mesh + skin...

    // Find first animated node
    GltfNode* animated_node = NULL;
    for (i32 node_idx = 0; node_idx < out_model->gltf_asset.num_nodes; ++node_idx)
    {
        GltfNode* current_node = &out_model->gltf_asset.nodes[node_idx];
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

    out_model->vertices = calloc(out_model->num_vertices, sizeof(SkinnedVertex));
    out_model->indices = calloc(out_model->num_indices, sizeof(u32));

    // Flatten all primitives into a single vertex/index array pair
    i32 vertex_offset = 0; // Incremented after each primitive
    i32 index_offset = 0; // Incremented after each primitive
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

    // Skeleton+Animation Setup
    {
        // TODO: Bind Pose + Skeleton Setup
        GltfSkin* skin = animated_node->skin;
        assert(skin);

        printf("Num Joints: %i\n", skin->num_joints);


        assert(out_model->gltf_asset.num_animations > 0);


        // FCS TODO: Load all anims...
        GltfAnimation* animation = &out_model->gltf_asset.animations[0];

        // Create Source Animation
        SourceAnimation source_animation = {};
        source_animation.num_channels = animation->num_channels;
        source_animation.channels = calloc(source_animation.num_channels, sizeof(SourceAnimationChannel));

        // Deterine Animation Start and End Times
        optional(f32) animation_start = {};
        optional(f32) animation_end = {};

        for (i32 channel_idx = 0; channel_idx < animation->num_channels; ++channel_idx)
        {
            SourceAnimationChannel* source_channel = &source_animation.channels[channel_idx];

            GltfAnimationChannel* gltf_channel = &animation->channels[channel_idx];
            GltfAnimationSampler* gltf_sampler = gltf_channel->sampler;

            source_channel->node = gltf_channel->target.node;
            source_channel->path = gltf_channel->target.path;

            u8* input_buffer = gltf_sampler->input->buffer_view->buffer->data;
            input_buffer += gltf_accessor_get_initial_offset(gltf_sampler->input);
            u32 input_buffer_byte_stride = gltf_accessor_get_stride(gltf_sampler->input);
            assert(input_buffer_byte_stride == sizeof(float));

            u8* output_buffer = gltf_sampler->output->buffer_view->buffer->data;
            output_buffer += gltf_accessor_get_initial_offset(gltf_sampler->output);
            u32 output_buffer_byte_stride = gltf_accessor_get_stride(gltf_sampler->output);

            source_channel->num_keyframes = gltf_sampler->input->count;
            source_channel->keyframes = calloc(source_channel->num_keyframes, sizeof(SourceAnimationKeyframe));
            for (i32 keyframe_idx = 0; keyframe_idx < source_channel->num_keyframes; ++keyframe_idx)
            {
                SourceAnimationKeyframe* source_keyframe = &source_channel->keyframes[keyframe_idx];

                memcpy(&source_keyframe->time, input_buffer, input_buffer_byte_stride);
                input_buffer += input_buffer_byte_stride;

                memcpy(&source_keyframe->value, output_buffer, output_buffer_byte_stride);
                output_buffer += output_buffer_byte_stride;

                // animation_start = MIN(animation_start, source_keyframe->time);
                if (!optional_is_set(animation_start) || optional_get(animation_start) > source_keyframe->time)
                {
                    optional_set(animation_start, source_keyframe->time);
                }
                if (!optional_is_set(animation_end) || optional_get(animation_end) < source_keyframe->time)
                {
                    optional_set(animation_end, source_keyframe->time);
                }
            }
        }

        assert(optional_is_set(animation_start) && optional_is_set(animation_end));
        const float timestep = 1.0f / 60.0f;
        // printf("Animation Start: %f End: %f\n", animation_start.value, animation_end.value);
        for (float current_time = optional_get(animation_start); current_time <= optional_get(animation_end); current_time += timestep)
        {
            printf("Anim Time: %f seconds\n", current_time);

            // 1. Compute all animation channel current values
            Vec4 current_channel_values[source_animation.num_channels];
            for (i32 channel_idx = 0; channel_idx < source_animation.num_channels; ++channel_idx)
            {
                SourceAnimationChannel* current_channel = &source_animation.channels[channel_idx];
                current_channel_values[channel_idx] = source_animation_channel_compute(current_channel, current_time);
                printf("\tChannel Idx: %i Value: ", channel_idx);
                vec4_print(current_channel_values[channel_idx]);
            }

            // 2. Compute all node xforms, working from root
        }

        // FCS TODO: make sure to throw on one last keyframe if we have some space left?

        exit(0);

        // FCS TODO: use SourceAnimation to generate BakedAnimation
    }


    // GPU Data Setup
    {
        size_t vertices_size = sizeof(SkinnedVertex) * out_model->num_vertices;
        GpuBufferUsageFlags vertex_buffer_usage = GPU_BUFFER_USAGE_VERTEX_BUFFER | GPU_BUFFER_USAGE_TRANSFER_DST;
        out_model->vertex_buffer = gpu_create_buffer(gpu_context, vertex_buffer_usage, GPU_MEMORY_PROPERTY_DEVICE_LOCAL, vertices_size, "mesh vertex buffer");
        gpu_upload_buffer(gpu_context, &out_model->vertex_buffer, vertices_size, out_model->vertices);

        size_t indices_size = sizeof(u32) * out_model->num_indices;
        GpuBufferUsageFlags index_buffer_usage = GPU_BUFFER_USAGE_INDEX_BUFFER | GPU_BUFFER_USAGE_TRANSFER_DST;
        out_model->index_buffer = gpu_create_buffer(gpu_context, index_buffer_usage, GPU_MEMORY_PROPERTY_DEVICE_LOCAL, indices_size, "mesh index buffer");
        gpu_upload_buffer(gpu_context, &out_model->index_buffer, indices_size, out_model->indices);
    }

    return true;
}

void animated_model_free(GpuContext* gpu_context, AnimatedModel* in_model)
{
    assert(in_model);
    gltf_free_asset(&in_model->gltf_asset);
    free(in_model->vertices);
    free(in_model->indices);

    gpu_destroy_buffer(gpu_context, &in_model->vertex_buffer);
    gpu_destroy_buffer(gpu_context, &in_model->index_buffer);
}
