#pragma once

#include "math/matrix.h"
#include "math/conversions.h"

typedef struct TRS
{
	Vec3 scale;
	Quat rotation;
	Vec3 translation;
} TRS;

const TRS trs_identity = {
	.scale = {.x = 1, .y = 1, .z = 1},
	.rotation = {.x = 0, .y = 0, .z = 0, .w = 1},
	.translation = {.x = 0, .y = 0, .z = 0},
};

TRS mat4_to_trs(Mat4 in_mat)
{
	TRS out_trs;

	out_trs.translation = vec4_xyz(in_mat.columns[3]);
	
	out_trs.scale.x = vec3_length(vec4_xyz(in_mat.columns[0]));
	out_trs.scale.y = vec3_length(vec4_xyz(in_mat.columns[1]));
	out_trs.scale.z = vec3_length(vec4_xyz(in_mat.columns[2]));
	
	const Mat4 rotation_matrix = {
		.columns = {
			vec4_scale(in_mat.columns[0], 1.0f / out_trs.scale.x),
			vec4_scale(in_mat.columns[1], 1.0f / out_trs.scale.y),	
			vec4_scale(in_mat.columns[2], 1.0f / out_trs.scale.z),
			vec4_new(0,0,0,1),
		}
	};
	out_trs.rotation = mat4_to_quat(rotation_matrix);

	return out_trs;
}

Mat4 trs_to_mat4(const TRS trs)
{
	return mat4_mul_mat4(
		mat4_mul_mat4(mat4_scale(trs.scale), quat_to_mat4(trs.rotation)), 
		mat4_translation(trs.translation)
	);
}

TRS trs_lerp(float t, const TRS a, const TRS b)
{
	return (TRS) {
		.scale = vec3_lerp(t, a.scale, b.scale),
		.rotation = quat_nlerp(t, a.rotation, b.rotation),
		.translation = vec3_lerp(t, a.translation, b.translation),
	};
}

TRS trs_combine(const TRS parent, const TRS child)
{
	TRS out_trs = {};
	out_trs.scale = vec3_mul_componentwise(parent.scale, child.scale);

	const Quat scaled_parent_rotation = quat_normalize(quat_mul(parent.rotation, mat4_to_quat(mat4_scale(parent.scale))));
	out_trs.rotation = quat_normalize(quat_mul(scaled_parent_rotation, child.rotation));

	const Vec3 scaled_translation = vec3_mul_componentwise(parent.scale, child.translation);
	const Vec3 rotated_translation = quat_rotate_vec3(parent.rotation, scaled_translation);
	out_trs.translation = vec3_add(parent.translation, rotated_translation);

	return out_trs;
}
