/* Division, remainder and multiplication by a constant power of two, which
   the back end makes with shifts and masks: for every integer type and
   every divisor 2^n it holds, the results for operands at the edges (0, 1,
   2^n - 1, 2^n, 2^n + 1, the largest multiple of 2^n and its neighbour,
   all ones, the top bit) are those with the same divisor read through a
   volatile, where the processor divides. A signed quotient rounds toward
   zero and its remainder has the dividend's sign. A line per type: the
   cases, how many differed, a sum of the results. */
#include <stdio.h>

static volatile unsigned long long one = 1;

#define EDGES 12

static unsigned long long edge(int e, int n, int bits) {
    unsigned long long top = one << (bits - 1), largest = top + (top - 1), d = one << n;
    switch (e) {
    case 0: return 0;
    case 1: return 1;
    case 2: return d - 1;
    case 3: return d;
    case 4: return (d + 1) & largest;
    case 5: return largest;
    case 6: return largest - 1;
    case 7: return largest - d;
    case 8: return largest - d + 1;
    case 9: return top;
    case 10: return (top + 1) & largest;
    default: return 0x5555555555555555ull & largest;
    }
}

/* The powers of two a type of 8, 16, 32 and 64 bits holds: all but the top
   bit (the signed types'), then with it. */
#define P7(X) X(0, 1) X(1, 2) X(2, 4) X(3, 8) X(4, 16) X(5, 32) X(6, 64)
#define P8(X) P7(X) X(7, 128)
#define P15(X) P8(X) X(8, 256) X(9, 512) X(10, 1024) X(11, 2048) X(12, 4096) X(13, 8192) X(14, 16384)
#define P16(X) P15(X) X(15, 32768)
#define P31(X) P16(X) X(16, 65536) X(17, 131072) X(18, 262144) X(19, 524288) X(20, 1048576) X(21, 2097152) \
    X(22, 4194304) X(23, 8388608) X(24, 16777216) X(25, 33554432) X(26, 67108864) X(27, 134217728) \
    X(28, 268435456) X(29, 536870912) X(30, 1073741824)
#define P32(X) P31(X) X(31, 2147483648)
#define P63(X) P32(X) X(32, 4294967296) X(33, 8589934592) X(34, 17179869184) X(35, 34359738368) \
    X(36, 68719476736) X(37, 137438953472) X(38, 274877906944) X(39, 549755813888) X(40, 1099511627776) \
    X(41, 2199023255552) X(42, 4398046511104) X(43, 8796093022208) X(44, 17592186044416) \
    X(45, 35184372088832) X(46, 70368744177664) X(47, 140737488355328) X(48, 281474976710656) \
    X(49, 562949953421312) X(50, 1125899906842624) X(51, 2251799813685248) X(52, 4503599627370496) \
    X(53, 9007199254740992) X(54, 18014398509481984) X(55, 36028797018963968) X(56, 72057594037927936) \
    X(57, 144115188075855872) X(58, 288230376151711744) X(59, 576460752303423488) \
    X(60, 1152921504606846976) X(61, 2305843009213693952) X(62, 4611686018427387904)
#define P64(X) P63(X) X(63, 9223372036854775808)

/* The quotient, the remainder and the product (made unsigned, where it
   wraps) with the constant written out. */
#define WRITTEN(n, v) \
    case n: q = (T)(x / (T)v##ull); r = (T)(x % (T)v##ull); p = (T)((U)x * (U)v##ull); break;

/* A type's cases: T the type, U its unsigned twin. */
#define CHECK(name, powers, count, bits) \
    do { \
        int cases = 0, differing = 0; \
        unsigned long long sum = 0; \
        for (int n = 0; n < count; n++) { \
            volatile T divisor = (T)(one << n); \
            for (int e = 0; e < EDGES; e++) { \
                T x = (T)edge(e, n, bits), q = 0, r = 0, p = 0; \
                T d = divisor; \
                switch (n) { powers(WRITTEN) } \
                if (q != (T)(x / d) || r != (T)(x % d) || p != (T)((U)x * (U)d)) differing++; \
                if ((T)((U)q * (U)d + (U)r) != x) differing++; \
                sum += (unsigned long long)(long long)q + 3 * (unsigned long long)(long long)r + 7 * (unsigned long long)(long long)p; \
                cases++; \
            } \
        } \
        printf("%s: %d cases, %d differ, sum %llx\n", name, cases, differing, sum); \
    } while (0)

static void unsignedTypes(void) {
#define T unsigned char
#define U unsigned char
    CHECK("unsigned char", P8, 8, 8);
#undef T
#undef U
#define T unsigned short
#define U unsigned short
    CHECK("unsigned short", P16, 16, 16);
#undef T
#undef U
#define T unsigned int
#define U unsigned int
    CHECK("unsigned int", P32, 32, 32);
#undef T
#undef U
#define T unsigned long
#define U unsigned long
    CHECK("unsigned long", P64, 64, 64);
#undef T
#undef U
}

static void signedTypes(void) {
#define T signed char
#define U unsigned char
    CHECK("signed char", P7, 7, 8);
#undef T
#undef U
#define T short
#define U unsigned short
    CHECK("short", P15, 15, 16);
#undef T
#undef U
#define T int
#define U unsigned int
    CHECK("int", P31, 31, 32);
#undef T
#undef U
#define T long
#define U unsigned long
    CHECK("long", P63, 63, 64);
#undef T
#undef U
}

static volatile int seven = 7;

int main(void) {
    unsignedTypes();
    signedTypes();
    /* Signed values: toward zero, the remainder with the dividend's sign. */
    int minus = -seven;
    printf("%d %d %d %d %d %d\n", minus / 2, minus % 2, minus / 4, minus % 4, minus / 8, minus % 8);
    printf("%d %d %d %d\n", (minus - 1) / 8, (minus - 1) % 8, (minus - 2) / 8, (minus - 2) % 8);
    long lowest = -9223372036854775807L + 6 - seven;
    printf("%ld %ld %ld %ld\n", lowest / 2, lowest % 2, (lowest + 1) / 4611686018427387904L, (lowest + 1) % 4611686018427387904L);
    /* The forms: compound assignments, an index, a pointer's step. */
    unsigned int u = 1052673u * (unsigned)seven / 7u;
    u /= 4096;
    printf("%u ", u);
    u %= 16;
    printf("%u ", u);
    int i = minus * 100;
    i /= 16;
    printf("%d ", i);
    i %= 8;
    printf("%d ", i);
    i *= 32;
    printf("%d\n", i);
    struct pair { long first, second; } pairs[8];
    for (int k = 0; k < 8; k++) {
        pairs[(k + seven) % 8].first = k * 4;
        pairs[(k + seven) % 8].second = k * 16 + 1;
    }
    long total = 0;
    for (int k = 0; k < seven + 1; k++) total += pairs[k].first * 1024 + pairs[k].second / 2;
    printf("%ld %d\n", total, (int)(&pairs[seven] - &pairs[1]));
    return 0;
}
