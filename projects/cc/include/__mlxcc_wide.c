/* The arithmetic of _BitInt(N) for N above 64 (C23 6.2.5), compiled into
   every program that uses such a type: mlx-cc keeps a wide value in
   memory as (N + 63) / 64 limbs of 64 bits, the lowest first, canonical
   (the bits of the top limb above N sign- or zero-extended), and calls
   these for every operation on them (gen.mlx). `bits` is N and
   `u` is 1 for an unsigned type. Nothing here needs the C library. */
typedef unsigned long __mlxcc_limb;

static int __mlxcc_wide_limbs(int bits) { return (bits + 63) / 64; }

/* The value's top limb made canonical for its width. */
void __mlxcc_wide_canon(__mlxcc_limb *d, int bits, int u)
{
    int limbs = __mlxcc_wide_limbs(bits);
    int used = bits - (limbs - 1) * 64;
    if (used == 64) return;
    __mlxcc_limb mask = ((__mlxcc_limb)1 << used) - 1;
    __mlxcc_limb top = d[limbs - 1] & mask;
    if (!u && (top >> (used - 1)) & 1) top |= ~mask;
    d[limbs - 1] = top;
}

/* The sign of a canonical value: 1 when negative (signed types only). */
static int __mlxcc_wide_negative(const __mlxcc_limb *a, int bits, int u)
{
    if (u) return 0;
    return (a[__mlxcc_wide_limbs(bits) - 1] >> 63) & 1;
}

void __mlxcc_wide_copy(__mlxcc_limb *d, const __mlxcc_limb *a, int limbs)
{
    for (int i = 0; i < limbs; i++) d[i] = a[i];
}

/* d = the 64-bit value v (sign-extended when the source is signed). */
void __mlxcc_wide_from64(__mlxcc_limb *d, __mlxcc_limb v, int sourceSigned, int bits, int u)
{
    int limbs = __mlxcc_wide_limbs(bits);
    __mlxcc_limb fill = (sourceSigned && (v >> 63)) ? ~(__mlxcc_limb)0 : 0;
    d[0] = v;
    for (int i = 1; i < limbs; i++) d[i] = fill;
    __mlxcc_wide_canon(d, bits, u);
}

/* d (toBits, tu) = a (fromBits, fu): extended by the source's sign, or
   truncated. d and a may be the same place. */
void __mlxcc_wide_convert(__mlxcc_limb *d, const __mlxcc_limb *a, int fromBits, int fu, int toBits, int tu)
{
    int from = __mlxcc_wide_limbs(fromBits), to = __mlxcc_wide_limbs(toBits);
    __mlxcc_limb fill = __mlxcc_wide_negative(a, fromBits, fu) ? ~(__mlxcc_limb)0 : 0;
    __mlxcc_limb tmp[64];
    int i;
    for (i = 0; i < from && i < to; i++) tmp[i] = a[i];
    for (; i < to; i++) tmp[i] = fill;
    for (i = 0; i < to; i++) d[i] = tmp[i];
    __mlxcc_wide_canon(d, toBits, tu);
}

void __mlxcc_wide_add(__mlxcc_limb *d, const __mlxcc_limb *a, const __mlxcc_limb *b, int bits, int u)
{
    int limbs = __mlxcc_wide_limbs(bits);
    __mlxcc_limb carry = 0;
    for (int i = 0; i < limbs; i++) {
        __mlxcc_limb s = a[i] + carry;
        __mlxcc_limb c1 = s < carry;
        __mlxcc_limb t = s + b[i];
        __mlxcc_limb c2 = t < s;
        d[i] = t;
        carry = c1 | c2;
    }
    __mlxcc_wide_canon(d, bits, u);
}

void __mlxcc_wide_sub(__mlxcc_limb *d, const __mlxcc_limb *a, const __mlxcc_limb *b, int bits, int u)
{
    int limbs = __mlxcc_wide_limbs(bits);
    __mlxcc_limb borrow = 0;
    for (int i = 0; i < limbs; i++) {
        __mlxcc_limb s = a[i] - borrow;
        __mlxcc_limb b1 = a[i] < borrow;
        __mlxcc_limb t = s - b[i];
        __mlxcc_limb b2 = s < b[i];
        d[i] = t;
        borrow = b1 | b2;
    }
    __mlxcc_wide_canon(d, bits, u);
}

