//MPR in C: Adapted from Gary Snethen's XenoCollide MPR reference implementation http://xenocollide.com

#pragma once

#include <stdio.h>
#include <math.h>

typedef struct MprVec3 { float x; float y; float z; } MprVec3;

MprVec3 mpr_vec3_add(MprVec3 a, MprVec3 b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
MprVec3 mpr_vec3_sub(MprVec3 a, MprVec3 b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; return a; }
MprVec3 mpr_vec3_negate(MprVec3 v) { v.x = -v.x; v.y = -v.y; v.z = -v.z; return v; }
MprVec3 mpr_vec3_scale(MprVec3 v, float a) { v.x *= a; v.y *= a; v.z *= a; return v; }
float mpr_vec3_dot(MprVec3 a, MprVec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
MprVec3 mpr_vec3_cross(MprVec3 a, MprVec3 b) { 
    MprVec3 r = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
    return r;
}
float mpr_vec3_length_squared(MprVec3 v) { return v.x * v.x + v.y * v.y + v.z * v.z; }
float mpr_vec3_length(MprVec3 v) { return sqrt(mpr_vec3_length_squared(v)); }
MprVec3 mpr_vec3_normalize(MprVec3 v) { float l = mpr_vec3_length(v); v.x /= l; v.y /= l; v.z /= l; return v; }
int mpr_vec3_is_zero(MprVec3 v) { return v.x == 0 && v.y == 0 && v.z == 0; }
void mpr_vec3_swap(MprVec3* a, MprVec3* b) {
    MprVec3 tmp = *a;
    *a = *b;
    *b = tmp;
}

// [ column-major
//     0, 4, 8,  12,
//     1, 5, 9,  13,
//     2, 6, 10, 14,
//     3, 7, 11, 15,
// ]
MprVec3 mpr_mat4_mult_vec3(const float m[/*static*/ 16], MprVec3 v) {
    MprVec3 result;
    result.x = m[0] * v.x + m[4] * v.y + m[8]  * v.z + m[12] * 1.0;
    result.y = m[1] * v.x + m[5] * v.y + m[9]  * v.z + m[13] * 1.0;
    result.z = m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * 1.0;
    float w  = m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * 1.0;
    result = mpr_vec3_scale(result, 1/w);
    return result;
}

MprVec3 mpr_average_point(const MprVec3* vertices, size_t count) {
    MprVec3 avg = { 0.f, 0.f, 0.f };
    for (size_t i = 0; i < count; i++) {
        avg.x += vertices[i].x;
        avg.y += vertices[i].y;
        avg.z += vertices[i].z;
    }
    avg.x /= count;
    avg.y /= count;
    avg.z /= count;
    return avg;
}

size_t index_of_furthest_point(const MprVec3 * vertices, size_t count, const float transform[16], MprVec3 d) {
    
    float maxProduct = mpr_vec3_dot(d, mpr_mat4_mult_vec3(transform, vertices[0]));
    size_t index = 0;
    for (size_t i = 1; i < count; i++) {
        float product = mpr_vec3_dot(d, mpr_mat4_mult_vec3(transform, vertices[i]));
        if (product > maxProduct) {
            maxProduct = product;
            index = i;
        }
    }
    return index;
}

MprVec3 convex_support(const MprVec3 * vertices, size_t count, const float transform[16], MprVec3 d) {
    return mpr_mat4_mult_vec3(transform,vertices[index_of_furthest_point(vertices, count, transform, d)]);
}

// TODO: Penetration Depth (bool to check for this, passed into mpr_check_collision?)
// Given that, let's discuss how to detect the contact normal and penetration depth along that normal using MPR on two shapes, A and B.
//
// The easiest approach is to first determine that the origin is within A-B (the Minkowski difference). You can do this in 2D using the 
// MPR2D algorithm on my website or the 3D equivalent in Game Programming Gems 7 ("XenoCollide: Complex Collision Made Simple").
//
// Once you've identified that the origin is within A-B, you know there's a collision. You can then continue casting the origin ray to 
// the boundary of A-B. Once you hit the surface, the normal of the portal is your contact normal.
//
// You now need to find the penetration depth along this contact normal. To do that, run MPR again, using the contact normal as the 
// direction of your origin ray. (You can do this by using "origin - contactNormal" as your interior point.) The distance between the
// origin and the surface of A-B along the direction of the contact normal is the penetration depth.

static float K_COLLIDE_EPSILON = 1e-3f;

int mpr_check_collision(const MprVec3* convexA, size_t countA, const float transformA[16],
                        const MprVec3* convexB, size_t countB, const float transformB[16],
                        MprVec3* out_normal, MprVec3* out_contact_a, MprVec3* out_contact_b) {
    
    // v0 - center of Minkowski sum
    MprVec3 v01 = mpr_mat4_mult_vec3(transformA, mpr_average_point(convexA, countA));
    MprVec3 v02 = mpr_mat4_mult_vec3(transformB, mpr_average_point(convexB, countB));
    MprVec3 v0 = mpr_vec3_sub(v02, v01);

    // Avoid case where centers overlap -- any direction is fine in this case
    if (mpr_vec3_is_zero(v0)) { v0.x = 0.00001f; }
    
    MprVec3 dir = mpr_vec3_negate(v0);

    // v1 - support in direction of origin
    MprVec3 v11 = convex_support(convexA, countA, transformA, mpr_vec3_negate(dir));
	MprVec3 v12 = convex_support(convexB, countB, transformB, dir);
    MprVec3 v1 = mpr_vec3_sub(v12, v11);

    if (mpr_vec3_dot(dir, v1) <= 0.0) {
        if (out_normal) *out_normal = dir;
        return 0;
    }

    dir = mpr_vec3_cross(v1, v0);
    if (mpr_vec3_is_zero(dir)) {
        dir = mpr_vec3_normalize(mpr_vec3_sub(v1, v0));
        if (out_normal) *out_normal = dir;
		if (out_contact_a) *out_contact_a = v11;
		if (out_contact_b) *out_contact_b = v12;
        return 1;
    }
    
    // v2 - support perpendicular to v1,v0
    MprVec3 v21 = convex_support(convexA, countA, transformA, mpr_vec3_negate(dir));
	MprVec3 v22 = convex_support(convexB, countB, transformB, dir);
    MprVec3 v2 = mpr_vec3_sub(v22, v21);
    if (mpr_vec3_dot(v2, dir) <= 0.0) {
        if (out_normal) *out_normal = dir;
        return 0;
    }
    
    dir = mpr_vec3_cross(mpr_vec3_sub(v1, v0), mpr_vec3_sub(v2, v0));
    float dist = mpr_vec3_dot(dir, v0);

    if (dist > 0.0) {
        mpr_vec3_swap(&v1,  &v2);
		mpr_vec3_swap(&v11, &v21);
		mpr_vec3_swap(&v12, &v22);
		dir = mpr_vec3_negate(dir);
    }

    //Phase 1: identify a portal
    while(1) {
        // v3 - support point perpendicular to current plane
        MprVec3 v31 = convex_support(convexA, countA, transformA, mpr_vec3_negate(dir));
        MprVec3 v32 = convex_support(convexB, countB, transformB, dir);
        MprVec3 v3 = mpr_vec3_sub(v32, v31);

        if (mpr_vec3_dot(v3, dir) <= 0.0) {
            if (out_normal) *out_normal = dir;
            return 0;
        }

        // If origin is outside (v1,v0,v3), then eliminate v2 and loop
        if (mpr_vec3_dot(mpr_vec3_cross(v1,v3), v0) < 0.0) {
            v2 = v3;
            v21 = v31;
            v22 = v32;
            dir = mpr_vec3_cross(mpr_vec3_sub(v1,v0), mpr_vec3_sub(v3,v0));
            continue;
        }

        // If origin is outside (v3,v0,v2), then eliminate v1 and loop
        if (mpr_vec3_dot(mpr_vec3_cross(v3,v2), v0) < 0.0) {
            v1 = v3;
            v11 = v31;
            v12 = v32;
            dir = mpr_vec3_cross(mpr_vec3_sub(v3,v0), mpr_vec3_sub(v2,v0));
            continue;
        }
        
        //Phase 2: refine the portal
        int hit = 0;
        int phase2 = 0;

        // We are now inside of a wedge
        while (1) {
            phase2++;

            // compute normal of wedge face
            dir = mpr_vec3_cross(mpr_vec3_sub(v2,v1), mpr_vec3_sub(v3,v1));
            if (mpr_vec3_is_zero(dir)) {
                return 1;
            }

            dir = mpr_vec3_normalize(dir);

            // Compute distance from origin to wedge face
            float d = mpr_vec3_dot(dir, v1);

            // If the origin is inside the wedge, we have a hit
            if (d >= 0.0 && !hit) {
                if (out_normal) *out_normal = dir;

                float b0 = mpr_vec3_dot(mpr_vec3_cross(v1,v2),v3);
                float b1 = mpr_vec3_dot(mpr_vec3_cross(v3,v2),v0);
                float b2 = mpr_vec3_dot(mpr_vec3_cross(v0,v1),v3);
                float b3 = mpr_vec3_dot(mpr_vec3_cross(v2,v1),v0);
                float sum = b0 + b1 + b2 + b3;

                if (sum <= 0.0) {
                    b0 = 0;
                    b1 = mpr_vec3_dot(mpr_vec3_cross(v2,v3), dir);
                    b2 = mpr_vec3_dot(mpr_vec3_cross(v3,v1), dir);
                    b3 = mpr_vec3_dot(mpr_vec3_cross(v1,v2), dir);
                    sum = b1 + b2 + b3;
                }

                float inv = 1.0f / sum;

                if (out_contact_a) {
                    *out_contact_a = mpr_vec3_scale(mpr_vec3_add(mpr_vec3_add(mpr_vec3_scale(v01, b0), mpr_vec3_scale(v11, b1)), 
                                                  mpr_vec3_add(mpr_vec3_scale(v21, b2), mpr_vec3_scale(v31, b3))), inv);
				}

				if (out_contact_b) {
                    *out_contact_b = mpr_vec3_scale(mpr_vec3_add(mpr_vec3_add(mpr_vec3_scale(v02, b0), mpr_vec3_scale(v12, b1)), 
                                                  mpr_vec3_add(mpr_vec3_scale(v22, b2), mpr_vec3_scale(v32, b3))), inv);
				}

                hit = 1;
            }

            // v4 - support point in the direction of the wedge face
            MprVec3 v41 = convex_support(convexA, countA, transformA, mpr_vec3_negate(dir));
            MprVec3 v42 = convex_support(convexB, countB, transformB, dir);
            MprVec3 v4 = mpr_vec3_sub(v42, v41);

            float delta = mpr_vec3_dot(mpr_vec3_sub(v4,v3), dir);
            float separation = -mpr_vec3_dot(v4,dir);

            // If the boundary is thin enough or the origin is outside the support plane for the newly discovered vertex, then we can terminate
            if (delta <= K_COLLIDE_EPSILON || separation >= 0.0) {
                if (out_normal) *out_normal = dir;
				return hit;
            }
            
            //Compute Tetrahedron divding faces
            float d1 = mpr_vec3_dot(mpr_vec3_cross(v4,v1), v0);
            float d2 = mpr_vec3_dot(mpr_vec3_cross(v4,v2), v0);
            float d3 = mpr_vec3_dot(mpr_vec3_cross(v4,v3), v0);

            if (d1 < 0.0) {
                if (d2 < 0.0) {
                    // Inside d1 & inside d2 ==> eliminate v1
                    v1  = v4;
                    v11 = v41;
                    v12 = v42;
                }
                else {
                    // Inside d1 & outside d2 ==> eliminate v3
                    v3  = v4;
                    v31 = v41;
                    v32 = v42;
                }
            } else {
                if (d3 < 0.0) {
                    // Outside d1 & inside d3 ==> eliminate v2
                    v2  = v4;
                    v21 = v41;
                    v22 = v42;
                } else {
                    // Outside d1 & outside d3 ==> eliminate v1
                    v1  = v4;
                    v11 = v41;
                    v12 = v42;
                }
            }
        }
    }
}
