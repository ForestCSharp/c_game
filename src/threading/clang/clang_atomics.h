#pragma once

typedef struct AtomicInt32
{
	volatile i32 atomic_int;
} AtomicInt32;

i32 atomic_i32_set(AtomicInt32* in_atomic, i32 in_new_value)
{
    return __atomic_exchange_n(&in_atomic->atomic_int, in_new_value, __ATOMIC_SEQ_CST);
}

i32 atomic_i32_add(AtomicInt32* in_atomic, i32 in_value_to_add)
{
    return __atomic_fetch_add(&in_atomic->atomic_int, in_value_to_add, __ATOMIC_SEQ_CST);
}

int atomic_i32_get(AtomicInt32* in_atomic)
{
    return __atomic_load_n(&in_atomic->atomic_int, __ATOMIC_SEQ_CST);
}

typedef struct AtomicInt64
{
    volatile i64 atomic_int;
} AtomicInt64;

i64 atomic_i64_set(AtomicInt64* in_atomic, i64 in_new_value)
{
    // Sets the value and returns the previous value
    return __atomic_exchange_n(&in_atomic->atomic_int, in_new_value, __ATOMIC_SEQ_CST);
}

i64 atomic_i64_add(AtomicInt64* in_atomic, i64 in_value_to_add)
{
    // Adds value and returns the value BEFORE the addition
    return __atomic_fetch_add(&in_atomic->atomic_int, in_value_to_add, __ATOMIC_SEQ_CST);
}

i64 atomic_i64_get(AtomicInt64* in_atomic)
{
    // Atomic read via a fetch-and-add of zero
    return __atomic_load_n(&in_atomic->atomic_int, __ATOMIC_SEQ_CST);
}

typedef struct AtomicBool
{
	volatile int atomic_int;
} AtomicBool;

void atomic_bool_set(AtomicBool* in_atomic, bool in_new_value)
{
    __atomic_store_n(&in_atomic->atomic_int, in_new_value ? 1 : 0, __ATOMIC_SEQ_CST);
}

bool atomic_bool_get(AtomicBool* in_atomic)
{
    return __atomic_load_n(&in_atomic->atomic_int, __ATOMIC_SEQ_CST) != 0;
}

