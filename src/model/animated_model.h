#pragma once

#include "../basic_math.h"
#include "../gltf.h"
#include "../gpu.h"
#include "../types.h"
#include "assert.h"

// FCS TODO: SourceAnimation should have its own representation of bone hierarchy
// FCS TODO: BakedAnimation should only maintain a flat array of bones in the layout they'll be sent to the GPU
// FCS TODO: shared struct for flat array of bones...

// ---- Source Animation: References GLTF Hierarchy Directly... slow to compute poses ---- //
typedef struct SourceAnimationKeyframe
{
    float time;
	union
	{
		Vec3 scale;
		Quat rotation;
		Vec3 translation;
	} value;
} SourceAnimationKeyframe;

typedef struct SourceAnimationChannel
{
    GltfNode* node;
    GltfAnimationPath path;

    i32 num_keyframes;
    SourceAnimationKeyframe* keyframes;
} SourceAnimationChannel;

typedef struct SourceAnimation
{
    i32 num_channels;
    SourceAnimationChannel* channels;

	i32 num_joints;
	Mat4* joint_inverse_bind_matrices;
} SourceAnimation;

// ---- Baked Animation: created by precomputing all bone matrices for each keyframe from a source animation ---- //
typedef struct BakedAnimationKeyframe
{
    float time;
	i32 num_joints;
	Mat4* joint_matrices;
} BakedAnimationKeyframe;

typedef struct BakedAnimation
{
    float timestep; // baked frames all have equal timesteps, used for fast access
	float animation_start; //FCS TODO: Rename to start_time
	float animation_end; //FCS TODO: Rename to end_time
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

	BakedAnimation baked_animation;
    // FCS TODO: JOINT MATRICES @ bind pose...
} AnimatedModel;

typedef struct NodeAnimData 
{
	optional(Vec3) translation;
	optional(Quat) rotation;
	optional(Vec3) scale;
	optional(Mat4) cached_transform;
} NodeAnimData;

void anim_data_set(NodeAnimData* anim_data, GltfAnimationPath path, SourceAnimationKeyframe* in_keyframe)
{
	switch (path)
	{
		case GLTF_ANIMATION_PATH_SCALE:
		{
			optional_set(anim_data->scale, in_keyframe->value.scale);
			break;
		}
		case GLTF_ANIMATION_PATH_ROTATION:
		{
			optional_set(anim_data->rotation, in_keyframe->value.rotation);
			break;
		}
		case GLTF_ANIMATION_PATH_TRANSLATION:
		{
			optional_set(anim_data->translation, in_keyframe->value.translation);
			break;
		}
		default:
		break;
	}
}

void anim_data_lerp_and_set(NodeAnimData* anim_data, GltfAnimationPath path, SourceAnimationKeyframe* in_left_keyframe, SourceAnimationKeyframe* in_right_keyframe, float t)
{
	switch (path)
	{
		case GLTF_ANIMATION_PATH_SCALE:
		{
			Vec3 lerped_scale = vec3_lerp(t, in_left_keyframe->value.scale, in_right_keyframe->value.scale);
			optional_set(anim_data->scale, lerped_scale);
			break;
		}
		case GLTF_ANIMATION_PATH_ROTATION:
		{
			Quat lerped_rotation = quat_slerp(t, in_left_keyframe->value.rotation, in_right_keyframe->value.rotation); 
			optional_set(anim_data->rotation, lerped_rotation);
			break;
		}
		case GLTF_ANIMATION_PATH_TRANSLATION:
		{
			Vec3 lerped_translation = vec3_lerp(t, in_left_keyframe->value.translation, in_right_keyframe->value.translation);
			optional_set(anim_data->translation, lerped_translation);
			break;
		}
		default:
		break;
	}
}

//FCS TODO: move out_anim_data to first arg rename to "anim_data"
void anim_data_compute_from_channel(NodeAnimData* anim_data, SourceAnimationChannel* in_channel, float in_time)
{
    // FCS TODO: clamp to first and last kefyrames if outside of time bounds
    assert(in_channel->num_keyframes > 0);
    SourceAnimationKeyframe* first_keyframe = &in_channel->keyframes[0];
    if (in_time < first_keyframe->time)
    {
		anim_data_set(anim_data, in_channel->path, first_keyframe);
    }

    i32 last_keyframe_idx = in_channel->num_keyframes - 1;
    for (i32 keyframe_idx = 0; keyframe_idx < in_channel->num_keyframes; ++keyframe_idx)
    {
        SourceAnimationKeyframe* current_keyframe = &in_channel->keyframes[keyframe_idx];
        if (in_time >= current_keyframe->time)
        {
            if (keyframe_idx == last_keyframe_idx)
            {
            	// Last keyframe. Clamp to end
				anim_data_set(anim_data, in_channel->path, current_keyframe);
            }
            else
            {
				// Need to lerp two keyframes
                SourceAnimationKeyframe* next_keyframe = &in_channel->keyframes[keyframe_idx + 1];
                const float t = (in_time - current_keyframe->time) / (next_keyframe->time - current_keyframe->time);
            	anim_data_lerp_and_set(anim_data, in_channel->path, current_keyframe, next_keyframe, t);    
            }
        }
    }
}

