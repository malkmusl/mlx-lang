/* Integer arithmetic, conversions and comparisons. */
#include <stdio.h>
#include <limits.h>

static int ident(int x) { return x; }
static unsigned uident(unsigned x) { return x; }

int main(void) {
    int a = 17, b = -5;
    printf("%d %d %d %d %d\n", a + b, a - b, a * b, a / b, a % b);
    printf("%d %d\n", -17 / 5, -17 % 5);
    unsigned u = 3000000000u;
    printf("%u %u %u\n", u + u, u * 3, u / 7);
    printf("%d\n", (int)u);
    long l = 1L << 40;
    printf("%ld %ld\n", l, l >> 3);
    printf("%d %d\n", (int)((unsigned)INT_MAX + (unsigned)ident(1)) == INT_MIN, uident(UINT_MAX) + 1u);
    signed char c = 200;
    unsigned char uc = 200;
    printf("%d %d %d\n", c, uc, c + uc);
    short s = -2;
    unsigned short us = 65535;
    printf("%d %d %d\n", s, us, s * us);
    printf("%d %d\n", -1 < 1u, -1L < 1u);
    printf("%d %d %d\n", 7 & 3, 7 | 8, 7 ^ 2);
    printf("%d %d %d\n", ~0, !0, !5);
    int x = -16;
    printf("%d %u\n", x >> 2, (unsigned)x >> 28);
    unsigned long big = 0xFFFFFFFFFFFFFFFFul;
    printf("%lu %lx\n", big, big / 3);
    long neg = -9223372036854775807L - 1;
    printf("%ld\n", neg);
    int i = 5;
    i += 3; printf("%d ", i);
    i -= 10; printf("%d ", i);
    i *= -7; printf("%d ", i);
    i /= 3; printf("%d ", i);
    i %= 4; printf("%d ", i);
    i <<= 4; printf("%d ", i);
    i >>= 1; printf("%d ", i);
    i |= 0x100; printf("%d ", i);
    i &= 0xF0F; printf("%d ", i);
    i ^= 0xFF; printf("%d\n", i);
    int j = 0;
    int j1 = j++;
    int j2 = ++j;
    int j3 = j--;
    printf("%d %d %d %d\n", j1, j2, j3, j);
    char ch = 'A';
    ch += 2;
    printf("%c %d\n", ch, ch);
    printf("%d\n", (char)300);
    printf("%d\n", (unsigned char)-1);
    printf("%d %d\n", (short)70000, (unsigned short)-3);
    printf("%ld %lu\n", (long)(int)-1, (unsigned long)(unsigned)-1);
    printf("%d\n", sizeof(char) + sizeof(short) * 10 + sizeof(int) * 100 + sizeof(long) * 1000);
    printf("%d %d\n", 1 ? 2 : 3, 0 ? 2 : 3);
    _Bool t = 42;
    printf("%d %d\n", t, (_Bool)0.5);
    long long ll = 123456789012345LL;
    printf("%lld\n", ll * 10);
    printf("%d\n", 'a' + '\n' + '\0' + '\x41' + '\101');
    printf("%d %d\n", 0x7fffffff, 017);
    return 0;
}
