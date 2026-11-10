// The translator's test of: see tools/check_lattice_emit.sh (casts).
#include <stdint.h>
#include <string.h>
#include <stddef.h>
namespace casts {
template <typename T, size_t N>
static unsigned copyIt(const uint8_t *input, uint8_t *output, size_t skip)
{
    const T *offsetInput = reinterpret_cast<const T *>(input);
    T alignedBuffer[N];
    uint8_t *raw = reinterpret_cast<uint8_t *>(&alignedBuffer[0]);
    for (size_t i = 0; i < skip; i++) raw[i] = 0;
    memcpy(&raw[skip], &reinterpret_cast<const uint8_t *>(&offsetInput[0])[skip], N * sizeof(T) - skip);
    memcpy(output, &alignedBuffer[0], N * sizeof(T));
    unsigned sum = 0;
    for (size_t i = 0; i < N; i++) sum = sum * 31 + (unsigned)alignedBuffer[i];
    return sum;
}
unsigned run()
{
    uint8_t in[16];
    uint8_t out[16];
    for (int i = 0; i < 16; i++) in[i] = (uint8_t)(i * 17 + 3);
    unsigned a = copyIt<uint32_t, 4>(in, out, 3);
    unsigned b = copyIt<uint16_t, 8>(in, out, 1);
    unsigned c2 = copyIt<uint8_t, 16>(in, out, 5);
    return a ^ (b * 7) ^ out[5] ^ (c2 * 13);
}
}
