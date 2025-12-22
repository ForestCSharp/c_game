#pragma once

#include "gpu/gpu.h"
#include "model/static_model.h"
#include "model/animated_model.h"
#include "stretchy_buffer.h"
#include "basic_types.h"
#include "math/math_lib.h"
#include "uniforms.h"

const i64 INVALID_INDEX = -1;

#define COMPONENT_TYPE(type) COMPONENT_TYPE_##type
#define COMPONENT_HANDLE_STRUCT_NAME(type) type##Handle
#define COMPONENT_ENTRY_STRUCT_NAME(type) struct ComponentEntry##type
#define COMPONENT_DATA_VAR_NAME(type) type##_storage

#define DEFINE_COMPONENT_STORAGE(type)\
struct {\
	sbuffer(COMPONENT_ENTRY_STRUCT_NAME(type){\
		type component;\
		i64 ref_count;\
	}) component_array;\
	sbuffer(i64) free_list;\
} COMPONENT_DATA_VAR_NAME(type);

#define DEFINE_COMPONENT_INTERFACE(type) \
	typedef struct { i64 idx; } COMPONENT_HANDLE_STRUCT_NAME(type);\
	type* game_object_manager_get_##type(GameObjectManager* manager, i64 component_idx)\
	{\
		bool is_valid_index = component_idx >= 0 && component_idx < sb_count(manager->COMPONENT_DATA_VAR_NAME(type).component_array);\
		return is_valid_index ? &manager->COMPONENT_DATA_VAR_NAME(type).component_array[component_idx].component : NULL;\
	}\
	type* game_object_manager_get_##type##_from_object(GameObjectManager* manager, GameObjectHandle object_handle)\
	{\
		GameObject* object = &manager->game_object_array[object_handle.idx];\
		const i64 component_idx = object->component_ids[COMPONENT_TYPE(type)];\
		return game_object_manager_get_##type(manager, component_idx);\
	}\
	COMPONENT_HANDLE_STRUCT_NAME(type) game_object_manager_create_##type(GameObjectManager* manager, type value)\
	{\
		COMPONENT_HANDLE_STRUCT_NAME(type) out_handle = {};\
		if (sb_count(manager->COMPONENT_DATA_VAR_NAME(type).free_list) > 0)\
		{\
			i64 found_idx = manager->COMPONENT_DATA_VAR_NAME(type).free_list[0];\
			manager->COMPONENT_DATA_VAR_NAME(type).component_array[found_idx].component = value;\
			sb_del(manager->COMPONENT_DATA_VAR_NAME(type).free_list, 0);\
			out_handle.idx = found_idx;\
		}\
		else\
		{\
			COMPONENT_ENTRY_STRUCT_NAME(type) new_component_entry = {.component = value, .ref_count = 0};\
			sb_push(manager->COMPONENT_DATA_VAR_NAME(type).component_array, new_component_entry);\
			out_handle.idx = sb_count(manager->COMPONENT_DATA_VAR_NAME(type).component_array) - 1;\
		}\
		return out_handle;\
	}\
	void game_object_manager_destroy_##type(GameObjectManager* manager, i64 idx)\
	{\
		assert(idx > INVALID_INDEX && idx < sb_count(manager->COMPONENT_DATA_VAR_NAME(type).component_array));\
		assert(manager->COMPONENT_DATA_VAR_NAME(type).component_array[idx].ref_count == 0);\
		sb_push(manager->COMPONENT_DATA_VAR_NAME(type).free_list, idx);\
	}\
	void game_object_manager_set_##type(GameObjectManager* manager, GameObjectHandle object_handle, COMPONENT_HANDLE_STRUCT_NAME(type)* component_handle)\
	{\
		GameObject* object = &manager->game_object_array[object_handle.idx];\
		i64 new_idx = component_handle ? component_handle->idx : INVALID_INDEX;\
		if (new_idx != INVALID_INDEX)\
		{\
			assert(new_idx >= 0 && new_idx < sb_count(manager->COMPONENT_DATA_VAR_NAME(type).component_array));\
		}\
		i64 old_idx = object->component_ids[COMPONENT_TYPE(type)];\
		if (new_idx != old_idx)\
		{\
			if (old_idx != INVALID_INDEX) {\
				manager->COMPONENT_DATA_VAR_NAME(type).component_array[old_idx].ref_count--;\
				if (manager->COMPONENT_DATA_VAR_NAME(type).component_array[old_idx].ref_count == 0)\
				{\
					game_object_manager_destroy_##type(manager, old_idx);\
				}\
			}\
			if (new_idx != INVALID_INDEX) { manager->COMPONENT_DATA_VAR_NAME(type).component_array[new_idx].ref_count++; }\
			object->component_ids[COMPONENT_TYPE(type)] = new_idx;\
		}\
	}\

