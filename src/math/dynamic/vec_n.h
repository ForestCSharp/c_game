#pragma once

#include "math/basic_math.h"
#include "stretchy_buffer.h"

typedef struct VecN
{
	sbuffer(f32) data;	
} VecN;

i32 vecn_count(const VecN* in_vec_n)
{
	assert(in_vec_n);
	return sb_count(in_vec_n->data);
}

VecN vecn_new(const i32 in_num_elements)
{
	sbuffer(f32) data = NULL;
	sb_add(data, in_num_elements);

	return (VecN) {
		.data = data,
	};
}

VecN vecn_copy(const VecN* in_vec_n)
{
	const i32 num_elements = vecn_count(in_vec_n);
	VecN out_vec_n = vecn_new(num_elements);
	memcpy(out_vec_n.data, in_vec_n->data, num_elements * sizeof(f32));
	return out_vec_n;
}

void vecn_destroy(VecN* in_vec_n)
{
	assert(in_vec_n);
	sb_free(in_vec_n->data);
}

void vecn_scale_in_place(VecN* in_vec_n, const f32 in_scale_factor)
{
	for (i32 idx = 0; idx < sb_count(in_vec_n->data); ++idx)
	{
		in_vec_n->data[idx] *= in_scale_factor;
	}
}

VecN vecn_scale(const VecN* in_vec_n, const f32 in_scale_factor)
{
	VecN out_vec_n = vecn_copy(in_vec_n);
	vecn_scale_in_place(&out_vec_n, in_scale_factor);
	return out_vec_n;
}

void vecn_add_in_place(VecN* in_vec_lhs, const VecN* in_vec_rhs)
{	
	for (i32 idx = 0; idx < sb_count(in_vec_lhs->data); ++idx)
	{
		in_vec_lhs->data[idx] += in_vec_rhs->data[idx];
	}
}

VecN vecn_add(const VecN* in_vec_lhs, const VecN* in_vec_rhs)
{	
	VecN out_vec_n = vecn_copy(in_vec_lhs);
	vecn_add_in_place(&out_vec_n, in_vec_rhs);
	return out_vec_n;
}

void vecn_sub_in_place(VecN* in_vec_lhs, const VecN* in_vec_rhs)
{		
	for (i32 idx = 0; idx < sb_count(in_vec_lhs->data); ++idx)
	{
		in_vec_lhs->data[idx] -= in_vec_rhs->data[idx];
	}
}

VecN vecn_sub(const VecN* in_vec_lhs, const VecN* in_vec_rhs)
{	
	VecN out_vec_n = vecn_copy(in_vec_lhs);
	vecn_sub_in_place(&out_vec_n, in_vec_rhs);
	return out_vec_n;
}

f32 vecn_dot(const VecN* in_vec_lhs, const VecN* in_vec_rhs)
{
	f32 result = 0;	
	for (i32 idx = 0; idx < sb_count(in_vec_lhs->data); ++idx)
	{
		result += in_vec_lhs->data[idx] * in_vec_rhs->data[idx];
	}
	return result;
}

void vecn_zero_in_place(VecN* in_vec_n)
{	
	u64 data_size = sizeof(f32) * vecn_count(in_vec_n);
	memset(in_vec_n->data, 0, data_size);
}