void __mlxcc_wide_neg(__mlxcc_limb *d, const __mlxcc_limb *a, int bits, int u)
{
    int limbs = __mlxcc_wide_limbs(bits);
    __mlxcc_limb carry = 1;
    for (int i = 0; i < limbs; i++) {
        __mlxcc_limb t = ~a[i] + carry;
        carry = (t < carry) ? 1 : 0;
        d[i] = t;
    }
    __mlxcc_wide_canon(d, bits, u);
}

void __mlxcc_wide_not(__mlxcc_limb *d, const __mlxcc_limb *a, int bits, int u)
{
    int limbs = __mlxcc_wide_limbs(bits);
    for (int i = 0; i < limbs; i++) d[i] = ~a[i];
    __mlxcc_wide_canon(d, bits, u);
}

void __mlxcc_wide_and(__mlxcc_limb *d, const __mlxcc_limb *a, const __mlxcc_limb *b, int bits, int u)
{
    int limbs = __mlxcc_wide_limbs(bits);
    for (int i = 0; i < limbs; i++) d[i] = a[i] & b[i];
}

void __mlxcc_wide_or(__mlxcc_limb *d, const __mlxcc_limb *a, const __mlxcc_limb *b, int bits, int u)
{
    int limbs = __mlxcc_wide_limbs(bits);
    for (int i = 0; i < limbs; i++) d[i] = a[i] | b[i];
}

void __mlxcc_wide_xor(__mlxcc_limb *d, const __mlxcc_limb *a, const __mlxcc_limb *b, int bits, int u)
{
    int limbs = __mlxcc_wide_limbs(bits);
    for (int i = 0; i < limbs; i++) d[i] = a[i] ^ b[i];
}

/* a b in 128 bits, from 32-bit halves. */
static void __mlxcc_wide_mul64(__mlxcc_limb a, __mlxcc_limb b, __mlxcc_limb *high, __mlxcc_limb *low)
{
    __mlxcc_limb al = a & 0xffffffffUL, ah = a >> 32, bl = b & 0xffffffffUL, bh = b >> 32;
    __mlxcc_limb ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
    __mlxcc_limb mid = (ll >> 32) + (lh & 0xffffffffUL) + (hl & 0xffffffffUL);
    *low = (mid << 32) | (ll & 0xffffffffUL);
    *high = hh + (lh >> 32) + (hl >> 32) + (mid >> 32);
}

/* The low `bits` of a times b (what wrapping arithmetic keeps). */
void __mlxcc_wide_mul(__mlxcc_limb *d, const __mlxcc_limb *a, const __mlxcc_limb *b, int bits, int u)
{
    int limbs = __mlxcc_wide_limbs(bits);
    __mlxcc_limb r[64];
    for (int i = 0; i < limbs; i++) r[i] = 0;
    for (int i = 0; i < limbs; i++) {
        __mlxcc_limb carry = 0;
        for (int j = 0; i + j < limbs; j++) {
            __mlxcc_limb high, low;
            __mlxcc_wide_mul64(a[i], b[j], &high, &low);
            __mlxcc_limb t = r[i + j] + low;
            __mlxcc_limb c1 = t < low;
            __mlxcc_limb s = t + carry;
            __mlxcc_limb c2 = s < carry;
            r[i + j] = s;
            carry = high + c1 + c2;
        }
    }
    for (int i = 0; i < limbs; i++) d[i] = r[i];
    __mlxcc_wide_canon(d, bits, u);
}

/* Whether a is zero. */
int __mlxcc_wide_nonzero(const __mlxcc_limb *a, int bits)
{
    int limbs = __mlxcc_wide_limbs(bits);
    for (int i = 0; i < limbs; i++) if (a[i]) return 1;
    return 0;
}

