/* _BitInt (C23 6.2.5) as mlx-cc reads it with -std=c23, widths up to
   64: the sizes, wrapping of the unsigned ones at their width, no
   promotion to int, the usual conversions between widths, the wb and
   uwb constants, the conversions to and from the other integers.
   clang -std=c23 printed the expected output (gcc 13 has no _BitInt). */
#include <stdio.h>
#include <limits.h>

static _BitInt(12) twelve(_BitInt(12) a, _BitInt(12) b) { return a * b; }

int main(void)
{
    _BitInt(7) small = 60;
    unsigned _BitInt(7) usmall = 100;
    _BitInt(12) mid = 2000;
    unsigned _BitInt(12) umid = 4000;
    _BitInt(40) wide = 549755813887wb;
    unsigned _BitInt(64) widest = 18446744073709551615uwb;
    signed _BitInt(3) tiny = -4;

    printf("%zu %zu %zu %zu %zu %zu\n", sizeof(_BitInt(7)), sizeof(_BitInt(12)), sizeof(_BitInt(17)), sizeof(_BitInt(33)), sizeof(_BitInt(64)), sizeof small);
    printf("%d %u %d %u\n", (int)small, (unsigned)usmall, (int)mid, (unsigned)umid);
    usmall = usmall + 50;
    umid = umid * 3;
    printf("%u %u\n", (unsigned)usmall, (unsigned)umid);
    printf("%d %d\n", (int)twelve(100, 20), (int)twelve(-50, 3));
    printf("%lld %llu %d\n", (long long)wide, (unsigned long long)widest, (int)tiny);
    printf("%d %d\n", (int)(tiny - 1), (int)(unsigned _BitInt(3))(tiny));
    printf("%zu %zu %zu\n", sizeof(1wb), sizeof(255uwb), sizeof(70000wb));
    printf("%d %d\n", (int)(mid / 7), (int)(mid % 7));
    printf("%d %d %d\n", small < mid, umid > usmall, (int)(small << 2));
    printf("%lld %d\n", (long long)(wide + 1), (int)(_BitInt(12))70000);
    printf("%d\n", BITINT_MAXWIDTH >= 64);
    return 0;
}
