#pragma once

// Linear Complementarity Problem (LCP) solver using Gaussian-Seidel method

#include "math/basic_math.h"
#include "memory/arena.h"

typedef struct VecN
{
	f32* data;
	i32 n;
} VecN;

typedef struct MatN
{
	VecN* rows;
	i32 n;
} MatN;

typedef struct MatMN
{
	VecN* rows;
	i32 m; // Num Rows 
	i32 n; // Num Columns 
} MatMN;

// VecN Functions

i32 vecn_count(const VecN* in_vec_n)
{
	assert(in_vec_n);
	return in_vec_n->n;
}

VecN vecn_new(Arena* arena, const i32 in_num_elements)
{
	f32* data = (f32*) arena_alloc(arena, in_num_elements * sizeof(f32));
	assert(data);
	memset(data, 0, in_num_elements * sizeof(f32));

	VecN out_vec_n = {
		.data = data,
		.n = in_num_elements,
	};

	return out_vec_n;
}

VecN vecn_copy(Arena* arena, const VecN* in_vec_n)
{
	const i32 num_elements = vecn_count(in_vec_n);
	VecN out_vec_n = vecn_new(arena, num_elements);
	memcpy(out_vec_n.data, in_vec_n->data, num_elements * sizeof(f32));

	return out_vec_n;
}

void vecn_scale_in_place(VecN* in_vec_n, const f32 in_scale_factor)
{
	for (i32 idx = 0; idx < in_vec_n->n; ++idx)
	{
		in_vec_n->data[idx] *= in_scale_factor;
	}
}

VecN vecn_scale(Arena* arena, const VecN* in_vec_n, const f32 in_scale_factor)
{
	VecN out_vec_n = vecn_copy(arena, in_vec_n);
	vecn_scale_in_place(&out_vec_n, in_scale_factor);

	return out_vec_n;
}

void vecn_add_in_place(VecN* in_vec_lhs, const VecN* in_vec_rhs)
{	
	for (i32 idx = 0; idx < in_vec_lhs->n; ++idx)
	{
		in_vec_lhs->data[idx] += in_vec_rhs->data[idx];
	}
}

VecN vecn_add(Arena* arena, const VecN* in_vec_lhs, const VecN* in_vec_rhs)
{	
	VecN out_vec_n = vecn_copy(arena, in_vec_lhs);
	vecn_add_in_place(&out_vec_n, in_vec_rhs);

	return out_vec_n;
}

void vecn_sub_in_place(VecN* in_vec_lhs, const VecN* in_vec_rhs)
{		
	for (i32 idx = 0; idx < in_vec_lhs->n; ++idx)
	{
		in_vec_lhs->data[idx] -= in_vec_rhs->data[idx];
	}
}

VecN vecn_sub(Arena* arena, const VecN* in_vec_lhs, const VecN* in_vec_rhs)
{	
	VecN out_vec_n = vecn_copy(arena, in_vec_lhs);
	vecn_sub_in_place(&out_vec_n, in_vec_rhs);

	return out_vec_n;
}