// nodes_array and anim_data_array are of length num_nodes 
// FCS TODO: pass args as struct?
Mat4 compute_node_transform(GltfNode* target_node, GltfNode* nodes_array, NodeAnimData* anim_data_array, i32 num_nodes)
{
	const size_t node_index = (target_node - nodes_array);
	assert(node_index < num_nodes && &nodes_array[node_index] == target_node);

	NodeAnimData* anim_data = &anim_data_array[node_index];

	if (optional_is_set(anim_data->cached_transform))
	{
		return optional_get(anim_data->cached_transform);
	}
	else
	{
		// Compute Parent Transform
		Mat4 parent_transform = mat4_identity;
		if (target_node->parent)
		{
			parent_transform = compute_node_transform(target_node->parent, nodes_array, anim_data_array, num_nodes);
		}

		Mat4 local_transform = mat4_identity;
		if (optional_is_set(anim_data->scale))
		{
			Vec3 scale = optional_get(anim_data->scale);
			local_transform = mat4_mul_mat4(local_transform, mat4_scale(scale));
		}
		if (optional_is_set(anim_data->rotation))
		{
			Quat rotation = optional_get(anim_data->rotation);
			local_transform = mat4_mul_mat4(local_transform, quat_to_mat4(rotation));
		}
		if (optional_is_set(anim_data->translation))
		{
			Vec3 translation = optional_get(anim_data->translation);
			local_transform = mat4_mul_mat4(local_transform, mat4_translation(translation));
		}

		Mat4 computed_transform = mat4_mul_mat4(parent_transform, local_transform);

		optional_set(anim_data->cached_transform, computed_transform);
		return computed_transform;
	}
}

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

    // Skeleton + Animation Setup
    {
        // TODO: Bind Pose + Skeleton Setup
        GltfSkin* skin = animated_node->skin;
        assert(skin);

        assert(out_model->gltf_asset.num_animations > 0);


        // FCS TODO: Load all anims...
        GltfAnimation* animation = &out_model->gltf_asset.animations[0];

        // Create Source Animation
        SourceAnimation source_animation = {};
        source_animation.num_channels = animation->num_channels;
        source_animation.channels = calloc(source_animation.num_channels, sizeof(SourceAnimationChannel));

		// Copy Inverse Bind Matrices
		source_animation.num_joints = skin->num_joints;
		source_animation.joint_inverse_bind_matrices = calloc(skin->num_joints, sizeof(Mat4));
		{
			 u8* bind_matrices_buffer = skin->inverse_bind_matrices->buffer_view->buffer->data;
			bind_matrices_buffer += gltf_accessor_get_initial_offset(skin->inverse_bind_matrices);
			u32 bind_matrices_buffer_byte_stride = gltf_accessor_get_stride(skin->inverse_bind_matrices);
			assert(bind_matrices_buffer_byte_stride == sizeof(Mat4));

			for (i32 joint_idx = 0; joint_idx < skin->num_joints; ++joint_idx)
			{
				memcpy(&source_animation.joint_inverse_bind_matrices[joint_idx], bind_matrices_buffer, bind_matrices_buffer_byte_stride);
				bind_matrices_buffer += bind_matrices_buffer_byte_stride;
			}
		}

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
		const float animation_duration = optional_get(animation_end) - optional_get(animation_start);
		printf("Animation Start: %f End: %f Duration: %f\n", optional_get(animation_start), optional_get(animation_end), animation_duration);

        const float timestep = 1.0f / 60.0f;

		const i32 num_keyframes = (i32) ceil(animation_duration / timestep);
		printf("Num Keyframes: %i\n", num_keyframes);

		out_model->baked_animation = (BakedAnimation) {
			.timestep = timestep,
			.animation_start = optional_get(animation_start),
			.animation_end = optional_get(animation_end),
			.num_keyframes = num_keyframes,
			.keyframes = calloc(num_keyframes, sizeof(BakedAnimationKeyframe))
		};

		float current_time = optional_get(animation_start);
		for (i32 keyframe_idx = 0; keyframe_idx < num_keyframes; ++keyframe_idx)
		{
			BakedAnimationKeyframe* keyframe = &out_model->baked_animation.keyframes[keyframe_idx];
			*keyframe = (BakedAnimationKeyframe) {
				.time = current_time,
				.num_joints = skin->num_joints,
				.joint_matrices = calloc(skin->num_joints, sizeof(Mat4))
			};

			current_time += timestep;

			const i32 num_gltf_nodes = out_model->gltf_asset.num_nodes;
			NodeAnimData* node_anim_data_array = calloc(num_gltf_nodes, sizeof(NodeAnimData));	

            // 1. Compute all animation channel current values
            for (i32 channel_idx = 0; channel_idx < source_animation.num_channels; ++channel_idx)
            {
                SourceAnimationChannel* current_channel = &source_animation.channels[channel_idx];

				GltfNode* node = current_channel->node;
				const size_t node_index = (node - out_model->gltf_asset.nodes);
				assert(&out_model->gltf_asset.nodes[node_index] == node);

				NodeAnimData* node_anim_data = &node_anim_data_array[node_index];
				anim_data_compute_from_channel(node_anim_data, current_channel, current_time);
            }

			Mat4 animated_node_transform = compute_node_transform(animated_node, out_model->gltf_asset.nodes, node_anim_data_array, num_gltf_nodes);
			optional(Mat4) inverse_global_transform = mat4_inverse(animated_node_transform); 
			assert(optional_is_set(inverse_global_transform));

            // 2. Iterate over Joints and computer their xforms (have to work back to root)
			for (i32 joint_idx = 0; joint_idx < skin->num_joints; ++joint_idx)
			{
				GltfNode* joint = skin->joints[joint_idx];

				Mat4 global_joint_transform = compute_node_transform(joint, out_model->gltf_asset.nodes, node_anim_data_array, num_gltf_nodes);
				Mat4 inverse_bind_matrix = source_animation.joint_inverse_bind_matrices[joint_idx];
				Mat4 final_joint_matrix = mat4_mul_mat4(optional_get(inverse_global_transform), mat4_mul_mat4(global_joint_transform, inverse_bind_matrix));

				keyframe->joint_matrices[joint_idx] = final_joint_matrix; 
			}

			free(node_anim_data_array);
			current_time += timestep;
		}

        // FCS TODO: make sure to throw on one last keyframe if we have some space left (current_time != animation_end)

		//FCS TODO: Free up SourceAnimation data...

        //exit(0);
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

	for (i32 keyframe_idx = 0; keyframe_idx < in_model->baked_animation.num_keyframes; ++keyframe_idx)
	{
		BakedAnimationKeyframe* keyframe = &in_model->baked_animation.keyframes[keyframe_idx]; 
		free(keyframe->joint_matrices);
	}
	free(in_model->baked_animation.keyframes);
}