// Adds a new object
#define ADD_OBJECT(manager_ptr) game_object_manager_add_object(manager_ptr)

// Checks if object handle points to a valid object
#define OBJECT_IS_VALID(manager_ptr, object_handle) game_object_manager_is_valid_object(manager_ptr, object_handle)

// Removes an existing object
#define REMOVE_OBJECT(manager_ptr, object_handle) game_object_manager_remove_object(manager_ptr, object_handle)

// Creates a component but doesn't yet associate it with an object
#define CREATE_COMPONENT(type, manager_ptr, value) game_object_manager_create_##type(manager_ptr, value)

// Associates a previously created component to an object. pass NULL to component_handle_ptr to remove an existing component
#define OBJECT_SET_COMPONENT(type, manager_ptr, object_handle, component_handle_ptr) game_object_manager_set_##type(manager_ptr, object_handle, component_handle_ptr)

// Creates a component and sets it to an object
#define OBJECT_CREATE_COMPONENT(type, manager_ptr, object_handle, value)\
{\
	COMPONENT_HANDLE_STRUCT_NAME(type) new_component_handle = CREATE_COMPONENT(type, manager_ptr, value);\
	OBJECT_SET_COMPONENT(type, manager_ptr, object_handle, &new_component_handle);\
}

// Used to get pointer to objects current component. Pointer may grow stale on component modfications
#define OBJECT_GET_COMPONENT(type, manager_ptr, object_handle) game_object_manager_get_##type##_from_object(manager_ptr, object_handle)

//FCS TODO: Function to compact GameObject and Component arrays (properly fixes up references...)

typedef struct {i64 idx; } GameObjectHandle;

typedef struct AttachmentPoint
{
	GameObjectHandle object_handle;	
	optional(String) name;
	bool ignore_translation;
	bool ignore_rotation;
	bool ignore_scale;
} AttachmentPoint;
declare_optional_type(AttachmentPoint);

typedef struct TransformComponent
{
	TRS trs;
	optional(AttachmentPoint) parent;
} TransformComponent;

typedef struct StaticModelComponent
{
	StaticModel static_model;
} StaticModelComponent;

typedef struct AnimatedModelComponent
{
	AnimatedModel animated_model;
	GpuBuffer joint_matrices_buffer;
	Mat4* mapped_buffer_data;
	float animation_rate;
	float current_anim_time;
} AnimatedModelComponent;

typedef struct ObjectRenderDataComponent
{
	sbuffer(GpuBuffer) uniform_buffers;
	sbuffer(ObjectUniformStruct*) uniform_data;
	sbuffer(GpuBindGroup) bind_groups;
} ObjectRenderDataComponent;

typedef struct PlayerControlComponent
{
	float move_speed;
} PlayerControlComponent;


typedef struct CameraComponent
{
	Vec3 camera_up;
	Vec3 camera_forward;
	float fov;
} CameraComponent;

typedef enum ComponentType
{
	COMPONENT_TYPE(TransformComponent),
	COMPONENT_TYPE(AnimatedModelComponent),
	COMPONENT_TYPE(StaticModelComponent),
	COMPONENT_TYPE(ObjectRenderDataComponent),
	COMPONENT_TYPE(PlayerControlComponent),
	COMPONENT_TYPE(CameraComponent),
	COMPONENT_TYPE_COUNT,
} ComponentType;

typedef struct GameObject
{
	bool is_valid;
	i64 component_ids[COMPONENT_TYPE_COUNT];
} GameObject;

typedef struct GameObjectManager
{
	sbuffer(GameObject) game_object_array;
	sbuffer(GameObjectHandle) game_object_free_list;
	
	DEFINE_COMPONENT_STORAGE(TransformComponent);
	DEFINE_COMPONENT_STORAGE(StaticModelComponent);
	DEFINE_COMPONENT_STORAGE(AnimatedModelComponent);
	DEFINE_COMPONENT_STORAGE(ObjectRenderDataComponent);
	DEFINE_COMPONENT_STORAGE(PlayerControlComponent);
	DEFINE_COMPONENT_STORAGE(CameraComponent);

} GameObjectManager;