f32 vecn_dot(const VecN* in_vec_lhs, const VecN* in_vec_rhs)
{
	f32 result = 0;	
	for (i32 idx = 0; idx < in_vec_lhs->n; ++idx)
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

// MatN Functions

MatN matn_new(Arena* arena, i32 in_num_elements)
{
	MatN out_mat_n = {
		.rows = (VecN*) arena_alloc(arena, in_num_elements * sizeof(VecN)),
		.n = in_num_elements,
	};
	assert(out_mat_n.rows);

	for (i32 i = 0; i < in_num_elements; ++i)
	{
		out_mat_n.rows[i] = vecn_new(arena, in_num_elements);
	}

	return out_mat_n;
}

i32 matn_num_dims(const MatN* in_mat_n)
{
	return in_mat_n->n;
}

MatN matn_copy(Arena* arena, const MatN* in_mat_n)
{
	MatN out_mat_n = {
		.rows = (VecN*) arena_alloc(arena, in_mat_n->n * sizeof(VecN)),
		.n = in_mat_n->n,
	};
	assert(out_mat_n.rows);

	for (i32 i = 0; i < in_mat_n->n; ++i)
	{
		out_mat_n.rows[i] = in_mat_n->rows[i];
	}	

	return out_mat_n;
}

void matn_zero_in_place(MatN* in_mat_n)
{
	const i32 num_dims = matn_num_dims(in_mat_n);
	for (i32 i = 0; i < num_dims; ++i)
	{
		vecn_zero_in_place(&in_mat_n->rows[i]);
	}	
}

MatN matn_zero(Arena* arena, const i32 in_dimensions)
{
	MatN out_mat = matn_new(arena, in_dimensions);
	matn_zero_in_place(&out_mat);
	return out_mat;
}

void matn_identity(MatN* in_mat_n)
{
	const i32 num_dims = matn_num_dims(in_mat_n);
	for (i32 i = 0; i < num_dims; ++i)
	{
		vecn_zero_in_place(&in_mat_n->rows[i]);
		in_mat_n->rows[i].data[i] = 1.0f;
	}	
}

void matn_transpose(Arena* arena, MatN* in_mat_n)
{
	MatN tmp = matn_copy(arena, in_mat_n);

	const i32 num_dimensions = matn_num_dims(in_mat_n);
	for (i32 i = 0; i < num_dimensions; ++i)
	{
		for (i32 j = 0; j < num_dimensions; ++j)
		{
			in_mat_n->rows[j].data[i] = tmp.rows[i].data[j];
		}
	}
}

void matn_scale_in_place(MatN* in_mat_n, const f32 in_scale)
{
	i32 num_dims = matn_num_dims(in_mat_n);
	for (i32 i = 0; i < num_dims; ++i)
	{
		vecn_scale_in_place(&in_mat_n->rows[i], in_scale);
	}	
}

VecN matn_mul_vecn(Arena* arena, const MatN* in_mat_n, const VecN* in_vec_n)
{	
	const i32 num_dims = matn_num_dims(in_mat_n);
	assert(num_dims == vecn_count(in_vec_n));

	VecN out_vec_n = vecn_new(arena, num_dims);
	for (i32 i = 0; i < num_dims; ++i)
	{
		out_vec_n.data[i] = vecn_dot(&in_mat_n->rows[i], in_vec_n);
	}

	return out_vec_n;
}

MatN matn_mul_matn(Arena* arena, const MatN* in_lhs, const MatN* in_rhs)
{
	const i32 n = matn_num_dims(in_lhs);
	assert(n == matn_num_dims(in_rhs));

	MatN out_mat_n = matn_zero(arena, n);

	for (i32 i = 0; i < n; ++i)
	{
		for (i32 j = 0; j < n; ++j)
		{
			f32 sum = 0.0f;
			for (i32 k = 0; k < n; ++k)
			{
				sum += in_lhs->rows[i].data[k] * in_rhs->rows[k].data[j];
			}
			out_mat_n.rows[i].data[j] = sum;
		}
	}

	return out_mat_n;
}

// MatMN Functions

MatMN matmn_new(Arena* arena, i32 in_m, i32 in_n)
{
	MatMN out_mat_mn = {
		.rows = (VecN*) arena_alloc(arena, in_m * sizeof(VecN)),
		.m = in_m,
		.n = in_n,
	};
	assert(out_mat_mn.rows);

	for (i32 i = 0; i < in_m; ++i)
	{
		out_mat_mn.rows[i] = vecn_new(arena, in_n);
	}

	return out_mat_mn;
}

void matmn_zero_in_place(MatMN* in_mat_mn)
{
	for (i32 i = 0; i < in_mat_mn->m; ++i)
	{
		vecn_zero_in_place(&in_mat_mn->rows[i]);
	}
}

MatMN matmn_zero(Arena* arena, i32 in_m, i32 in_n)
{
	MatMN out_mat_mn = matmn_new(arena, in_m, in_n);
	matmn_zero_in_place(&out_mat_mn);
	return out_mat_mn;
}

MatMN matmn_copy(Arena* arena, const MatMN* in_mat_mn)
{
	MatMN out_mat_mn = {
		.rows = (VecN*) arena_alloc(arena, in_mat_mn->m * sizeof(VecN)),
		.m = in_mat_mn->m,
		.n = in_mat_mn->n,
	};
	assert(out_mat_mn.rows);

	for (i32 i = 0; i < in_mat_mn->m; ++i)
	{	
		out_mat_mn.rows[i] = vecn_copy(arena, &in_mat_mn->rows[i]);
	}

	return out_mat_mn;
}

void matmn_scale_in_place(MatMN* in_mat_mn, const f32 in_scale)
{
	for (i32 i = 0; i < in_mat_mn->m; ++i)
	{
		vecn_scale_in_place(&in_mat_mn->rows[i], in_scale);
	}	
}

VecN matmn_mul_vecn(Arena* arena, const MatMN* in_mat_mn, const VecN* in_vec_n)
{
	if (in_mat_mn->n != vecn_count(in_vec_n))	
	{
		assert(false);
		return vecn_copy(arena, in_vec_n);
	}

	VecN out_vec_n = vecn_new(arena, in_mat_mn->m);
	for (i32 m = 0; m < in_mat_mn->m; ++m)
	{
		out_vec_n.data[m] = vecn_dot(in_vec_n, &in_mat_mn->rows[m]);
	}
	return out_vec_n;
}

MatMN matmn_transpose(Arena* arena, const MatMN* in_mat_mn)
{
	MatMN out_mat_mn = matmn_new(arena, in_mat_mn->n, in_mat_mn->m);
		
	for (i32 m = 0; m < in_mat_mn->m; ++m)
	{
		for (i32 n = 0; n < in_mat_mn->n; ++n)
		{
			out_mat_mn.rows[n].data[m] = in_mat_mn->rows[m].data[n];
		}
	}

	return out_mat_mn;
}

MatMN matmn_mul_matmn(Arena* arena, const MatMN* in_lhs, const MatMN* in_rhs)
{
	assert(in_lhs->n == in_rhs->m);

	MatMN out_mat_mn = matmn_new(arena, in_lhs->m, in_rhs->n);

	MatMN transposed_rhs = matmn_transpose(arena, in_rhs);
	for (i32 m = 0; m < in_lhs->m; ++m)
	{
		for (i32 n = 0; n < in_rhs->n; ++n)
		{
			out_mat_mn.rows[m].data[n] = vecn_dot(&in_lhs->rows[m], &transposed_rhs.rows[n]);
		}
	}

	return out_mat_mn;
}

MatN matn_from_matmn(Arena* arena, const MatMN* in_mat_mn)
{
	assert(in_mat_mn->m == in_mat_mn->n); // must be square
	const i32 out_dimensions = in_mat_mn->m;
	MatN out_mat_n = {
		.rows = (VecN*) arena_alloc(arena, out_dimensions * sizeof(VecN)),
		.n = out_dimensions,
	};
	assert(out_mat_n.rows);

	for (i32 i = 0; i < out_dimensions; ++i)
	{
		out_mat_n.rows[i] = vecn_copy(arena, &in_mat_mn->rows[i]);
	}

	return out_mat_n;
}

// Gaussian-Seidel solver
VecN lcp_gauss_seidel(Arena* arena, const MatN* in_mat_n, const VecN* in_vec_n)
{
	const i32 n = vecn_count(in_vec_n);
	VecN out_vec_n = vecn_new(arena, n);
	vecn_zero_in_place(&out_vec_n);

	for (i32 iter = 0; iter < n; ++iter)
	{
		for (i32 i = 0; i < n; ++i)
		{
			f32 dot_result = vecn_dot(&in_mat_n->rows[i], &out_vec_n);
			f32 dx = (in_vec_n->data[i] - dot_result) / in_mat_n->rows[i].data[i];
			if (dx * 0.f == dx * 0.f)
			{
				out_vec_n.data[i] += dx;
			}
		}
	}

	return out_vec_n;
}
