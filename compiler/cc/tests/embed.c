/* #embed (C23 6.10.3.2) as mlx-cc reads it with -std=c23: the bytes of a
   file as integer constants, with limit, prefix, suffix and if_empty,
   and __has_embed. The expected output follows from the standard (gcc
   13 has no #embed): embed.bin holds "MLX embed test!\n", 16 bytes;
   empty.bin is empty. */
#include <stdio.h>

static const unsigned char whole[] = {
#embed "embed.bin"
};
static const unsigned char first[] = {
#embed "embed.bin" limit(3) prefix(0xAA,) suffix(, 0xBB)
};
static const int nothing[] = {
#embed "empty.bin" if_empty(7, 8, 9) prefix(1,)
};
static const char text[] = {
#embed <embed.bin> __limit__(15)
    , 0
};

int main(void)
{
    unsigned sum = 0;
    for (unsigned i = 0; i < sizeof whole; i++) sum += whole[i];
    printf("%zu %u %c%c%c\n", sizeof whole, sum, whole[0], whole[1], whole[2]);
    printf("%zu %02X %02X %02X %02X %02X\n", sizeof first, first[0], first[1], first[2], first[3], first[4]);
    printf("%zu %d %d %d\n", sizeof nothing / sizeof nothing[0], nothing[0], nothing[1], nothing[2]);
    printf("%s|%zu\n", text, sizeof text);
#if __has_embed("embed.bin") == __STDC_EMBED_FOUND__ && __has_embed(<embed.bin>) == 1
    printf("found\n");
#endif
#if __has_embed("empty.bin") == __STDC_EMBED_EMPTY__
    printf("empty\n");
#endif
#if __has_embed("nowhere.bin") == __STDC_EMBED_NOT_FOUND__
    printf("not found\n");
#endif
    return 0;
}
