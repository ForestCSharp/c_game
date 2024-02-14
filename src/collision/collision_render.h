#pragma once

//Collision Render Data 
#include "collision.h"
#include "gpu/gpu.h"
#include "gpu/bindless.h"
#include "model/static_model.h"
#include "stretchy_buffer.h"

typedef struct ColliderRenderData
{
	i32 num_vertices;
	GpuBuffer vertex_buffer;
	StorageBufferHandle vertex_buffer_handle;
	i32 num_indices;
	GpuBuffer index_buffer;
	StorageBufferHandle index_buffer_handle;
} ColliderRenderData;


//FCS TODO: move any basic shape generation to its own file
void append_uv_sphere(Vec3 offset, f32 radius, i32 longitudes, i32 latitudes, sbuffer(StaticVertex)* out_vertices, sbuffer(u32)* out_indices)
{
	//Figure out where our indices should start
	const u32 index_offset = sb_count(*out_vertices);

	latitudes = MAX(2, latitudes);
	longitudes = MAX(3, longitudes);

	f32 deltaLatitude = PI / (f32) latitudes;
	f32 deltaLongitude = 2.0f * PI / (f32) longitudes;
	
	for (i32 i = 0; i <= latitudes; ++i)
	{
		f32 latitudeAngle = PI / 2.0f - i * deltaLatitude;	/* Starting -pi/2 to pi/2 */
		f32 xz = radius * cosf(latitudeAngle);			/* r * cos(phi) */
		f32 y = radius * sinf(latitudeAngle);			/* r * sin(phi )*/

		/*
			* We add (latitudes + 1) vertices per longitude because of equator,
			* the North pole and South pole are not counted here, as they overlap.
			* The first and last vertices have same position and normal, but
			* different tex coords.
		*/
		for (i32 j = 0; j <= longitudes; ++j)
		{
			f32 longitudeAngle = j * deltaLongitude;

			StaticVertex vertex;
		
			vertex.position = vec4_from_vec3(offset, 1.0);
			vertex.position.x += xz * cosf(longitudeAngle);	
			vertex.position.y += y;
			vertex.position.z += xz * sinf(longitudeAngle);	/* z = r * sin(phi) */
			vertex.uv.x = (float) j/longitudes;				/* s */
			vertex.uv.y = (float) i/latitudes;				/* t */

			const f32 lengthInv = 1.0f / radius; 
			vertex.normal.x = vertex.position.x * lengthInv;
			vertex.normal.y = vertex.position.y * lengthInv;
			vertex.normal.z = vertex.position.z * lengthInv;
			sb_push(*out_vertices, vertex);
		}
	}

	/*
	*  Indices
	*  k1--k1+1
	*  |  / |
	*  | /  |
	*  k2--k2+1
	*/
	unsigned int k1, k2;
	for(int i = 0; i < latitudes; ++i)
	{
		k1 = (i * (longitudes + 1));
		k2 = (k1 + longitudes + 1);
		// 2 Triangles per latitude block excluding the first and last longitudes blocks
		for(int j = 0; j < longitudes; ++j, ++k1, ++k2)
		{
			if (i != 0)
			{
				sb_push(*out_indices, index_offset + k1);
				sb_push(*out_indices, index_offset + k2);
				sb_push(*out_indices, index_offset + k1 + 1);
			}

			if (i != (latitudes - 1))
			{
				sb_push(*out_indices, index_offset + k1 + 1);
				sb_push(*out_indices, index_offset + k2);
				sb_push(*out_indices, index_offset + k2 + 1);
			}
		}
	}
}

