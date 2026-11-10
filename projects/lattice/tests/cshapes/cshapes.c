/* The translator's test of: see tools/check_lattice_emit.sh (cshapes). */
typedef unsigned long size_t;
typedef struct
{
    unsigned long low64, high64;
} H;
static unsigned first(const void *input, size_t len)
{
    const unsigned char *p = (const unsigned char *)input;
    return len ? p[0] : 0;
}
static int equal(H a, H b) { return (a.low64 == b.low64) & (a.high64 == b.high64); }
static int compare(H a, H b)
{
    int hcmp = (a.high64 > b.high64) - (b.high64 > a.high64);
    if (hcmp)
        return hcmp;
    return (a.low64 > b.low64) - (b.low64 > a.low64);
}
static unsigned flags(unsigned total, size_t len)
{
    unsigned large = 0;
    large |= (unsigned)((len >= 16) | (total >= 16));
    return large;
}
static unsigned last(const unsigned char *secret, size_t size) { return secret[size - 4 - 2] + *(secret + size - 4 - 1); }
static unsigned aligned(const unsigned char *input, int align) { return align ? input[1] : input[0]; }
unsigned cshapes_run(void)
{
    unsigned char s[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    H a = {1, 2}, b = {1, 3};
    unsigned sum = first(s, 8) + (unsigned)equal(a, a) * 10 + (unsigned)(compare(a, b) + 2) * 100;
    sum += flags(3, 20) * 1000 + last(s, 8) * 10000 + aligned(s, 1) * 100000 + (unsigned)!equal(a, b) * 1000000;
    return sum;
}
