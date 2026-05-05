#pragma once

//FCS TODO: these VecN, MatN, MatMN matrices don't change after init. 
//			consider some sort of 'template' system rather than allocating

// Linear Complementarity Problem (LCP) solver using Gaussian-Seidel method

#include "math/basic_math.h"
#include "stretchy_buffer.h"

typedef struct VecN
{
	sbuffer(f32) data;
} VecN;

typedef struct MatN
{
	sbuffer(VecN) rows;	
} MatN;

typedef struct MatMN
{
	sbuffer(VecN) rows;	
	i32 m; // Num Rows 
	i32 n; // Num Columns 
} MatMN;

typedef struct LCP_Operation
{
	bool in_operation;
	sbuffer(VecN) vec_n_array;
	sbuffer(MatN) mat_n_array;
	sbuffer(MatMN) mat_mn_array;
} LCP_Operation;

//FCS TODO: Need lcp_op_stack support

_Thread_local LCP_Operation current_lcp_op = {
	.in_operation = false,
	.vec_n_array = NULL,
	.mat_n_array = NULL,
	.mat_mn_array = NULL,
};

bool LCP_IN_OP()
{
	return current_lcp_op.in_operation;
}

void LCP_OP_VECN(const VecN* in_vec_n)
{
	if (LCP_IN_OP())
	{
		sb_push(current_lcp_op.vec_n_array, *in_vec_n);
	}
}

void LCP_OP_BEGIN()
{
	assert(current_lcp_op.in_operation == false);
	current_lcp_op.in_operation = true;
}

void vecn_destroy(VecN* in_vec_n);
void matn_destroy(MatN* in_mat_n);
void matmn_destroy(MatMN* in_mat_mn);

//FCS TODO: Verify this
void LCP_OP_END()
{
	assert(current_lcp_op.in_operation == true);
	
	for (i32 i = 0; i < sb_count(current_lcp_op.vec_n_array); ++i)
	{
		vecn_destroy(&current_lcp_op.vec_n_array[i]);
	}
	sb_free(current_lcp_op.vec_n_array);

	/* The VecNs of any registered MatN and MatMN have already been destroyed above,
	   so we just need to clear their rows arrays now... */

	for (i32 i = 0; i < sb_count(current_lcp_op.mat_n_array); ++i)
	{
		sb_free(current_lcp_op.mat_n_array[i].rows);
	}
	sb_free(current_lcp_op.mat_n_array);

	for (i32 i = 0; i < sb_count(current_lcp_op.mat_mn_array); ++i)
	{
		sb_free(current_lcp_op.mat_mn_array[i].rows);
	}
	sb_free(current_lcp_op.mat_mn_array);

	current_lcp_op = (LCP_Operation) {
		.in_operation = false,
		.vec_n_array = NULL,
		.mat_n_array = NULL,
		.mat_mn_array = NULL,
	};
}

// VecN Functions

i32 vecn_count(const VecN* in_vec_n)
{
	assert(in_vec_n);
	return sb_count(in_vec_n->data);
}

VecN vecn_new(const i32 in_num_elements)
{
	sbuffer(f32) data = NULL;
	sb_add(data, in_num_elements);

	VecN out_vec_n = {
		.data = data,
	};

	LCP_OP_VECN(&out_vec_n);

	return out_vec_n;
}