/* -1, 0 or 1 as a is below, equal to or above b. */
int __mlxcc_wide_cmp(const __mlxcc_limb *a, const __mlxcc_limb *b, int bits, int u)
{
    int limbs = __mlxcc_wide_limbs(bits);
    if (!u) {
        int na = __mlxcc_wide_negative(a, bits, 0), nb = __mlxcc_wide_negative(b, bits, 0);
        if (na != nb) return na ? -1 : 1;
    }
    for (int i = limbs - 1; i >= 0; i--) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

/* Unsigned long division of `limbs` limbs, bit by bit: q = a / b, r = a % b. */
static void __mlxcc_wide_udivmod(__mlxcc_limb *q, __mlxcc_limb *r, const __mlxcc_limb *a, const __mlxcc_limb *b, int limbs)
{
    for (int i = 0; i < limbs; i++) { q[i] = 0; r[i] = 0; }
    for (int bit = limbs * 64 - 1; bit >= 0; bit--) {
        /* r = (r << 1) | a's bit */
        __mlxcc_limb carry = (a[bit / 64] >> (bit % 64)) & 1;
        for (int i = 0; i < limbs; i++) {
            __mlxcc_limb next = r[i] >> 63;
            r[i] = (r[i] << 1) | carry;
            carry = next;
        }
        /* r >= b ? */
        int ge = 1;
        for (int i = limbs - 1; i >= 0; i--) {
            if (r[i] != b[i]) { ge = r[i] > b[i]; break; }
        }
        if (ge) {
            __mlxcc_limb borrow = 0;
            for (int i = 0; i < limbs; i++) {
                __mlxcc_limb s = r[i] - borrow;
                __mlxcc_limb b1 = r[i] < borrow;
                __mlxcc_limb t = s - b[i];
                __mlxcc_limb b2 = s < b[i];
                r[i] = t;
                borrow = b1 | b2;
            }
            q[bit / 64] |= (__mlxcc_limb)1 << (bit % 64);
        }
    }
}

static void __mlxcc_wide_divmod(__mlxcc_limb *d, const __mlxcc_limb *a, const __mlxcc_limb *b, int bits, int u, int wantRemainder)
{
    int limbs = __mlxcc_wide_limbs(bits);
    __mlxcc_limb na[64], nb[64], q[64], r[64];
    int negA = __mlxcc_wide_negative(a, bits, u), negB = __mlxcc_wide_negative(b, bits, u);
    if (negA) __mlxcc_wide_neg(na, a, bits, 1); else __mlxcc_wide_copy(na, a, limbs);
    if (negB) __mlxcc_wide_neg(nb, b, bits, 1); else __mlxcc_wide_copy(nb, b, limbs);
    __mlxcc_wide_canon(na, bits, 1);
    __mlxcc_wide_canon(nb, bits, 1);
    __mlxcc_wide_udivmod(q, r, na, nb, limbs);
    /* C truncates: the quotient's sign is the signs' product, the
       remainder's the dividend's. */
    if (wantRemainder) {
        if (negA) __mlxcc_wide_neg(d, r, bits, u); else __mlxcc_wide_copy(d, r, limbs);
    } else {
        if (negA != negB) __mlxcc_wide_neg(d, q, bits, u); else __mlxcc_wide_copy(d, q, limbs);
    }
    __mlxcc_wide_canon(d, bits, u);
}

void __mlxcc_wide_div(__mlxcc_limb *d, const __mlxcc_limb *a, const __mlxcc_limb *b, int bits, int u)
{
    __mlxcc_wide_divmod(d, a, b, bits, u, 0);
}

void __mlxcc_wide_rem(__mlxcc_limb *d, const __mlxcc_limb *a, const __mlxcc_limb *b, int bits, int u)
{
    __mlxcc_wide_divmod(d, a, b, bits, u, 1);
}

void __mlxcc_wide_shl(__mlxcc_limb *d, const __mlxcc_limb *a, long count, int bits, int u)
{
    int limbs = __mlxcc_wide_limbs(bits);
    __mlxcc_limb r[64];
    if (count < 0 || count >= bits) {
        for (int i = 0; i < limbs; i++) d[i] = 0;
        return;
    }
    int words = (int)(count / 64), rest = (int)(count % 64);
    for (int i = limbs - 1; i >= 0; i--) {
        __mlxcc_limb v = 0;
        if (i - words >= 0) {
            v = a[i - words] << rest;
            if (rest && i - words - 1 >= 0) v |= a[i - words - 1] >> (64 - rest);
        }
        r[i] = v;
    }
    for (int i = 0; i < limbs; i++) d[i] = r[i];
    __mlxcc_wide_canon(d, bits, u);
}

void __mlxcc_wide_shr(__mlxcc_limb *d, const __mlxcc_limb *a, long count, int bits, int u)
{
    int limbs = __mlxcc_wide_limbs(bits);
    __mlxcc_limb r[64];
    __mlxcc_limb fill = __mlxcc_wide_negative(a, bits, u) ? ~(__mlxcc_limb)0 : 0;
    if (count < 0 || count >= bits) {
        for (int i = 0; i < limbs; i++) d[i] = fill;
        __mlxcc_wide_canon(d, bits, u);
        return;
    }
    int words = (int)(count / 64), rest = (int)(count % 64);
    for (int i = 0; i < limbs; i++) {
        __mlxcc_limb v;
        __mlxcc_limb low = (i + words < limbs) ? a[i + words] : fill;
        __mlxcc_limb high = (i + words + 1 < limbs) ? a[i + words + 1] : fill;
        v = low >> rest;
        if (rest) v |= high << (64 - rest);
        r[i] = v;
    }
    for (int i = 0; i < limbs; i++) d[i] = r[i];
    __mlxcc_wide_canon(d, bits, u);
}

/* The value as a double: its top 64 significant bits, scaled. */
double __mlxcc_wide_to_double(const __mlxcc_limb *a, int bits, int u)
{
    int limbs = __mlxcc_wide_limbs(bits);
    __mlxcc_limb m[64];
    int negative = __mlxcc_wide_negative(a, bits, u);
    if (negative) __mlxcc_wide_neg(m, a, bits, 1); else __mlxcc_wide_copy(m, a, limbs);
    __mlxcc_wide_canon(m, bits, 1);
    int top = -1;
    for (int i = limbs - 1; i >= 0 && top < 0; i--) {
        if (m[i]) {
            int b = 63;
            while (!((m[i] >> b) & 1)) b--;
            top = i * 64 + b;
        }
    }
    if (top < 0) return 0.0;
    double value;
    if (top < 64) {
        value = (double)m[0];
    } else {
        /* The 64 bits from the top, and whether anything was below them. */
        int shift = top - 63;
        __mlxcc_limb high = (m[shift / 64] >> (shift % 64));
        if (shift % 64 && shift / 64 + 1 < limbs) high |= m[shift / 64 + 1] << (64 - shift % 64);
        int sticky = 0;
        for (int i = 0; i < shift && !sticky; i++) sticky = (m[i / 64] >> (i % 64)) & 1;
        if (sticky) high |= 1;
        value = (double)high;
        for (int i = 0; i < shift; i++) value *= 2.0;
    }
    return negative ? -value : value;
}

/* d = the double v truncated toward zero. */
void __mlxcc_wide_from_double(__mlxcc_limb *d, double v, int bits, int u)
{
    int limbs = __mlxcc_wide_limbs(bits);
    union { double f; __mlxcc_limb b; } cast;
    cast.f = v;
    int negative = (cast.b >> 63) & 1;
    int exponent = (int)((cast.b >> 52) & 0x7ff);
    __mlxcc_limb fraction = cast.b & 0xfffffffffffffUL;
    for (int i = 0; i < limbs; i++) d[i] = 0;
    if (exponent == 0x7ff) return;
    if (exponent == 0) return;
    __mlxcc_limb mantissa = fraction | ((__mlxcc_limb)1 << 52);
    int shift = exponent - 1023 - 52;
    if (shift < -52) return;
    if (shift <= 0) {
        d[0] = mantissa >> (-shift);
    } else {
        __mlxcc_limb one[64];
        for (int i = 0; i < limbs; i++) one[i] = 0;
        one[0] = mantissa;
        __mlxcc_wide_shl(d, one, shift, bits, 1);
    }
    if (negative) __mlxcc_wide_neg(d, d, bits, u);
    __mlxcc_wide_canon(d, bits, u);
}
