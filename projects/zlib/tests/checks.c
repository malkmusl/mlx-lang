/* The check values (Adler-32, CRC-32 and their combinations), the CRC
   table, the error strings, the bounds, the structures' layout. */
#include <stdio.h>
#include <stddef.h>
#include <zlib.h>
#include "corpus.h"

int main(void) {
    printf("sizes z_stream %zu gz_header %zu uLong %zu uInt %zu z_off_t %zu\n", sizeof(z_stream), sizeof(gz_header), sizeof(uLong), sizeof(uInt), sizeof(z_off_t));
    printf("offsets %zu %zu %zu %zu %zu %zu %zu\n", offsetof(z_stream, avail_in), offsetof(z_stream, msg), offsetof(z_stream, zalloc),
           offsetof(z_stream, data_type), offsetof(z_stream, adler), offsetof(gz_header, extra), offsetof(gz_header, done));
    printf("version %c %d\n", zlibVersion()[0], zlibVersion()[0] == ZLIB_VERSION[0]);
    printf("flags %lx\n", zlibCompileFlags());
    printf("initial adler %lu crc %lu\n", adler32(0, Z_NULL, 0), crc32(0, Z_NULL, 0));
    for (int code = -7; code <= 2; code++) printf("zError(%d) \"%s\"\n", code, zError(code));
    const z_crc_t *table = get_crc_table();
    printf("crc table %08x %08x %08x %08x\n", table[0], table[1], table[128], table[255]);
    for (int index = 0; index < CORPUS_COUNT; index++) {
        size_t size;
        unsigned char *data = corpus_sample(index, &size);
        uLong a = adler32(1, data, (uInt)size);
        uLong c = crc32(0, data, (uInt)size);
        printf("sample %d size %zu adler %08lx crc %08lx z %08lx %08lx\n", index, size, a, c, adler32_z(1, data, size), crc32_z(0, data, size));
        /* Pieces at odd places, combined. */
        size_t cut = size / 3 + 1 < size ? size / 3 + 1 : size;
        uLong a1 = adler32(1, data, (uInt)cut), a2 = adler32(1, data + cut, (uInt)(size - cut));
        uLong c1 = crc32(0, data, (uInt)cut), c2 = crc32(0, data + cut, (uInt)(size - cut));
        printf("  combined adler %08lx crc %08lx gen %08lx op %08lx\n", adler32_combine(a1, a2, (z_off_t)(size - cut)), crc32_combine(c1, c2, (z_off_t)(size - cut)),
               crc32_combine_gen((z_off_t)(size - cut)), crc32_combine_op(c1, c2, crc32_combine_gen((z_off_t)(size - cut))));
        /* A byte at a time. */
        uLong ab = 1, cb = 0;
        for (size_t i = 0; i < size && i < 5000; i++) {
            ab = adler32(ab, data + i, 1);
            cb = crc32(cb, data + i, 1);
        }
        printf("  bytewise adler %08lx crc %08lx\n", ab, cb);
        printf("  compressBound %lu deflateBound %lu\n", compressBound(size), deflateBound(Z_NULL, size));
        free(data);
    }
    printf("combine64 %08lx %08lx\n", adler32_combine64(0x12345678, 0x9abcdef0, 1000000007), crc32_combine64(0x12345678, 0x9abcdef0, 1000000007));
    printf("combine negative %08lx\n", adler32_combine(1, 1, -5));
    printf("big adler %08lx\n", adler32(0xffffffff, (const Bytef *)"abcdefghijklmnopqrstuvwxyz0123456789", 36));
    return 0;
}