VecN vecn_copy(const VecN* in_vec_n)
{
	const i32 num_elements = vecn_count(in_vec_n);
	VecN out_vec_n = vecn_new(num_elements);
	memcpy(out_vec_n.data, in_vec_n->data, num_elements * sizeof(f32));

	LCP_OP_VECN(&out_vec_n);

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

	LCP_OP_VECN(&out_vec_n);

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

	LCP_OP_VECN(&out_vec_n);

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

	LCP_OP_VECN(&out_vec_n);
	
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

// MatN Functions

MatN matn_new(i32 in_num_elements)
{
	MatN out_mat_n = {};

	for (i32 i = 0; i < in_num_elements; ++i)
	{
		sb_push(out_mat_n.rows, vecn_new(in_num_elements));
	}

	return out_mat_n;
}

i32 matn_num_dims(const MatN* in_mat_n)
{
	return sb_count(in_mat_n->rows);
}

void matn_destroy(MatN* in_mat_n)
{	
	i32 num_dims = matn_num_dims(in_mat_n);
	for (i32 i = 0; i < num_dims; ++i)
	{
		vecn_destroy(&in_mat_n->rows[i]);
	}
	sb_free(in_mat_n->rows);
}

MatN matn_copy(const MatN* in_mat_n)
{
	MatN out_mat_n = {};

	i32 num_dims = matn_num_dims(in_mat_n);
	for (i32 i = 0; i < num_dims; ++i)
	{
		sb_push(out_mat_n.rows, in_mat_n->rows[i]);
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

MatN matn_zero(const i32 in_dimensions)
{
	MatN out_mat = matn_new(in_dimensions);
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

void matn_transpose(MatN* in_mat_n)
{
	MatN tmp = matn_copy(in_mat_n);

	const i32 num_dimensions = matn_num_dims(in_mat_n);
	for (i32 i = 0; i < num_dimensions; ++i)
	{
		for (i32 j = 0; j < num_dimensions; ++j)
		{
			in_mat_n->rows[j].data[i] = tmp.rows[i].data[j];
		}
	}

	matn_destroy(&tmp);
}

void matn_scale_in_place(MatN* in_mat_n, const f32 in_scale)
{
	i32 num_dims = matn_num_dims(in_mat_n);
	for (i32 i = 0; i < num_dims; ++i)
	{
		vecn_scale_in_place(&in_mat_n->rows[i], in_scale);
	}	
}

VecN matn_mul_vecn(const MatN* in_mat_n, const VecN* in_vec_n)
{	
	const i32 num_dims = matn_num_dims(in_mat_n);
	assert(num_dims == vecn_count(in_vec_n));

	VecN out_vec_n = vecn_new(num_dims);
	for (i32 i = 0; i < num_dims; ++i)
	{
		out_vec_n.data[i] = vecn_dot(&in_mat_n->rows[i], in_vec_n);
	}

	return out_vec_n;
}

MatN matn_mul_matn(const MatN* in_lhs, const MatN* in_rhs)
{
	const i32 n = matn_num_dims(in_lhs);
	assert(n == matn_num_dims(in_rhs));

	MatN out_mat_n = matn_zero(n);

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

MatMN matmn_new(i32 in_m, i32 in_n)
{
	MatMN out_mat_mn = {
		.m = in_m,
		.n = in_n,
	};

	for (i32 i = 0; i < in_m; ++i)
	{
		sb_push(out_mat_mn.rows, vecn_new(in_n));
	}

	return out_mat_mn;
}

void matmn_destroy(MatMN* in_mat_mn)
{	
	for (i32 i = 0; i < in_mat_mn->m; ++i)
	{
		vecn_destroy(&in_mat_mn->rows[i]);
	}
	sb_free(in_mat_mn->rows);
}

void matmn_zero_in_place(MatMN* in_mat_mn)
{
	for (i32 i = 0; i < in_mat_mn->m; ++i)
	{
		vecn_zero_in_place(&in_mat_mn->rows[i]);
	}
}

MatMN matmn_zero(i32 in_m, i32 in_n)
{
	MatMN out_mat_mn = matmn_new(in_m, in_n);
	matmn_zero_in_place(&out_mat_mn);
	return out_mat_mn;
}

MatMN matmn_copy(const MatMN* in_mat_mn)
{
	MatMN out_mat_mn ={
		.m = in_mat_mn->m,
		.n = in_mat_mn->n,
	};

	for (i32 i = 0; i < in_mat_mn->m; ++i)
	{	
		sb_push(out_mat_mn.rows, vecn_copy(&in_mat_mn->rows[i]));
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

VecN matmn_mul_vecn(const MatMN* in_mat_mn, const VecN* in_vec_n)
{
	if (in_mat_mn->n != vecn_count(in_vec_n))	
	{
		assert(false);
		return vecn_copy(in_vec_n);
	}

	VecN out_vec_n = vecn_new(in_mat_mn->m);
	for (i32 m = 0; m < in_mat_mn->m; ++m)
	{
		out_vec_n.data[m] = vecn_dot(in_vec_n, &in_mat_mn->rows[m]);
	}
	return out_vec_n;
}

MatMN matmn_transpose(const MatMN* in_mat_mn)
{
	MatMN out_mat_mn = matmn_new(in_mat_mn->n, in_mat_mn->m);
		
	for (i32 m = 0; m < in_mat_mn->m; ++m)
	{
		for (i32 n = 0; n < in_mat_mn->n; ++n)
		{
			out_mat_mn.rows[n].data[m] = in_mat_mn->rows[m].data[n];
		}
	}

	return out_mat_mn;
}

MatMN matmn_mul_matmn(const MatMN* in_lhs, const MatMN* in_rhs)
{
	assert(in_lhs->n == in_rhs->m);

	MatMN out_mat_mn = matmn_new(in_lhs->m, in_rhs->n);

	MatMN transposed_rhs = matmn_transpose(in_rhs);
	for (i32 m = 0; m < in_lhs->m; ++m)
	{
		for (i32 n = 0; n < in_rhs->n; ++n)
		{
			out_mat_mn.rows[m].data[n] = vecn_dot(&in_lhs->rows[m], &transposed_rhs.rows[n]);
		}
	}
	matmn_destroy(&transposed_rhs);

	return out_mat_mn;
}

MatN matn_from_matmn(const MatMN* in_mat_mn)
{
	assert(in_mat_mn->m == in_mat_mn->n); // must be square
	const i32 out_dimensions = in_mat_mn->m;
	MatN out_mat_n = {};
	for (i32 i = 0; i < out_dimensions; ++i)
	{
		sb_push(out_mat_n.rows, vecn_copy(&in_mat_mn->rows[i]));
	}
	return out_mat_n;
}

// Gaussian-Seidel solver
VecN lcp_gauss_seidel(const MatN* in_mat_n, const VecN* in_vec_n)
{
	const i32 n = vecn_count(in_vec_n);
	VecN out_vec_n = vecn_new(n);
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