DEFINE_COMPONENT_INTERFACE(TransformComponent);
DEFINE_COMPONENT_INTERFACE(StaticModelComponent);
DEFINE_COMPONENT_INTERFACE(AnimatedModelComponent);
DEFINE_COMPONENT_INTERFACE(ObjectRenderDataComponent);
DEFINE_COMPONENT_INTERFACE(PlayerControlComponent);
DEFINE_COMPONENT_INTERFACE(CameraComponent);

void game_object_manager_init(GameObjectManager* manager)
{
	assert(manager);
	*manager = (GameObjectManager){};
}

void game_object_manager_destroy(GameObjectManager* manager)
{
	assert(manager);

	sb_free(manager->game_object_array);
	sb_free(manager->game_object_free_list);

	sb_free(manager->COMPONENT_DATA_VAR_NAME(TransformComponent).component_array);
	sb_free(manager->COMPONENT_DATA_VAR_NAME(TransformComponent).free_list);

	sb_free(manager->COMPONENT_DATA_VAR_NAME(StaticModelComponent).component_array);
	sb_free(manager->COMPONENT_DATA_VAR_NAME(StaticModelComponent).free_list);

	sb_free(manager->COMPONENT_DATA_VAR_NAME(AnimatedModelComponent).component_array);
	sb_free(manager->COMPONENT_DATA_VAR_NAME(AnimatedModelComponent).free_list);

	sb_free(manager->COMPONENT_DATA_VAR_NAME(ObjectRenderDataComponent).component_array);
	sb_free(manager->COMPONENT_DATA_VAR_NAME(ObjectRenderDataComponent).free_list);
}

GameObjectHandle game_object_manager_add_object(GameObjectManager* manager)
{	
	GameObject new_object = {
		.is_valid = true,
	};
	memset(new_object.component_ids, -1, sizeof(i64) * COMPONENT_TYPE_COUNT);

	GameObjectHandle out_handle = {};

	if (sb_count(manager->game_object_free_list) > 0)
	{
		out_handle.idx = manager->game_object_free_list[0].idx;
		sb_del(manager->game_object_free_list, 0);
	}
	else
	{
		out_handle.idx = sb_count(manager->game_object_array);
		sb_push(manager->game_object_array, new_object);
	}

	return out_handle;
}

bool game_object_manager_is_valid_object(GameObjectManager* manager, GameObjectHandle in_handle)
{
	i64 obj_idx = in_handle.idx;
	assert(obj_idx >= 0 && obj_idx < sb_count(manager->game_object_array));
	return manager->game_object_array[obj_idx].is_valid;
}

void game_object_manager_remove_object(GameObjectManager* manager, GameObjectHandle in_handle)
{
	i64 obj_idx = in_handle.idx;
	assert(obj_idx >= 0 && obj_idx < sb_count(manager->game_object_array));
	manager->game_object_array[obj_idx].is_valid = false;
	sb_push(manager->game_object_free_list, ((GameObjectHandle){.idx = obj_idx }));
}

// Component Helper Functions
TRS game_object_compute_global_transform(GameObjectManager* manager, GameObjectHandle object_handle)
{
	TransformComponent* my_transform = OBJECT_GET_COMPONENT(TransformComponent, manager, object_handle);
	assert(my_transform);
	TRS my_trs = my_transform->trs;

	TRS parent_trs = trs_identity;
	if (optional_is_set(my_transform->parent))
	{
		const AttachmentPoint* parent = &optional_get(my_transform->parent);
		GameObjectHandle parent_object_handle = parent->object_handle;

		parent_trs = game_object_compute_global_transform(manager, parent_object_handle);

		// Check ignore flags
		if (parent->ignore_translation)
		{
			parent_trs.translation = vec3_zero;
		}

		if (parent->ignore_rotation)
		{
			parent_trs.rotation = quat_identity;
		}

		if (parent->ignore_scale)
		{
			parent_trs.scale = vec3_one;
		}
	}

	return trs_combine(parent_trs, my_trs);
}

