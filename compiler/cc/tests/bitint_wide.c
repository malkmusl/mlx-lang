/* _BitInt wider than 64 bits (C23 6.2.5) as mlx-cc keeps them: limbs in
   memory, the arithmetic in the compiled-in runtime of __mlxcc_wide.c.
   The sizes, wb constants beyond 64 bits, the operators, the
   conversions, wide parameters and results (in registers up to 128
   bits, in memory beyond), globals with constant and computed
   initializers. clang -std=c23 printed the expected output. */
#include <stdio.h>

typedef unsigned _BitInt(128) u128;
typedef _BitInt(128) i128;
typedef unsigned _BitInt(256) u256;
typedef _BitInt(200) i200;

static void print128(const char *label, u128 v)
{
    printf("%s %016llx%016llx\n", label, (unsigned long long)(v >> 64), (unsigned long long)v);
}

static void print256(const char *label, u256 v)
{
    printf("%s %016llx%016llx%016llx%016llx\n", label, (unsigned long long)(v >> 192), (unsigned long long)(v >> 128), (unsigned long long)(v >> 64), (unsigned long long)v);
}

static u128 mul128(u128 a, u128 b) { return a * b; }
static i128 neg128(i128 a) { return -a; }
static u256 shift256(u256 a, int by) { return a << by; }
static i200 sum200(i200 a, i200 b, i200 c) { return a + b + c; }

u128 g1 = 340282366920938463463374607431768211455uwb;
i128 g2 = -170141183460469231731687303715884105728wb;
i128 g3 = -5;
u128 g4 = (u128)1 << 100;
unsigned _BitInt(72) g5 = 0xffffffffffffffffffuwb;
u256 g6 = 0x123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0uwb;

int main(void)
{
    printf("%zu %zu %zu %zu %zu\n", sizeof(u128), sizeof(_BitInt(65)), sizeof(_BitInt(129)), sizeof(u256), _Alignof(u128));
    print128("g1", g1);
    print128("g2", (u128)g2);
    print128("g3", (u128)g3);
    print128("g4", g4);
    print128("g5", (u128)g5);
    print256("g6", g6);

    u128 a = 0xfedcba9876543210fedcba9876543210uwb;
    u128 b = 0x0123456789abcdefuwb;
    print128("a+b", a + b);
    print128("a-b", a - b);
    print128("a*b", a * b);
    print128("a/b", a / b);
    print128("a%b", a % b);
    print128("a&b", a & b);
    print128("a|b", a | b);
    print128("a^b", a ^ b);
    print128("~a", ~a);
    print128("a<<33", a << 33);
    print128("a>>33", a >> 33);
    print128("mul", mul128(a, b));
    print128("b*b", mul128(b, b));
    print128("ovf", a + a);

    i128 s = -123456789012345678901234567890wb;
    i128 t = 1000000007;
    print128("s", (u128)s);
    print128("s/t", (u128)(s / t));
    print128("s%t", (u128)(s % t));
    print128("-s", (u128)neg128(s));
    print128("s>>7", (u128)(s >> 7));
    print128("s*t", (u128)(s * t));
    printf("%d %d %d %d %d %d\n", s < t, s > t, s == s, s != t, (int)(s <= -s), (u128)s > (u128)t);
    printf("%d %d %d\n", !s, !!t, (i128)0 ? 1 : 0);
    printf("%d %d\n", s && t, (i128)0 || (u128)0);

    long long low = (long long)s;
    unsigned long long ulow = (unsigned long long)a;
    int narrow = (int)(a >> 100);
    printf("%lld %llu %d\n", low, ulow, narrow);
    i128 fromNeg = -7;
    u128 fromUnsigned = 18446744073709551615ull;
    i128 fromChar = (signed char)-1;
    print128("fromNeg", (u128)fromNeg);
    print128("fromUnsigned", fromUnsigned);
    print128("fromChar", (u128)fromChar);

    double d = (double)a;
    double e = (double)s;
    u128 fromDouble = (u128)1e30;
    i128 fromNegDouble = (i128)-2.5e20;
    printf("%.17g %.17g %.1f\n", d, e, (double)(i128)-3);
    print128("fromDouble", fromDouble);
    print128("fromNegDouble", (u128)fromNegDouble);
    float f = (float)((u128)1 << 70);
    printf("%.9g\n", f);

    u256 wide = (u256)a << 128 | b;
    print256("wide", wide);
    print256("wide*wide", wide * wide);
    print256("wide/a", wide / a);
    print256("wide<<77", shift256(wide, 77));
    print256("wide>>77", wide >> 77);
    print256("-wide", -wide);
    i200 p = sum200(123456789123456789wb, -5, (i200)1 << 150);
    print256("p", (u256)(u128)(p >> 128) << 128 | (u256)(u128)p);

    u128 c = 1;
    c <<= 100;
    c += 5;
    c *= 3;
    c -= 1;
    c |= 0xf0;
    c ^= 0xff;
    c &= ~(u128)7;
    c >>= 2;
    c /= 7;
    c %= (u128)1 << 90;
    print128("c", c);
    c++;
    ++c;
    print128("c++", c);
    u128 old = c--;
    print128("old", old);
    print128("c--", c);

    switch ((i128)3) {
    case 3: printf("three\n"); break;
    default: printf("other\n"); break;
    }
    int widths = 0;
    for (u128 i = 0; i < 10; i++) widths += (int)i;
    printf("%d\n", widths);
    u128 table[3] = { 1, (u128)2 << 64, 3 };
    print128("t1", table[1]);
    print128("t0+t2", table[0] + table[2]);
    struct { int tag; u128 value; } holder = { 7, (u128)0xabcdefuwb << 64 };
    print128("holder", holder.value);
    printf("%d %zu\n", holder.tag, sizeof holder);
    i128 cond = s < 0 ? s : t;
    print128("cond", (u128)cond);
    printf("%d\n", (int)(unsigned _BitInt(8))(a + 1));
    _BitInt(65) narrowSigned = (_BitInt(65))a;
    print128("narrow65", (u128)narrowSigned);
    print128("pwr", (u128)1 << 127);
    print128("3^80", (u128)((u128)3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3));
    return 0;
}
