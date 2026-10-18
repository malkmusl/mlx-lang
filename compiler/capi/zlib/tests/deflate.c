/* deflate: every level and strategy, the window sizes and memory levels,
   the zlib, gzip and raw formats, input and output in pieces of every
   size, the flushes, parameters changed on the way, dictionaries, primed
   bits, gzip headers, copies and resets, the bounds, the errors; and
   compress. Each stream is printed as its size and CRC-32, so the bytes
   are compared with what zlib makes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "corpus.h"

static unsigned char *out;
static size_t out_cap = 1 << 20;

static void show(const char *label, z_stream *s, int ret) {
    printf("%s: ret %d in %lu out %lu crc %08lx adler %08lx type %d msg %s\n", label, ret, s->total_in, s->total_out,
           crc32(0, out, (uInt)s->total_out), s->adler, s->data_type, s->msg ? s->msg : "-");
}

/* Compresses DATA in input pieces of ICHUNK and output pieces of OCHUNK,
   with FLUSH after each input piece (Z_FINISH at the end); the calls'
   return codes are summed into a hash. */
static int run(const char *label, const unsigned char *data, size_t size, int level, int wbits, int mem, int strategy, size_t ichunk, size_t ochunk, int flush) {
    z_stream s;
    memset(&s, 0, sizeof s);
    int ret = deflateInit2(&s, level, Z_DEFLATED, wbits, mem, strategy);
    if (ret != Z_OK) {
        printf("%s: init %d\n", label, ret);
        return ret;
    }
    size_t pos = 0;
    unsigned long hash = 0;
    int calls = 0;
    for (;;) {
        size_t n = size - pos < ichunk ? size - pos : ichunk;
        int last = pos + n == size;
        s.next_in = (Bytef *)data + pos;
        s.avail_in = (uInt)n;
        do {
            s.next_out = out + s.total_out;
            s.avail_out = (uInt)(out_cap - s.total_out < ochunk ? out_cap - s.total_out : ochunk);
            ret = deflate(&s, last ? Z_FINISH : flush);
            hash = hash * 33 + (unsigned long)(ret + 10) + s.avail_out % 7;
            calls++;
        } while (ret == Z_OK && (s.avail_out == 0 || (last && ret != Z_STREAM_END)));
        pos += n - s.avail_in;
        if (last || ret < 0) break;
    }
    printf("[%d calls hash %08lx] ", calls, hash & 0xffffffff);
    show(label, &s, ret);
    unsigned pending;
    int bits;
    deflatePending(&s, &pending, &bits);
    int end = deflateEnd(&s);
    if (end != Z_OK) printf("  deflateEnd %d\n", end);
    return ret;
}

static int allocs, frees;
static voidpf counting_alloc(voidpf opaque, uInt items, uInt size) {
    allocs++;
    (*(int *)opaque)++;
    return calloc(items, size);
}
static void counting_free(voidpf opaque, voidpf address) {
    frees++;
    free(address);
}