void game_object_render_data_setup(GameObjectManager* manager, GpuDevice* in_gpu_device, GameObjectHandle object_handle, GpuBindGroupLayout* per_object_bind_group_layout)
{
	ObjectRenderDataComponent object_render_data = {};

	//For each backbuffer...
	u32 swapchain_count = gpu_get_swapchain_count(in_gpu_device);
	for (i32 swapchain_idx = 0; swapchain_idx < swapchain_count; ++swapchain_idx)
	{
		StaticModelComponent* static_model_component = OBJECT_GET_COMPONENT(StaticModelComponent, manager, object_handle);
		AnimatedModelComponent* animated_model_component = OBJECT_GET_COMPONENT(AnimatedModelComponent, manager, object_handle);
		if (!static_model_component && !animated_model_component)
		{
			continue;
		}

		// Shouldn't have both types of models
		assert(static_model_component == NULL || animated_model_component == NULL);

		ObjectUniformStruct initial_uniform_data = {
			.model = mat4_identity,
			.is_skinned = animated_model_component != NULL,
		};

		// Create Uniform Buffer
		GpuBufferCreateInfo uniform_buffer_create_info = {
			.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
			.is_cpu_visible = true,
			.size = sizeof(ObjectUniformStruct),
			.data = &initial_uniform_data,
		};
		GpuBuffer uniform_buffer;
		gpu_create_buffer(in_gpu_device, &uniform_buffer_create_info, &uniform_buffer);
		sb_push(
			object_render_data.uniform_buffers,
			uniform_buffer
		);

		// Map Uniform Buffer
		sb_push(
			object_render_data.uniform_data,
			(ObjectUniformStruct*) gpu_map_buffer(in_gpu_device, &uniform_buffer)
		);

		// Create Descriptor Set
		GpuBindGroupCreateInfo per_object_bind_group_create_info = {
			.layout = per_object_bind_group_layout,
		};
		GpuBindGroup per_object_bind_group;
		gpu_create_bind_group(in_gpu_device, &per_object_bind_group_create_info, &per_object_bind_group);

		// Write uniform buffer to descriptor set
	   sbuffer(GpuResourceWrite) resource_writes = NULL;

		sb_push(resource_writes, ((GpuResourceWrite){
			.binding_index = 0,
			.type = GPU_BINDING_TYPE_BUFFER,
			.buffer_binding = {
				.buffer = &uniform_buffer,
			},
		}));

		if (static_model_component)
		{
			sb_push(resource_writes, ((GpuResourceWrite){
				.binding_index = 1,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &static_model_component->static_model.vertex_buffer,
				},
			}));
			
			sb_push(resource_writes, ((GpuResourceWrite){
				.binding_index = 2,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &static_model_component->static_model.index_buffer,
				},
			}));
		}
		else if(animated_model_component)
		{
			sb_push(resource_writes, ((GpuResourceWrite){
				.binding_index = 1,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &animated_model_component->animated_model.static_vertex_buffer,
				},
			}));
			
			sb_push(resource_writes, ((GpuResourceWrite){
				.binding_index = 2,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &animated_model_component->animated_model.index_buffer,
				},
			}));

			sb_push(resource_writes, ((GpuResourceWrite){
				.binding_index = 3,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &animated_model_component->animated_model.skinned_vertex_buffer,
				},
			}));

			sb_push(resource_writes, ((GpuResourceWrite){
				.binding_index = 4,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &animated_model_component->animated_model.inverse_bind_matrices_buffer,
				},
			}));

			sb_push(resource_writes, ((GpuResourceWrite){	
				.binding_index = 5,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &animated_model_component->joint_matrices_buffer,
				},	
			}));
		}

		const GpuBindGroupUpdateInfo per_object_bind_group_update_info = {
			.bind_group = &per_object_bind_group,
			.num_writes = sb_count(resource_writes),
			.writes = resource_writes,
		};
		gpu_update_bind_group(in_gpu_device, &per_object_bind_group_update_info);
		sb_push(object_render_data.bind_groups, per_object_bind_group);	
		sb_free(resource_writes);
	}

	OBJECT_CREATE_COMPONENT(ObjectRenderDataComponent, manager, object_handle, object_render_data);
}

//FCS TODO: Would like to get rid of render data component
//FCS TODO: Unify Static/Animated model components and put render data there (3 components down to 1)

