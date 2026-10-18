// Alignment beyond 16: globals with aligned(N) after the declarator,
// before it and _Alignas; locals so aligned (the frame keeps room to
// align them in, their address computed on entry), recursively;
// __int128 aligned to 16 (a member of a struct, a struct aligned to it).
#include <stdio.h>
#include <string.h>
char pad1;
long aligned __attribute__((aligned(32))) = 7;
char pad2;
__attribute__((aligned(64))) long a2 = 7;
_Alignas(128) long a3;
struct V { _Alignas(32) double d[4]; };
typedef __uint128_t pair;
struct __attribute__((aligned(sizeof(pair)))) Table { void *previous; unsigned size, mask, count; pair array[1]; };
struct Holder { void *p; __int128 x; };
static int check(void *p, int a) { return (int)((unsigned long)p % a); }
int f(int depth) {
    char c = 1;
    _Alignas(64) long l = 2;
    long m __attribute__((aligned(128))) = 3;
    static long s __attribute__((aligned(256)));
    struct V v;
    char buf[3];
    memset(&v, 0, sizeof v);
    v.d[2] = 2.5;
    l += m;
    buf[0] = c;
    int r = check(&l, 64) + check(&m, 128) + check(&v, 32) + check(&s, 256) + (int)l - 5 + (int)(v.d[2] * 2) - 5 + buf[0] - 1;
    if (depth > 0) r += f(depth - 1);
    return r;
}
int main(void) {
    char pad[5];
    pad[0] = 0;
    printf("%d %d %d %d %d\n", f(3), pad[0], check(&aligned, 32), check(&a2, 64), check(&a3, 128));
    printf("%d %d %d %d %d %d\n", (int)_Alignof(pair), (int)sizeof(struct Table), (int)__builtin_offsetof(struct Table, array), (int)__builtin_offsetof(struct Holder, x), (int)_Alignof(struct Holder), (int)_Alignof(unsigned __int128));
    return 0;
}