//FCS TODO: Fix jagged edges on top and bottom of empty capsule...
void append_cylinder(Vec3 offset, f32 radius, f32 half_height, i32 longitudes, i32 latitudes, sbuffer(StaticVertex)* out_vertices, sbuffer(u32)* out_indices)
{
	//Figure out where our indices should start
	const u32 index_offset = sb_count(*out_vertices);

	latitudes = MAX(2, latitudes);
	longitudes = MAX(3, longitudes);

	f32 deltaLatitude = PI / (f32) latitudes;
	f32 deltaLongitude = 2.0f * PI / (f32) longitudes;
	
	for (i32 i = 0; i <= latitudes; ++i)
	{
		f32 latitudeAngle = PI / 2.0f - i * deltaLatitude;	
		f32 xz = radius;	
		f32 y = half_height * sinf(latitudeAngle);

		for (i32 j = 0; j <= longitudes; ++j)
		{
			f32 longitudeAngle = j * deltaLongitude;

			StaticVertex vertex;
		
			vertex.position = vec4_from_vec3(offset, 1.0);
			vertex.position.x += xz * cosf(longitudeAngle);	
			vertex.position.y += y;							
			vertex.position.z += xz * sinf(longitudeAngle);	
			vertex.uv.x = (float) j/longitudes;				
			vertex.uv.y = (float) i/latitudes;				

			const f32 lengthInv = 1.0f / radius; 
			vertex.normal.x = vertex.position.x * lengthInv;
			vertex.normal.y = vertex.position.y * lengthInv;
			vertex.normal.z = vertex.position.z * lengthInv;
			sb_push(*out_vertices, vertex);
		}
	}

	/*
	*  Indices
	*  k1--k1+1
	*  |  / |
	*  | /  |
	*  k2--k2+1
	*/
	unsigned int k1, k2;
	for(int i = 0; i < latitudes; ++i)
	{
		k1 = (i * (longitudes + 1));
		k2 = (k1 + longitudes + 1);
		// 2 Triangles per latitude block excluding the first and last longitudes blocks
		for(int j = 0; j < longitudes; ++j, ++k1, ++k2)
		{
			if (i != 0)
			{
				sb_push(*out_indices, index_offset + k1);
				sb_push(*out_indices, index_offset + k2);
				sb_push(*out_indices, index_offset + k1 + 1);
			}

			if (i != (latitudes - 1))
			{
				sb_push(*out_indices, index_offset + k1 + 1);
				sb_push(*out_indices, index_offset + k2);
				sb_push(*out_indices, index_offset + k2 + 1);
			}
		}
	}

}

void append_box(const Vec3 axes[3], const float half_extents[3], sbuffer(StaticVertex)* out_vertices, sbuffer(u32)* out_indices)
{
	u32 index_offset = sb_count(*out_vertices);

	for (i32 axis_idx = 0; axis_idx < 3; ++axis_idx)
	{
		Vec3 positive_quad_center = vec3_scale(axes[axis_idx], half_extents[axis_idx]);
		Vec3 negative_quad_center = vec3_scale(axes[axis_idx], -half_extents[axis_idx]);

		i32 other_axis_a = (axis_idx + 1) % 3;
		Vec3 offset_a = vec3_scale(axes[other_axis_a], half_extents[other_axis_a]);
		
		i32 other_axis_b = (axis_idx + 2) % 3;
		Vec3 offset_b = vec3_scale(axes[other_axis_b], half_extents[other_axis_b]);

		Vec3 positions[8] = 
		{
			vec3_add(vec3_add(positive_quad_center, offset_a), offset_b),
			vec3_add(vec3_sub(positive_quad_center, offset_a), offset_b),
			vec3_sub(vec3_add(positive_quad_center, offset_a), offset_b),
			vec3_sub(vec3_sub(positive_quad_center, offset_a), offset_b),
			vec3_add(vec3_add(negative_quad_center, offset_a), offset_b),
			vec3_add(vec3_sub(negative_quad_center, offset_a), offset_b),
			vec3_sub(vec3_add(negative_quad_center, offset_a), offset_b),
			vec3_sub(vec3_sub(negative_quad_center, offset_a), offset_b),
		};

		Vec2 uvs[8] = 
		{
			vec2_new(1.0, 0.0),
			vec2_new(1.0, 1.0),
			vec2_new(0.0, 1.0),
			vec2_new(0.0, 0.0),
			vec2_new(1.0, 0.0),
			vec2_new(1.0, 1.0),
			vec2_new(0.0, 1.0),
			vec2_new(0.0, 0.0),
		};

		for (i32 vtx_idx = 0; vtx_idx < 8; ++vtx_idx)
		{
			StaticVertex vtx = {
				.position = vec4_from_vec3(positions[vtx_idx], 1.0),	// Set up later
				.normal = vec4_from_vec3(axes[axis_idx], 0.0),			// Point in direction of axis	
				.uv = uvs[vtx_idx], 									// Set Up Later
			};
			sb_push(*out_vertices, vtx);
		}

		sb_push(*out_indices, index_offset + 0);
		sb_push(*out_indices, index_offset + 1);
		sb_push(*out_indices, index_offset + 2);
		sb_push(*out_indices, index_offset + 1);
		sb_push(*out_indices, index_offset + 2);
		sb_push(*out_indices, index_offset + 3);
		index_offset += 4;

		sb_push(*out_indices, index_offset + 0);
		sb_push(*out_indices, index_offset + 1);
		sb_push(*out_indices, index_offset + 2);
		sb_push(*out_indices, index_offset + 1);
		sb_push(*out_indices, index_offset + 2);
		sb_push(*out_indices, index_offset + 3);
		index_offset += 4;
	}
}

