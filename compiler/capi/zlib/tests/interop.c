/* Round trips between two zlibs through files: built against one
   library, `interop pack DIR` writes streams of the test samples (every
   format, level, strategy, memory level and flush, and gzip files made
   with gzwrite) into DIR; built against another, `interop unpack DIR`
   inflates them (and reads the gzip files with gzread, the zlib streams
   with uncompress) and says whether each gives its sample back. The
   check builds it with the system's compiler and zlib as the reference,
   and with mlx-cc and mlx-zlib, and runs it both ways. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "corpus.h"

struct config { int level, wbits, mem, strategy, flush_every; };

static const struct config configs[] = {
    {0, 15, 8, 0, 0}, {1, 15, 8, 0, 0}, {3, 15, 8, 0, 0}, {6, 15, 8, 0, 0}, {9, 15, 8, 0, 0},
    {0, 31, 8, 0, 0}, {1, 31, 8, 0, 0}, {6, 31, 8, 0, 0}, {9, 31, 8, 0, 0},
    {0, -15, 8, 0, 0}, {2, -15, 8, 0, 0}, {6, -15, 8, 0, 0}, {9, -15, 8, 0, 0},
    {1, 9, 8, 0, 0}, {6, 9, 8, 0, 0}, {9, -9, 8, 0, 0}, {6, 28, 8, 0, 0}, {5, 12, 8, 0, 0},
    {6, 15, 8, Z_FILTERED, 0}, {6, 15, 8, Z_HUFFMAN_ONLY, 0}, {6, 15, 8, Z_RLE, 0}, {6, 15, 8, Z_FIXED, 0},
    {1, 31, 8, Z_FIXED, 0}, {9, -15, 8, Z_RLE, 0},
    {6, 15, 1, 0, 0}, {6, 15, 9, 0, 0}, {9, 31, 2, 0, 0}, {1, -15, 1, 0, 0},
    {6, 15, 8, 0, 1000}, {6, 31, 8, 0, 33000}, {1, -15, 8, 0, 7}, {0, 15, 8, 0, 5000},
};
#define CONFIGS (int)(sizeof configs / sizeof *configs)

static unsigned char *buffer;
static size_t buffer_cap = 1 << 21;

static void name_of(char *name, size_t size, const char *dir, int sample, int config) {
    snprintf(name, size, "%s/s%d-c%d.z", dir, sample, config);
}

static int write_file(const char *path, const unsigned char *data, size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fwrite(data, 1, size, f);
    return fclose(f);
}

static unsigned char *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = malloc(*size + 1);
    if (fread(data, 1, *size, f) != *size) *size = 0;
    fclose(f);
    return data;
}

static size_t pack(const unsigned char *data, size_t size, const struct config *c) {
    z_stream s;
    memset(&s, 0, sizeof s);
    if (deflateInit2(&s, c->level, Z_DEFLATED, c->wbits, c->mem, c->strategy) != Z_OK) return 0;
    s.next_out = buffer;
    s.avail_out = (uInt)buffer_cap;
    size_t pos = 0;
    do {
        size_t n = c->flush_every && size - pos > (size_t)c->flush_every ? (size_t)c->flush_every : size - pos;
        s.next_in = (Bytef *)data + pos;
        s.avail_in = (uInt)n;
        pos += n;
        int flush = pos == size ? Z_FINISH : c->flush_every ? (pos / c->flush_every) % 2 ? Z_SYNC_FLUSH : Z_FULL_FLUSH : Z_NO_FLUSH;
        deflate(&s, flush);
    } while (pos < size);
    size_t made = s.total_out;
    deflateEnd(&s);
    return made;
}

static int pack_all(const char *dir) {
    char name[512];
    for (int i = 0; i < CORPUS_COUNT; i++) {
        size_t size;
        unsigned char *data = corpus_sample(i, &size);
        for (int k = 0; k < CONFIGS; k++) {
            size_t made = pack(data, size, &configs[k]);
            name_of(name, sizeof name, dir, i, k);
            if (write_file(name, buffer, made) != 0) return 1;
        }
        /* A gzip file in pieces, and compress's stream. */
        snprintf(name, sizeof name, "%s/s%d.gz", dir, i);
        gzFile file = gzopen(name, "wb9");
        for (size_t at = 0; at < size; at += 7777) gzwrite(file, data + at, (unsigned)(size - at < 7777 ? size - at : 7777));
        gzclose(file);
        uLongf len = (uLongf)buffer_cap;
        compress2(buffer, &len, data, (uLong)size, 7);
        snprintf(name, sizeof name, "%s/s%d.compress", dir, i);
        write_file(name, buffer, len);
        free(data);
    }
    return 0;
}

static int unpack_all(const char *dir) {
    char name[512];
    int failures = 0;
    for (int i = 0; i < CORPUS_COUNT; i++) {
        size_t size;
        unsigned char *data = corpus_sample(i, &size);
        unsigned long crc = crc32(0, data, (uInt)size);
        for (int k = 0; k < CONFIGS; k++) {
            name_of(name, sizeof name, dir, i, k);
            size_t packed_size;
            unsigned char *packed = read_file(name, &packed_size);
            if (!packed) {
                printf("s%d-c%d: missing\n", i, k);
                failures++;
                continue;
            }
            z_stream s;
            memset(&s, 0, sizeof s);
            int wbits = configs[k].wbits;
            inflateInit2(&s, wbits < 0 ? -15 : wbits > 15 ? 31 : 15);
            s.next_in = packed;
            s.avail_in = (uInt)packed_size;
            s.next_out = buffer;
            s.avail_out = (uInt)buffer_cap;
            int ret = inflate(&s, Z_FINISH);
            int ok = ret == Z_STREAM_END && s.total_out == size && crc32(0, buffer, (uInt)s.total_out) == crc && s.avail_in == 0;
            printf("s%d-c%d: %s (%d, %lu bytes)\n", i, k, ok ? "ok" : "WRONG", ret, s.total_out);
            failures += !ok;
            inflateEnd(&s);
            free(packed);
        }
        snprintf(name, sizeof name, "%s/s%d.gz", dir, i);
        gzFile file = gzopen(name, "rb");
        unsigned long got_crc = 0;
        long total = 0;
        int got;
        while ((got = gzread(file, buffer, 5000)) > 0) {
            got_crc = crc32(got_crc, buffer, (uInt)got);
            total += got;
        }
        int ok = got == 0 && (size_t)total == size && got_crc == crc;
        printf("s%d.gz: %s (%ld bytes)\n", i, ok ? "ok" : "WRONG", total);
        failures += !ok;
        gzclose(file);
        snprintf(name, sizeof name, "%s/s%d.compress", dir, i);
        size_t packed_size;
        unsigned char *packed = read_file(name, &packed_size);
        uLongf len = (uLongf)buffer_cap;
        int ret = packed ? uncompress(buffer, &len, packed, (uLong)packed_size) : -100;
        ok = ret == Z_OK && len == size && crc32(0, buffer, (uInt)len) == crc;
        printf("s%d.compress: %s (%d, %lu bytes)\n", i, ok ? "ok" : "WRONG", ret, len);
        failures += !ok;
        free(packed);
        free(data);
    }
    printf("%d failures\n", failures);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: interop pack|unpack DIR\n");
        return 2;
    }
    buffer = malloc(buffer_cap);
    if (strcmp(argv[1], "pack") == 0) return pack_all(argv[2]);
    return unpack_all(argv[2]);
}
