/* <stdatomic.h>: C11's atomics over mlx-cc's __atomic builtins (locked
   x86 instructions). The _Atomic qualifier is not there: the atomic
   types are the plain ones, and every access to one an atomic is meant
   for goes through these functions (as libpas and other portable code
   does). The orders are taken as sequentially consistent. */
#ifndef _MLXCC_STDATOMIC_H
#define _MLXCC_STDATOMIC_H
#include <stddef.h>
#include <stdint.h>

typedef enum {
    memory_order_relaxed = 0,
    memory_order_consume = 1,
    memory_order_acquire = 2,
    memory_order_release = 3,
    memory_order_acq_rel = 4,
    memory_order_seq_cst = 5
} memory_order;

#define __ATOMIC_RELAXED 0
#define __ATOMIC_CONSUME 1
#define __ATOMIC_ACQUIRE 2
#define __ATOMIC_RELEASE 3
#define __ATOMIC_ACQ_REL 4
#define __ATOMIC_SEQ_CST 5

typedef _Bool atomic_bool;
typedef char atomic_char;
typedef signed char atomic_schar;
typedef unsigned char atomic_uchar;
typedef short atomic_short;
typedef unsigned short atomic_ushort;
typedef int atomic_int;
typedef unsigned int atomic_uint;
typedef long atomic_long;
typedef unsigned long atomic_ulong;
typedef long long atomic_llong;
typedef unsigned long long atomic_ullong;
typedef intptr_t atomic_intptr_t;
typedef uintptr_t atomic_uintptr_t;
typedef size_t atomic_size_t;
typedef ptrdiff_t atomic_ptrdiff_t;
typedef intmax_t atomic_intmax_t;
typedef uintmax_t atomic_uintmax_t;

typedef struct { unsigned char __value; } atomic_flag;
#define ATOMIC_FLAG_INIT { 0 }
#define ATOMIC_VAR_INIT(value) (value)
#define atomic_init(object, value) ((void)(*(object) = (value)))
#define kill_dependency(y) (y)

#define ATOMIC_BOOL_LOCK_FREE 2
#define ATOMIC_CHAR_LOCK_FREE 2
#define ATOMIC_SHORT_LOCK_FREE 2
#define ATOMIC_INT_LOCK_FREE 2
#define ATOMIC_LONG_LOCK_FREE 2
#define ATOMIC_LLONG_LOCK_FREE 2
#define ATOMIC_POINTER_LOCK_FREE 2

#define atomic_thread_fence(order) __atomic_thread_fence(order)
#define atomic_signal_fence(order) __atomic_signal_fence(order)
#define atomic_is_lock_free(object) ((void)(object), 1)

#define atomic_store_explicit(object, value, order) __atomic_store_n(object, value, order)
#define atomic_store(object, value) __atomic_store_n(object, value, 5)
#define atomic_load_explicit(object, order) __atomic_load_n(object, order)
#define atomic_load(object) __atomic_load_n(object, 5)
#define atomic_exchange_explicit(object, value, order) __atomic_exchange_n(object, value, order)
#define atomic_exchange(object, value) __atomic_exchange_n(object, value, 5)
#define atomic_compare_exchange_strong_explicit(object, expected, desired, success, failure) __atomic_compare_exchange_n(object, expected, desired, 0, success, failure)
#define atomic_compare_exchange_strong(object, expected, desired) __atomic_compare_exchange_n(object, expected, desired, 0, 5, 5)
#define atomic_compare_exchange_weak_explicit(object, expected, desired, success, failure) __atomic_compare_exchange_n(object, expected, desired, 1, success, failure)
#define atomic_compare_exchange_weak(object, expected, desired) __atomic_compare_exchange_n(object, expected, desired, 1, 5, 5)
#define atomic_fetch_add_explicit(object, operand, order) __atomic_fetch_add(object, operand, order)
#define atomic_fetch_add(object, operand) __atomic_fetch_add(object, operand, 5)
#define atomic_fetch_sub_explicit(object, operand, order) __atomic_fetch_sub(object, operand, order)
#define atomic_fetch_sub(object, operand) __atomic_fetch_sub(object, operand, 5)
#define atomic_fetch_or_explicit(object, operand, order) __atomic_fetch_or(object, operand, order)
#define atomic_fetch_or(object, operand) __atomic_fetch_or(object, operand, 5)
#define atomic_fetch_xor_explicit(object, operand, order) __atomic_fetch_xor(object, operand, order)
#define atomic_fetch_xor(object, operand) __atomic_fetch_xor(object, operand, 5)
#define atomic_fetch_and_explicit(object, operand, order) __atomic_fetch_and(object, operand, order)
#define atomic_fetch_and(object, operand) __atomic_fetch_and(object, operand, 5)
#define atomic_flag_test_and_set_explicit(flag, order) __atomic_test_and_set(&(flag)->__value, order)
#define atomic_flag_test_and_set(flag) __atomic_test_and_set(&(flag)->__value, 5)
#define atomic_flag_clear_explicit(flag, order) __atomic_clear(&(flag)->__value, order)
#define atomic_flag_clear(flag) __atomic_clear(&(flag)->__value, 5)
#endif