int main(void) {
    out = malloc(out_cap);
    unsigned char *sample[CORPUS_COUNT];
    size_t sizes[CORPUS_COUNT];
    for (int i = 0; i < CORPUS_COUNT; i++) sample[i] = corpus_sample(i, &sizes[i]);
    char label[200];

    /* Every sample at the default settings. */
    for (int i = 0; i < CORPUS_COUNT; i++) {
        snprintf(label, sizeof label, "sample %d default", i);
        run(label, sample[i], sizes[i], Z_DEFAULT_COMPRESSION, 15, 8, Z_DEFAULT_STRATEGY, 1 << 30, 1 << 30, Z_NO_FLUSH);
    }
    /* Every level and strategy. */
    int some[] = {2, 3, 6};
    for (int k = 0; k < 3; k++)
        for (int strategy = 0; strategy <= Z_FIXED; strategy++)
            for (int level = 0; level <= 9; level++) {
                snprintf(label, sizeof label, "sample %d level %d strategy %d", some[k], level, strategy);
                run(label, sample[some[k]], sizes[some[k]], level, 15, 8, strategy, 1 << 30, 1 << 30, Z_NO_FLUSH);
            }
    /* Window sizes, formats and memory levels. */
    int wbits[] = {15, -15, 31, 9, -9, 12, 8, 24, -8, 7, 16};
    int levels[] = {1, 6, 9};
    int mems[] = {1, 8, 9, 2};
    for (int w = 0; w < 11; w++)
        for (int l = 0; l < 3; l++)
            for (int m = 0; m < 4; m++) {
                snprintf(label, sizeof label, "mix wbits %d level %d mem %d", wbits[w], levels[l], mems[m]);
                run(label, sample[6], sizes[6], levels[l], wbits[w], mems[m], Z_DEFAULT_STRATEGY, 1 << 30, 1 << 30, Z_NO_FLUSH);
            }
    /* Pieces of input and output, and the flushes. */
    size_t chunks[][2] = {{1, 1}, {7, 13}, {1000, 64}, {65536, 3}, {4096, 4096}, {3, 100000}};
    int flushes[] = {Z_NO_FLUSH, Z_SYNC_FLUSH, Z_FULL_FLUSH, Z_PARTIAL_FLUSH, Z_BLOCK};
    for (int c = 0; c < 6; c++)
        for (int f = 0; f < 5; f++)
            for (int level = 0; level <= 9; level += 3) {
                size_t size = chunks[c][0] < 100 ? 6000 : sizes[6];
                if (f > 0 && chunks[c][0] < 1000) size = 3000;
                /* (A flush needs room for its marker: zlib.h asks for more
                   than 6 bytes of output.) */
                size_t ochunk = f > 0 && chunks[c][1] < 8 ? 8 : chunks[c][1];
                snprintf(label, sizeof label, "pieces %zu/%zu flush %d level %d", chunks[c][0], ochunk, flushes[f], level);
                run(label, sample[6], size, level, f % 2 ? 31 : 15, 8, Z_DEFAULT_STRATEGY, chunks[c][0], ochunk, flushes[f]);
            }

    /* Parameters changed on the way. */
    {
        z_stream s;
        memset(&s, 0, sizeof s);
        deflateInit(&s, 1);
        s.next_out = out;
        s.avail_out = (uInt)out_cap;
        int params[][2] = {{9, 0}, {0, 0}, {6, Z_FILTERED}, {0, 0}, {3, Z_HUFFMAN_ONLY}, {6, Z_RLE}, {9, Z_FIXED}, {Z_DEFAULT_COMPRESSION, 0}, {1, 0}};
        size_t step = sizes[6] / 10;
        for (int k = 0; k < 9; k++) {
            s.next_in = sample[6] + k * step;
            s.avail_in = (uInt)step;
            int ret = deflate(&s, Z_NO_FLUSH);
            int pret = deflateParams(&s, params[k][0], params[k][1]);
            printf("params %d %d: deflate %d params %d in %lu out %lu\n", params[k][0], params[k][1], ret, pret, s.total_in, s.total_out);
        }
        s.next_in = sample[6] + 9 * step;
        s.avail_in = (uInt)(sizes[6] - 9 * step);
        show("params end", &s, deflate(&s, Z_FINISH));
        printf("params bad %d %d\n", deflateParams(&s, 10, 0), deflateParams(&s, 5, 9));
        deflateEnd(&s);
    }
    /* A dictionary, in the zlib and the raw formats; the dictionary deflate keeps. */
    for (int raw = 0; raw < 2; raw++) {
        z_stream s;
        memset(&s, 0, sizeof s);
        deflateInit2(&s, 6, Z_DEFLATED, raw ? -15 : 15, 8, Z_DEFAULT_STRATEGY);
        int set = deflateSetDictionary(&s, sample[2] + 500, 40000);
        printf("dictionary %d: set %d adler %08lx\n", raw, set, s.adler);
        unsigned char kept[40000];
        uInt kept_len = 0;
        printf("  get %d %u crc %08lx\n", deflateGetDictionary(&s, kept, &kept_len), kept_len, crc32(0, kept, kept_len));
        s.next_in = sample[2];
        s.avail_in = 20000;
        s.next_out = out;
        s.avail_out = (uInt)out_cap;
        show(raw ? "dictionary raw" : "dictionary zlib", &s, deflate(&s, Z_FINISH));
        printf("  set after start %d\n", deflateSetDictionary(&s, sample[2], 100));
        deflateEnd(&s);
    }
    {
        z_stream s;
        memset(&s, 0, sizeof s);
        deflateInit2(&s, 6, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
        printf("dictionary gzip %d\n", deflateSetDictionary(&s, sample[2], 100));
        deflateEnd(&s);
    }
    /* Bits before the data (raw), and what is pending. */
    {
        z_stream s;
        memset(&s, 0, sizeof s);
        deflateInit2(&s, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
        printf("prime %d %d %d %d\n", deflatePrime(&s, 3, 5), deflatePrime(&s, 16, 0xabcd), deflatePrime(&s, 17, 1), deflatePrime(&s, -1, 1));
        unsigned pending;
        int bits;
        printf("pending %d", deflatePending(&s, &pending, &bits));
        printf(" %u %d\n", pending, bits);
        s.next_in = sample[1];
        s.avail_in = 5;
        s.next_out = out;
        s.avail_out = (uInt)out_cap;
        show("primed", &s, deflate(&s, Z_FINISH));
        deflateEnd(&s);
    }
    /* gzip headers. */
    for (int k = 0; k < 4; k++) {
        z_stream s;
        memset(&s, 0, sizeof s);
        deflateInit2(&s, k * 3, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
        gz_header head;
        memset(&head, 0, sizeof head);
        head.text = k & 1;
        head.time = 1700000000 + k;
        head.os = 3 + k;
        unsigned char extra[3000];
        for (int i = 0; i < 3000; i++) extra[i] = (unsigned char)(i * 7);
        if (k >= 1) { head.extra = extra; head.extra_len = k == 3 ? 3000 : 10; }
        if (k >= 2) head.name = (Bytef *)"file.txt";
        if (k >= 2) head.comment = (Bytef *)"a comment";
        head.hcrc = k >= 1;
        printf("header %d: set %d bound %lu\n", k, deflateSetHeader(&s, &head), deflateBound(&s, sizes[2]));
        s.next_in = sample[2];
        s.avail_in = (uInt)sizes[2];
        s.next_out = out;
        s.avail_out = k == 3 ? 100 : (uInt)out_cap;
        int ret;
        while ((ret = deflate(&s, Z_FINISH)) == Z_OK) {
            s.avail_out = 100;
        }
        snprintf(label, sizeof label, "header %d", k);
        show(label, &s, ret);
        deflateEnd(&s);
    }
    {
        z_stream s;
        memset(&s, 0, sizeof s);
        deflateInit(&s, 6);
        gz_header head;
        memset(&head, 0, sizeof head);
        printf("header on zlib %d\n", deflateSetHeader(&s, &head));
        deflateEnd(&s);
    }
    /* A copy made on the way: both go on alike. */
    {
        z_stream s, t;
        memset(&s, 0, sizeof s);
        deflateInit(&s, 9);
        s.next_in = sample[6];
        s.avail_in = 80000;
        s.next_out = out;
        s.avail_out = (uInt)out_cap;
        deflate(&s, Z_NO_FLUSH);
        printf("copy %d\n", deflateCopy(&t, &s));
        s.avail_in = (uInt)(sizes[6] - 80000);
        show("original", &s, deflate(&s, Z_FINISH));
        unsigned long first = crc32(0, out, (uInt)s.total_out);
        t.next_out = out;
        t.avail_out = (uInt)out_cap;
        t.total_out = 0;
        t.avail_in = (uInt)(sizes[6] - 80000);
        int ret = deflate(&t, Z_FINISH);
        printf("copy ret %d out %lu crc %08lx (original %08lx)\n", ret, t.total_out, crc32(0, out, (uInt)t.total_out), first);
        deflateEnd(&s);
        deflateEnd(&t);
    }
    /* Reset and tune. */
    {
        z_stream s;
        memset(&s, 0, sizeof s);
        deflateInit(&s, 6);
        for (int k = 0; k < 3; k++) {
            if (k == 2) printf("tune %d\n", deflateTune(&s, 4, 8, 16, 32));
            s.next_in = sample[2];
            s.avail_in = 30000;
            s.next_out = out;
            s.avail_out = (uInt)out_cap;
            snprintf(label, sizeof label, "reset %d", k);
            show(label, &s, deflate(&s, Z_FINISH));
            printf("  reset %d\n", deflateReset(&s));
        }
        deflateEnd(&s);
    }
    /* The bounds, against what is made. */
    for (int w = 0; w < 4; w++)
        for (int m = 0; m < 3; m++) {
            z_stream s;
            memset(&s, 0, sizeof s);
            deflateInit2(&s, 9, Z_DEFLATED, wbits[w], mems[m], Z_DEFAULT_STRATEGY);
            uLong bound = deflateBound(&s, sizes[4]);
            s.next_in = sample[4];
            s.avail_in = (uInt)sizes[4];
            s.next_out = out;
            s.avail_out = (uInt)bound;
            int ret = deflate(&s, Z_FINISH);
            printf("bound wbits %d mem %d: %lu ret %d out %lu\n", wbits[w], mems[m], bound, ret, s.total_out);
            deflateEnd(&s);
        }
    /* Errors. */
    {
        z_stream s;
        memset(&s, 0, sizeof s);
        printf("init errors %d %d %d %d %d %d %d %d %d\n", deflateInit2(&s, 10, Z_DEFLATED, 15, 8, 0), deflateInit2(&s, 6, 7, 15, 8, 0),
               deflateInit2(&s, 6, Z_DEFLATED, 16 + 8, 8, 0), deflateInit2(&s, 6, Z_DEFLATED, -16, 8, 0), deflateInit2(&s, 6, Z_DEFLATED, 15, 0, 0),
               deflateInit2(&s, 6, Z_DEFLATED, 15, 10, 0), deflateInit2(&s, 6, Z_DEFLATED, 15, 8, 5), deflateInit_(&s, 6, "2.0", sizeof s),
               deflateInit_(&s, 6, ZLIB_VERSION, sizeof s - 8));
        printf("null %d %d %d %d\n", deflateInit(NULL, 6), deflate(NULL, Z_FINISH), deflateEnd(NULL), deflateReset(NULL));
        deflateInit(&s, 6);
        printf("bad flush %d %d\n", deflate(&s, -1), deflate(&s, 6));
        s.next_out = NULL;
        s.avail_out = 10;
        int ret = deflate(&s, Z_NO_FLUSH);
        printf("no output %d %s\n", ret, s.msg);
        s.next_out = out;
        s.avail_out = 0;
        s.msg = NULL;
        ret = deflate(&s, Z_NO_FLUSH);
        printf("no room %d %s\n", ret, s.msg ? s.msg : "-");
        s.avail_out = 1000;
        s.msg = NULL;
        ret = deflate(&s, Z_NO_FLUSH);
        printf("nothing to do %d %s\n", ret, s.msg ? s.msg : "-");
        ret = deflate(&s, Z_SYNC_FLUSH);
        printf("sync %d, again %d\n", ret, deflate(&s, Z_SYNC_FLUSH));
        s.next_in = sample[2];
        s.avail_in = 1000;
        ret = deflate(&s, Z_NO_FLUSH);
        printf("busy end %d\n", deflateEnd(&s));
        printf("end again %d\n", deflateEnd(&s));
        memset(&s, 0, sizeof s);
        deflateInit(&s, 6);
        s.next_in = sample[1];
        s.avail_in = 5;
        s.next_out = out;
        s.avail_out = 1000;
        ret = deflate(&s, Z_FINISH);
        int more = deflate(&s, Z_NO_FLUSH);
        printf("after finish %d %d %s, finish again %d\n", ret, more, s.msg ? s.msg : "-", deflate(&s, Z_FINISH));
        s.avail_in = 3;
        s.msg = NULL;
        printf("input after finish %d %s\n", deflate(&s, Z_FINISH), s.msg ? s.msg : "-");
        printf("end %d\n", deflateEnd(&s));
    }
    /* The allocator's calls. */
    {
        z_stream s;
        memset(&s, 0, sizeof s);
        int count = 0;
        s.zalloc = counting_alloc;
        s.zfree = counting_free;
        s.opaque = &count;
        deflateInit2(&s, 9, Z_DEFLATED, 31, 9, 0);
        s.next_in = sample[2];
        s.avail_in = 50000;
        s.next_out = out;
        s.avail_out = (uInt)out_cap;
        show("allocator", &s, deflate(&s, Z_FINISH));
        deflateEnd(&s);
        printf("  allocs %d frees %d opaque %d\n", allocs, frees, count);
    }
    /* compress and compress2. */
    for (int level = -1; level <= 10; level++) {
        uLongf len = (uLongf)out_cap;
        int ret = compress2(out, &len, sample[3], (uLong)sizes[3], level);
        printf("compress2 level %d: %d %lu crc %08lx\n", level, ret, len, crc32(0, out, (uInt)len));
    }
    {
        uLongf len = (uLongf)out_cap;
        int ret = compress(out, &len, sample[2], (uLong)sizes[2]);
        printf("compress %d %lu crc %08lx\n", ret, len, crc32(0, out, (uInt)len));
        len = 100;
        ret = compress(out, &len, sample[2], (uLong)sizes[2]);
        printf("compress short %d %lu\n", ret, len);
        len = compressBound(0);
        ret = compress(out, &len, sample[0], 0);
        printf("compress empty %d %lu crc %08lx\n", ret, len, crc32(0, out, (uInt)len));
    }
    return 0;
}
