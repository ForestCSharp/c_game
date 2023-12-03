#pragma once


#include "math/matrix.h"
#include "model/static_model.h"
#include "stretchy_buffer.h"
#include "types.h"

const i64 INVALID_INDEX = -1;

#define COMPONENT_TYPE(type) COMPONENT_TYPE_##type
#define COMPONENT_ENTRY_STRUCT_NAME(type) struct ComponentEntry##type
#define COMPONENT_DATA_VAR_NAME(type) type##_storage

	//sbuffer(type) component_array;\

#define DEFINE_COMPONENT_STORAGE(type)\
struct {\
	sbuffer(COMPONENT_ENTRY_STRUCT_NAME(type){\
		type component;\
		i64 ref_count;\
	}) component_array;\
	sbuffer(i64) free_list;\
} COMPONENT_DATA_VAR_NAME(type);

#define DEFINE_COMPONENT_INTERFACE(type) \
	type* game_object_manager_get_##type(GameObjectManager* manager, i64 component_idx)\
	{\
		bool is_valid_index = component_idx >= 0 && component_idx < sb_count(manager->COMPONENT_DATA_VAR_NAME(type).component_array);\
		return is_valid_index ? &manager->COMPONENT_DATA_VAR_NAME(type).component_array[component_idx].component : NULL;\
	}\
	type* game_object_manager_get_##type##_from_object(GameObjectManager* manager, GameObject* object)\
	{\
		const i64 component_idx = object->component_ids[COMPONENT_TYPE(type)];\
		return game_object_manager_get_##type(manager, component_idx);\
	}\
	type* game_object_manager_create_##type(GameObjectManager* manager, type value)\
	{\
		if (sb_count(manager->COMPONENT_DATA_VAR_NAME(type).free_list) > 0)\
		{\
			i64 found_idx = manager->COMPONENT_DATA_VAR_NAME(type).free_list[0];\
			manager->COMPONENT_DATA_VAR_NAME(type).component_array[found_idx].component = value;\
			sb_del(manager->COMPONENT_DATA_VAR_NAME(type).free_list, 0);\
			return &manager->COMPONENT_DATA_VAR_NAME(type).component_array[found_idx].component;\
		}\
		else\
		{\
			COMPONENT_ENTRY_STRUCT_NAME(type) new_component_entry = {.component = value, .ref_count = 0};\
			sb_push(manager->COMPONENT_DATA_VAR_NAME(type).component_array, new_component_entry);\
			const i64 new_idx = sb_count(manager->COMPONENT_DATA_VAR_NAME(type).component_array) - 1;\
			return &manager->COMPONENT_DATA_VAR_NAME(type).component_array[new_idx].component;\
		}\
	}\
	void game_object_manager_destroy_##type(GameObjectManager* manager, i64 idx)\
	{\
		assert(idx > INVALID_INDEX && idx < sb_count(manager->COMPONENT_DATA_VAR_NAME(type).component_array));\
		assert(manager->COMPONENT_DATA_VAR_NAME(type).component_array[idx].ref_count == 0);\
		sb_push(manager->COMPONENT_DATA_VAR_NAME(type).free_list, idx);\
	}\
	void game_object_manager_set_##type(GameObjectManager* manager, GameObject* object, type* component)\
	{\
		i64 new_idx = component ? (COMPONENT_ENTRY_STRUCT_NAME(type)*) component - manager->COMPONENT_DATA_VAR_NAME(type).component_array : INVALID_INDEX;\
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
			printf("old_idx: %lli, new_idx: %lli\n", old_idx, new_idx);\
			object->component_ids[COMPONENT_TYPE(type)] = new_idx;\
		}\
	}\

#define ADD_OBJECT(manager_ptr) game_object_manager_add_object(manager_ptr)
#define REMOVE_OBJECT(manager_ptr, object_ptr) game_object_manager_remove_object(manager_ptr, object_ptr)

#define CREATE_COMPONENT(type, manager_ptr, value) game_object_manager_create_##type(manager_ptr, value)

// Used to set/unset comopnents on an object
#define OBJECT_SET_COMPONENT(type, manager_ptr, object_ptr, component_ptr) game_object_manager_set_##type(manager_ptr, object_ptr, component_ptr)

#define GET_COMPONENT(type, manager_ptr, component_idx) game_object_manager_get_##type(manager_ptr, component_idx)
#define OBJECT_GET_COMPONENT(type, manager_ptr, object_ptr) game_object_manager_get_##type##_from_object(manager_ptr, object_ptr)

//FCS TODO: Function to compact GameObject and Component arrays (properly fixes up references...)
//FCS TODO: Automatically DESTROY_COMPONENT when its ref_count reaches zero as a result of OBJECT_SET_COMPONENT calls...


typedef struct TransformComponent
{
	TRS trs;
} TransformComponent;

typedef struct StaticModelComponent
{
	StaticModel static_model;
} StaticModelComponent;

typedef struct ObjectRenderDataComponent
{
	sbuffer(GpuBuffer) uniform_buffers;
	sbuffer(GpuDescriptorSet) descriptor_sets;
} ObjectRenderDataComponent;

// To add a new component: 
// 1. Add to ComponentType enum: COMPONENT_TYPE(ComponentStruct)
// 2. DEFINE_COMPONENT_STORAGE(ComponentStruct)
// 3. DEFINE_COMPONENT_INTERFACE(ComponentStruct)

typedef enum ComponentType
{
	COMPONENT_TYPE(TransformComponent),
	COMPONENT_TYPE(StaticModelComponent),
	COMPONENT_TYPE(ObjectRenderDataComponent),
	COMPONENT_TYPE_COUNT,
} ComponentType;

typedef struct GameObject
{
	i64 component_ids[COMPONENT_TYPE_COUNT];
} GameObject;

typedef struct GameObjectManager
{
	sbuffer(GameObject) game_object_array;
	
	DEFINE_COMPONENT_STORAGE(TransformComponent);
	DEFINE_COMPONENT_STORAGE(StaticModelComponent);
	DEFINE_COMPONENT_STORAGE(ObjectRenderDataComponent);

} GameObjectManager;

DEFINE_COMPONENT_INTERFACE(TransformComponent);
DEFINE_COMPONENT_INTERFACE(StaticModelComponent);
DEFINE_COMPONENT_INTERFACE(ObjectRenderDataComponent);


void game_object_manager_init(GameObjectManager* manager)
{
	assert(manager);
	*manager = (GameObjectManager){};
}

void game_object_manager_destroy(GameObjectManager* manager)
{
	assert(manager);

	sb_free(manager->game_object_array);
	sb_free(manager->COMPONENT_DATA_VAR_NAME(TransformComponent).component_array);
	sb_free(manager->COMPONENT_DATA_VAR_NAME(StaticModelComponent).component_array);
}

GameObject* game_object_manager_add_object(GameObjectManager* manager)
{	
	GameObject new_object = {};
	memset(new_object.component_ids, -1, sizeof(i64) * COMPONENT_TYPE_COUNT);

	i64 new_idx = sb_count(manager->game_object_array);
	sb_push(manager->game_object_array, new_object);
	return &manager->game_object_array[new_idx];
}

void game_object_manager_remove_object(GameObjectManager* manager, GameObject* object)
{
	i64 obj_idx = object - manager->game_object_array;
	assert(obj_idx >= 0 && obj_idx < sb_count(manager->game_object_array));
	sb_del(manager->game_object_array, obj_idx);
}