//FCS TODO: Draw Colliders using debug drawing framework
void collider_render_data_create(GpuContext* gpu_context, 
								 BindlessResourceManager* bindless_resource_manager,
								 const Collider* in_collider,
								 ColliderRenderData* out_render_data)
{
	assert(in_collider && out_render_data);

	sbuffer(StaticVertex) vertices = NULL;
	sbuffer(u32) indices = NULL;

	switch(in_collider->type)
	{
		case COLLIDER_TYPE_SPHERE:
		{
			//FCS TODO: 1 UV Sphere	
			append_uv_sphere(vec3_zero, in_collider->sphere.radius, 12, 12, &vertices, &indices);
			break;
		}
		case COLLIDER_TYPE_CAPSULE:
		{
			LineSegment segment = capsule_segment(in_collider->capsule);
			const Vec3 top_sphere_offset = vec3_sub(segment.start, in_collider->capsule.center); 
			append_uv_sphere(top_sphere_offset, in_collider->capsule.radius, 12, 12, &vertices, &indices);
			const Vec3 bottom_sphere_offset = vec3_sub(segment.end, in_collider->capsule.center);
			append_uv_sphere(bottom_sphere_offset, in_collider->capsule.radius, 12, 12, &vertices, &indices);
			const Vec3 capsule_offset = vec3_zero;
			append_cylinder(capsule_offset, in_collider->capsule.radius, in_collider->capsule.half_height, 12, 12, &vertices, &indices);
			break;
		}
		case COLLIDER_TYPE_OBB:
		{
			Vec3 obb_axes[3];
			obb_make_axes(in_collider->obb, obb_axes);
			append_box(obb_axes, in_collider->obb.halfwidths, &vertices, &indices);	
			break;
		}
	}

	// GPU Data Setup
	GpuBufferCreateInfo vertex_buffer_create_info = {
		.size = sizeof(StaticVertex) * sb_count(vertices),
		.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER | GPU_BUFFER_USAGE_TRANSFER_DST,
		.memory_properties = GPU_MEMORY_PROPERTY_DEVICE_LOCAL,
		.debug_name = "collision vertex buffer",
	};
	GpuBuffer vertex_buffer = gpu_create_buffer(gpu_context, &vertex_buffer_create_info);
	gpu_upload_buffer(gpu_context, &vertex_buffer, vertex_buffer_create_info.size, vertices);
	StorageBufferHandle vertex_buffer_handle = bindless_resource_manager_register_storage_buffer(gpu_context, bindless_resource_manager, &vertex_buffer);

	GpuBufferCreateInfo index_buffer_create_info = {
		.size = sizeof(u32) * sb_count(indices),
		.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER | GPU_BUFFER_USAGE_TRANSFER_DST,
		.memory_properties = GPU_MEMORY_PROPERTY_DEVICE_LOCAL,
		.debug_name = "collision index buffer",
	};
	GpuBuffer index_buffer = gpu_create_buffer(gpu_context, &index_buffer_create_info);
	gpu_upload_buffer(gpu_context, &index_buffer, index_buffer_create_info.size, indices);
	StorageBufferHandle index_buffer_handle = bindless_resource_manager_register_storage_buffer(gpu_context, bindless_resource_manager, &index_buffer);

	*out_render_data = (ColliderRenderData) {
		.num_vertices = sb_count(vertices),
		.vertex_buffer = vertex_buffer,
		.vertex_buffer_handle = vertex_buffer_handle,
		.num_indices = sb_count(indices),
		.index_buffer = index_buffer,
		.index_buffer_handle = index_buffer_handle,
	};
}

void collider_render_data_free(GpuContext* gpu_context, ColliderRenderData* in_render_data)
{
	assert(in_render_data);
	gpu_destroy_buffer(gpu_context, &in_render_data->vertex_buffer);
	gpu_destroy_buffer(gpu_context, &in_render_data->index_buffer);
}
