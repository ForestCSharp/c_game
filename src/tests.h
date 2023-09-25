#pragma once

#include "types.h"

bool test_stretchy_buffer()
{

    typedef struct TestStruct
    {
        int a;
        int b;
        float f;
        double d;
    } TestStruct;

    TestStruct *struct_array = NULL;
    TestStruct t = {
        .a = 1,
        .b = 2,
        .f = 3.0,
        .d = 2.0,
    };

    sb_push(struct_array, t);
    t.a = 3;
    sb_push(struct_array, t);
    t.a = 4;
    sb_push(struct_array, t);
    sb_del(struct_array, 1);

    printf("sb_len: %i \n", sb_count(struct_array));
    for (int i = 0; i < sb_count(struct_array); ++i)
    {
        printf("elem: %i .a: %i\n", i, struct_array[i].a);
    }

    // TODO: Test sb_del and sb_ins
    u32 *int_array = NULL;
    sb_push(int_array, 1);
    sb_ins(int_array, 0, 2);
    sb_ins(int_array, 1, 3);
    sb_del(int_array, 1);
    sb_ins(int_array, 1, 4);
    printf("int_array len: %i \n", sb_count(int_array));
    printf("[");
    for (int i = 0; i < sb_count(int_array); ++i)
    {
        printf("%i%s", int_array[i], i != sb_count(int_array) - 1 ? "," : "");
    }
    printf("]\n");

    return true;
}

bool test_math()
{
    Vec4 a = vec4_new(2.0, 1.0, 1.0, 0.0);
    Vec4 b = vec4_new(-2.0, -1.0, 1.0, 0.0);
    Vec4 add = vec4_add(&a, &b);
    Vec4 sub = vec4_sub(&a, &b);

    printf("add: ");
    vec4_print(&add);
    printf("sub: ");
    vec4_print(&sub);
    printf("dot: %f \n", vec4_dot(&a, &a));

    Vec3 a3 = vec3_new(1, 0, 0);
    Vec3 b3 = vec3_new(0, 1, 0);
    Vec3 a3_cross_b3 = vec3_cross(&a3, &b3);
    Vec3 b3_cross_a3 = vec3_cross(&b3, &a3);

    printf("a3_cross_b3: "), vec3_print(&a3_cross_b3);
    printf("b3_cross_a3: "), vec3_print(&b3_cross_a3);

    Mat4 ma = {
        .d =
            {
                1,
                0,
                0,
                0,
                1,
                1,
                0,
                0,
                1,
                0,
                1,
                0,
                0,
                0,
                0,
                1,
            },
    };

    Mat4 mb = {
        .d =
            {
                2,
                0,
                0,
                0,
                0,
                2,
                0,
                0,
                0,
                0,
                2,
                0,
                0,
                0,
                0,
                2,
            },
    };

    Mat4 ma_times_mb = mat4_mult_mat4(&ma, &mb);

    printf("ma:\n"), mat4_print(&ma);
    printf("mb:\n"), mat4_print(&mb);
    printf("ma times mb:\n"), mat4_print(&ma_times_mb);

    Vec4 v = vec4_new(1, 1, 1, 1);
    Vec4 mb_times_v = mat4_mult_vec4(&mb, &v);
    printf("mb_times_v: "), vec4_print(&mb_times_v);

    return true;
}

void run_tests()
{
    bool result = true;

    result &= test_stretchy_buffer();
    result &= test_math();

    if (result)
    {
        printf("TESTS PASSED\n");
    }
    else
    {
        printf("TESTS FAILED\n");
    }
}
