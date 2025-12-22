#pragma once

typedef struct AtomicInt32
{
	volatile i32 atomic_int;
} AtomicInt32;

i32 atomic_i32_set(AtomicInt32* in_atomic, i32 in_new_value)
{
	return __sync_lock_test_and_set(&in_atomic->atomic_int, in_new_value);
}

i32 atomic_i32_add(AtomicInt32* in_atomic, i32 in_value_to_add)
{
	return __sync_fetch_and_add(&in_atomic->atomic_int, in_value_to_add);
}

int atomic_i32_get(AtomicInt32* in_atomic)
{
	return atomic_i32_add(in_atomic, 0);	
}

typedef struct AtomicInt64
{
    volatile i64 atomic_int;
} AtomicInt64;

i64 atomic_i64_set(AtomicInt64* in_atomic, i64 in_new_value)
{
    // Sets the value and returns the previous value
    return __sync_lock_test_and_set(&in_atomic->atomic_int, in_new_value);
}

i64 atomic_i64_add(AtomicInt64* in_atomic, i64 in_value_to_add)
{
    // Adds value and returns the value BEFORE the addition
    return __sync_fetch_and_add(&in_atomic->atomic_int, in_value_to_add);
}

i64 atomic_i64_get(AtomicInt64* in_atomic)
{
    // Atomic read via a fetch-and-add of zero
    return atomic_i64_add(in_atomic, 0);    
}

typedef struct AtomicBool
{
	volatile int atomic_int;
} AtomicBool;

void atomic_bool_set(AtomicBool* in_atomic, bool in_new_value)
{
	if (in_new_value)
	{
		__sync_lock_test_and_set(&in_atomic->atomic_int, 1);
	}
	else
	{
		__sync_and_and_fetch(&in_atomic->atomic_int, 0);
	}
}

bool atomic_bool_get(AtomicBool* in_atomic)
{
	return __sync_add_and_fetch(&in_atomic->atomic_int, 0) != 0;
}

