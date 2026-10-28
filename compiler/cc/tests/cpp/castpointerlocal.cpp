// A scalar local read through a pointer to another type (ANGLE's
// convert999E5toRGBFloats: `reinterpret_cast<const RGB9E5Data *>(&input)`,
// then the bit-fields through it): the local stays in memory, whatever
// holds its address.
#include <cstdio>
struct D { unsigned R : 9; unsigned G : 9; unsigned B : 9; unsigned E : 5; };
static void convert(unsigned int input, float *red, float *exp)
{
    const D *inputData = reinterpret_cast<const D *>(&input);
    *red = inputData->R * 2.0f;
    *exp = inputData->E;
}
static int lowByte(int value)
{
    const unsigned char *bytes = reinterpret_cast<const unsigned char *>(&value);
    return bytes[0] + bytes[1];
}
int main()
{
    float r, e;
    convert((7u << 27) | 5u, &r, &e);
    printf("%g %g %d\n", r, e, lowByte(0x0304));
    return 0;
}
