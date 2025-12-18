#pragma once

typedef struct AtomicInt
{
	volatile int atomic_int;
} AtomicInt;

i32 atomic_int_set(AtomicInt* in_atomic, i32 in_new_value)
{
	return __sync_lock_test_and_set(&in_atomic->atomic_int, in_new_value);
}

i32 atomic_int_add(AtomicInt* in_atomic, i32 in_value_to_add)
{
	return __sync_fetch_and_add(&in_atomic->atomic_int, in_value_to_add);
}

int atomic_int_get(AtomicInt* in_atomic)
{
	return atomic_int_add(in_atomic, 0);	
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

