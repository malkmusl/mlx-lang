/* FreeType's arithmetic: FT_MulFix, FT_MulDiv, FT_DivFix, the rounding
   functions, the trigonometry of fttrigon.h and the matrices, on values
   of a fixed pseudo-random sequence, as FreeType 2.13 computes them. */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRIGONOMETRY_H
#include FT_GLYPH_H
#include <stdio.h>

static unsigned long long state = 88172645463325252ULL;

static long next(void)
{
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return (long)state;
}

/* A value of `bits` bits, either sign. */
static long value(int bits)
{
    long v = next();
    long magnitude = (long)((unsigned long)v >> (64 - bits));
    return (v & 1) ? -magnitude : magnitude;
}

int main(void)
{
    unsigned long sum = 0;
    int i;
    for (i = 0; i < 20000; i++) {
        long a = value(1 + i % 32), b = value(1 + (i / 7) % 32), c = value(1 + (i / 3) % 31);
        sum = sum * 31 + (unsigned long)FT_MulFix(a, b);
        sum = sum * 31 + (unsigned long)FT_MulDiv(a, b, c);
        sum = sum * 31 + (unsigned long)FT_DivFix(a, c);
        sum = sum * 31 + (unsigned long)FT_RoundFix(a) + (unsigned long)FT_CeilFix(b) + (unsigned long)FT_FloorFix(c);
        if (i < 8)
            printf("%ld %ld %ld: %ld %ld %ld\n", a, b, c, FT_MulFix(a, b), FT_MulDiv(a, b, c), FT_DivFix(a, c));
    }
    printf("arithmetic %lu\n", sum);

    sum = 0;
    for (i = 0; i < 20000; i++) {
        FT_Angle angle = value(26);
        FT_Vector v;
        FT_Fixed length;
        FT_Angle back;
        sum = sum * 31 + (unsigned long)FT_Sin(angle) + 7 * (unsigned long)FT_Cos(angle);
        if (i % 4)
            sum = sum * 31 + (unsigned long)FT_Tan(value(22));
        v.x = value(1 + i % 30);
        v.y = value(1 + (i / 5) % 30);
        sum = sum * 31 + (unsigned long)FT_Atan2(v.x, v.y);
        sum = sum * 31 + (unsigned long)FT_Vector_Length(&v);
        length = 0;
        back = 0;
        FT_Vector_Polarize(&v, &length, &back);
        sum = sum * 31 + (unsigned long)length + 3 * (unsigned long)back;
        FT_Vector_Rotate(&v, angle);
        sum = sum * 31 + (unsigned long)v.x + 5 * (unsigned long)v.y;
        FT_Vector_Unit(&v, angle);
        sum = sum * 31 + (unsigned long)v.x + 5 * (unsigned long)v.y;
        FT_Vector_From_Polar(&v, value(20), angle);
        sum = sum * 31 + (unsigned long)v.x + 5 * (unsigned long)v.y;
        sum = sum * 31 + (unsigned long)FT_Angle_Diff(value(26), value(26));
        if (i < 6) {
            FT_Vector_Unit(&v, angle);
            printf("angle %ld: sin %ld cos %ld unit %ld %ld\n", angle, FT_Sin(angle), FT_Cos(angle), v.x, v.y);
        }
    }
    printf("trigonometry %lu\n", sum);

    sum = 0;
    for (i = 0; i < 5000; i++) {
        FT_Matrix a, b;
        FT_Vector v;
        a.xx = value(18); a.xy = value(17); a.yx = value(17); a.yy = value(18);
        b.xx = value(18); b.xy = value(17); b.yx = value(17); b.yy = value(18);
        v.x = value(20); v.y = value(20);
        FT_Vector_Transform(&v, &a);
        FT_Matrix_Multiply(&a, &b);
        sum = sum * 31 + (unsigned long)v.x + 3 * (unsigned long)v.y + (unsigned long)b.xx + 5 * (unsigned long)b.xy + 7 * (unsigned long)b.yx + 11 * (unsigned long)b.yy;
        if (FT_Matrix_Invert(&b) == 0)
            sum = sum * 31 + (unsigned long)b.xx + 5 * (unsigned long)b.xy + 7 * (unsigned long)b.yx + 11 * (unsigned long)b.yy;
        else
            sum = sum * 31 + 1;
    }
    printf("matrices %lu\n", sum);
    return 0;
}