//FCS TODO: Use above for anywhere that needs num_joints + Mat4* array...
typedef struct JointData
{
	i32 num_joints;
	Mat4* joint_matrices;
} JointData;

JointData animated_model_sample_animation(AnimatedModel* in_model, float in_anim_time)
{	
	BakedAnimation* animation = &in_model->baked_animation;

	i32 last_keyframe_idx = animation->num_keyframes - 1;
	for (i32 keyframe_idx = 0; keyframe_idx < animation->num_keyframes; ++keyframe_idx)
	{
	  	BakedAnimationKeyframe* keyframe = &animation->keyframes[keyframe_idx];

		const bool is_last_keyframe = keyframe_idx == last_keyframe_idx; 
		// If anim time is before or after all of our keyframes, clamp to first/last keyframe
		if (in_anim_time <= keyframe->time || is_last_keyframe)
		{
			printf("in_anim_time: %f keyframe->time: %f\n", in_anim_time, keyframe->time);
			printf("Returning keyframe %i\n", keyframe_idx);
			return (JointData) {
				.num_joints = keyframe->num_joints,
				.joint_matrices = keyframe->joint_matrices,
			};
		}
		
		BakedAnimationKeyframe* next_keyframe = &animation->keyframes[keyframe_idx + 1];
	  	if (in_anim_time <= next_keyframe->time)
		{
			//FCS TODO: Need to actually lerp here
			printf("in_anim_time: %f keyframe->time: %f\n", in_anim_time, keyframe->time);
			printf("Returning keyframe %i\n", keyframe_idx);
			return (JointData) {
				.num_joints = keyframe->num_joints,
				.joint_matrices = keyframe->joint_matrices,
			};
		}

		// Otherwise we need to lerp between the two keyframes	
		//FCS TODO:
	}

	assert(false);
	return (JointData) {};
}
//FCS TODO: Need to allocate bones if we're lerping
