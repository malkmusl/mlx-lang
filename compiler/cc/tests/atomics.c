// The atomic builtins (GCC's __atomic_* and __sync_* families): loads,
// stores, exchanges, the fetch-and-op and op-and-fetch forms on every
// size, compare-exchange with the expected value updated, test-and-set,
// fences, and the generic (pointer) forms.
#include <stdio.h>
int main(void) {
    int v = 10; long l = 100; unsigned char b = 0; short s = 5; int *p = &v; int expected = 3; int ok = 0;
    printf("%d %ld\n", __atomic_load_n(&v, 5), __atomic_load_n(&l, 0));
    __atomic_store_n(&v, 20, 5);
    int x = __atomic_exchange_n(&v, 30, 5);
    printf("%d %d\n", x, v);
    int a1 = __atomic_fetch_add(&v, 5, 5);
    int a2 = __atomic_add_fetch(&v, 7, 5);
    int a3 = __atomic_fetch_sub(&v, 2, 5);
    long a4 = __atomic_sub_fetch(&l, 1, 5);
    printf("%d %d %d %ld %d %ld\n", a1, a2, a3, a4, v, l);
    int c1 = __atomic_fetch_and(&v, 0x3c, 5);
    int c2 = __atomic_or_fetch(&v, 0x81, 5);
    int c3 = __atomic_fetch_xor(&v, 0xff, 5);
    int c4 = __atomic_nand_fetch(&v, 0x0f, 5);
    printf("%d %d %d %d %d\n", c1, c2, c3, c4, v);
    expected = v;
    ok = __atomic_compare_exchange_n(&v, &expected, 77, 0, 5, 5);
    printf("%d %d %d\n", ok, v, expected);
    expected = 1;
    ok = __atomic_compare_exchange_n(&v, &expected, 88, 0, 5, 5);
    printf("%d %d %d\n", ok, v, expected);
    short s1 = __atomic_fetch_add(&s, -7, 5);
    int t1 = __atomic_test_and_set(&b, 5);
    int t2 = __atomic_test_and_set(&b, 5);
    __atomic_clear(&b, 5);
    int *q = __atomic_exchange_n(&p, (int *)0, 5);
    printf("%d %d %d %d %d %d %d\n", (int)s1, (int)s, t1, t2, (int)b, q == &v, p == 0);
    int f1 = __sync_fetch_and_add(&v, 3);
    int f2 = __sync_sub_and_fetch(&v, 1);
    int f3 = __sync_bool_compare_and_swap(&v, 12, 5);
    int f4 = __sync_val_compare_and_swap(&v, 4, 9);
    int f5 = __sync_lock_test_and_set(&v, 42);
    __sync_lock_release(&v);
    __sync_synchronize();
    printf("%d %d %d %d %d %d\n", f1, f2, f3, f4, f5, v);
    __atomic_thread_fence(5);
    __atomic_signal_fence(5);
    long out;
    __atomic_load(&l, &out, 5);
    long src = 7;
    long old;
    __atomic_store(&l, &src, 5);
    src = 8;
    __atomic_exchange(&l, &src, &old, 5);
    long exp2 = 8;
    long des = 9;
    int ok2 = __atomic_compare_exchange(&l, &exp2, &des, 0, 5, 5);
    printf("%ld %ld %ld %d %ld %d\n", out, old, l, ok2, exp2, (int)__atomic_always_lock_free(4, 0));
    return 0;
}
