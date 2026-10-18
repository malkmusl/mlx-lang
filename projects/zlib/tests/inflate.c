/* inflate: streams of every level, format and window size, input and
   output in pieces of every size, automatic detection of the format,
   gzip headers, dictionaries, synchronization, marks, copies, primed
   bits, the Z_BLOCK and Z_TREES stops, damaged streams (one of each
   error zlib names, and many damaged at random) with zlib's codes and
   messages, uncompress, and inflateBack. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "corpus.h"

static unsigned char *out;
static size_t out_cap = 1 << 20;
static unsigned char packed[1 << 20];

/* Inflates IN (LEN bytes) in pieces; prints the outcome. */
static int run(const char *label, const unsigned char *in, size_t len, int wbits, size_t ichunk, size_t ochunk, int flush) {
    z_stream s;
    memset(&s, 0, sizeof s);
    int ret = inflateInit2(&s, wbits);
    if (ret != Z_OK) {
        printf("%s: init %d\n", label, ret);
        return ret;
    }
    size_t pos = 0;
    unsigned long hash = 0;
    int calls = 0;
    for (;;) {
        size_t n = len - pos < ichunk ? len - pos : ichunk;
        s.next_in = (Bytef *)in + pos;
        s.avail_in = (uInt)n;
        s.next_out = out + s.total_out;
        s.avail_out = (uInt)(out_cap - s.total_out < ochunk ? out_cap - s.total_out : ochunk);
        uLong before = s.total_out;
        ret = inflate(&s, flush);
        calls++;
        hash = hash * 33 + (unsigned long)(ret + 10) * 7 + (unsigned long)s.data_type;
        pos += n - s.avail_in;
        int progress = n != s.avail_in || s.total_out != before;
        if (ret == Z_STREAM_END || ret == Z_NEED_DICT || (ret != Z_OK && ret != Z_BUF_ERROR) || !progress) break;
    }
    printf("%s: ret %d msg %s in %lu out %lu crc %08lx adler %08lx used %zu [%d calls %08lx] mark %ld\n", label, ret, s.msg ? s.msg : "-", s.total_in, s.total_out,
           crc32(0, out, (uInt)s.total_out), s.adler, pos, calls, hash & 0xffffffff, inflateMark(&s));
    inflateEnd(&s);
    return ret;
}

static size_t pack(const unsigned char *data, size_t size, int level, int wbits, int strategy, int flush_every) {
    z_stream s;
    memset(&s, 0, sizeof s);
    deflateInit2(&s, level, Z_DEFLATED, wbits, 8, strategy);
    s.next_out = packed;
    s.avail_out = sizeof packed;
    size_t pos = 0;
    while (pos < size) {
        size_t n = flush_every && size - pos > (size_t)flush_every ? (size_t)flush_every : size - pos;
        s.next_in = (Bytef *)data + pos;
        s.avail_in = (uInt)n;
        pos += n;
        deflate(&s, pos == size ? Z_FINISH : flush_every ? Z_FULL_FLUSH : Z_NO_FLUSH);
    }
    if (size == 0) deflate(&s, Z_FINISH);
    size_t made = s.total_out;
    deflateEnd(&s);
    return made;
}

/* Bits for streams made by hand, the first bit first. */
static unsigned char hand[4096];
static size_t hand_bits;
static void bits(unsigned value, int count) {
    for (int i = 0; i < count; i++, hand_bits++) {
        if ((hand_bits & 7) == 0) hand[hand_bits >> 3] = 0;
        if ((value >> i) & 1) hand[hand_bits >> 3] |= (unsigned char)(1 << (hand_bits & 7));
    }
}
/* A Huffman code: its bits from the first (most significant) on. */
static void code(unsigned value, int count) {
    for (int i = count - 1; i >= 0; i--) bits((value >> i) & 1, 1);
}
static size_t hand_bytes(void) {
    size_t n = (hand_bits + 7) >> 3;
    for (int i = 0; i < 8; i++) hand[n + i] = 0;
    return n + 8;
}
static void hand_start(void) { hand_bits = 0; }

/* A dynamic block header whose code length code gives symbols A and B a
   length of 1 each (both in the first HCLEN positions). */
static const int order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
static void dynamic_header(int hlit, int hdist, int a, int b) {
    bits(1, 1);
    bits(2, 2);
    bits((unsigned)hlit, 5);
    bits((unsigned)hdist, 5);
    int last = 0;
    for (int i = 0; i < 19; i++) if (order[i] == a || order[i] == b) last = i;
    int count = last + 1 < 4 ? 4 : last + 1;
    bits((unsigned)(count - 4), 4);
    for (int i = 0; i < count; i++) bits(order[i] == a || order[i] == b ? 1 : 0, 3);
}

static unsigned char window[1 << 15];
static unsigned char *back_in;
static size_t back_left, back_piece;
static unsigned back_input(void *desc, unsigned char **buf) {
    size_t n = back_left < back_piece ? back_left : back_piece;
    *buf = back_in;
    back_in += n;
    back_left -= n;
    (*(int *)desc)++;
    return (unsigned)n;
}
static unsigned long back_crc;
static size_t back_total;
static int back_output(void *desc, unsigned char *buf, unsigned len) {
    back_crc = crc32(back_crc, buf, len);
    back_total += len;
    (*(int *)desc)++;
    return back_total > 150000;
}

int main(void) {
    out = malloc(out_cap);
    unsigned char *sample[CORPUS_COUNT];
    size_t sizes[CORPUS_COUNT];
    for (int i = 0; i < CORPUS_COUNT; i++) sample[i] = corpus_sample(i, &sizes[i]);
    char label[200];

    /* Every sample, level and format. */
    int wbits[] = {15, -15, 31, 9, -9, 12};
    for (int i = 0; i < CORPUS_COUNT; i++)
        for (int w = 0; w < 6; w++)
            for (int level = 0; level <= 9; level += 3) {
                size_t n = pack(sample[i], sizes[i], level, wbits[w], Z_DEFAULT_STRATEGY, 0);
                snprintf(label, sizeof label, "sample %d wbits %d level %d (%zu)", i, wbits[w], level, n);
                run(label, packed, n, wbits[w] < 0 ? -15 : wbits[w] > 15 ? 31 : 15, 1 << 30, 1 << 30, Z_NO_FLUSH);
            }
    /* Pieces, flushes, detection. */
    size_t n = pack(sample[6], sizes[6], 6, 31, Z_DEFAULT_STRATEGY, 30000);
    size_t chunks[][2] = {{1, 1 << 30}, {1 << 30, 1}, {7, 13}, {300, 4096}, {65536, 100}, {5, 258}};
    for (int c = 0; c < 6; c++) {
        snprintf(label, sizeof label, "pieces %zu/%zu", chunks[c][0], chunks[c][1]);
        run(label, packed, n, 47, chunks[c][0], chunks[c][1], Z_NO_FLUSH);
    }
    run("finish at once", packed, n, 31, 1 << 30, 1 << 30, Z_FINISH);
    run("finish short", packed, n, 31, 1 << 30, 1000, Z_FINISH);
    run("sync flush", packed, n, 31, 1000, 1000, Z_SYNC_FLUSH);
    run("block", packed, n, 31, 1000, 1 << 30, Z_BLOCK);
    run("trees", packed, n, 31, 1000, 1 << 30, Z_TREES);
    run("gzip as zlib", packed, n, 15, 1 << 30, 1 << 30, Z_NO_FLUSH);
    run("gzip as raw", packed, n, -15, 1 << 30, 1 << 30, Z_NO_FLUSH);
    run("gzip with zlib or gzip", packed, n, 15 + 32, 1 << 30, 1 << 30, Z_NO_FLUSH);
    n = pack(sample[2], sizes[2], 9, 15, Z_DEFAULT_STRATEGY, 0);
    run("zlib with zlib or gzip", packed, n, 15 + 32, 1 << 30, 1 << 30, Z_NO_FLUSH);
    run("zlib as gzip", packed, n, 31, 1 << 30, 1 << 30, Z_NO_FLUSH);
    run("zlib with the header's window", packed, n, 0, 1 << 30, 1 << 30, Z_NO_FLUSH);
    run("zlib with a smaller window", packed, n, 9, 1 << 30, 1 << 30, Z_NO_FLUSH);
    n = pack(sample[2], sizes[2], 9, 9, Z_DEFAULT_STRATEGY, 0);
    run("small window with a larger one", packed, n, 15, 1 << 30, 1 << 30, Z_NO_FLUSH);
    for (int strategy = 1; strategy <= 4; strategy++) {
        n = pack(sample[3], sizes[3], 6, 15, strategy, 0);
        snprintf(label, sizeof label, "strategy %d", strategy);
        run(label, packed, n, 15, 100, 100, Z_NO_FLUSH);
    }

    /* The steps of Z_BLOCK and Z_TREES: data_type after each call. */
    for (int k = 0; k < 2; k++) {
        n = pack(sample[6], 60000, 6, -15, Z_DEFAULT_STRATEGY, 7000);
        z_stream s;
        memset(&s, 0, sizeof s);
        inflateInit2(&s, -15);
        s.next_in = packed;
        s.avail_in = (uInt)n;
        s.next_out = out;
        s.avail_out = (uInt)out_cap;
        printf("%s:", k ? "trees" : "block");
        int ret;
        for (int calls = 0; calls < 40; calls++) {
            ret = inflate(&s, k ? Z_TREES : Z_BLOCK);
            printf(" %d/%d/%lu", ret, s.data_type, s.total_out);
            if (ret != Z_OK) break;
        }
        printf("\n");
        inflateEnd(&s);
    }

    /* Damaged streams, one of each error. */
    struct { const char *name; unsigned char bytes[16]; size_t len; int wbits; } bad[] = {
        {"header check", {0x78, 0x9d, 0x01, 0x00}, 4, 15},
        {"zlib method", {0x77, 0x09, 0x01, 0x00}, 4, 15},
        {"window size", {0x88, 0x1c, 0x01, 0x00}, 4, 15},
        {"gzip method", {0x1f, 0x8b, 0x07, 0x00, 0, 0, 0, 0, 0, 3}, 10, 31},
        {"gzip flags", {0x1f, 0x8b, 0x08, 0xe0, 0, 0, 0, 0, 0, 3}, 10, 31},
        {"header crc", {0x1f, 0x8b, 0x08, 0x02, 0, 0, 0, 0, 0, 3, 0x12, 0x34, 0x03, 0x00}, 14, 31},
        {"block type", {0x07, 0x00}, 2, -15},
        {"stored lengths", {0x01, 0x05, 0x00, 0x00, 0x00, 1, 2, 3, 4, 5}, 10, -15},
        {"too many symbols", {0xf5, 0xff, 0xff, 0xff}, 4, -15},
        {"code lengths", {0x05, 0x00, 0x92, 0x04, 0, 0}, 6, -15},
        {"gzip only", {0x78, 0x9c, 0x03, 0x00}, 4, 31},
        {"zlib only", {0x1f, 0x8b, 0x08, 0x00}, 4, 15},
        {"empty", {0}, 0, 15},
        {"truncated", {0x78, 0x9c, 0x4b}, 3, 15},
    };
    for (size_t k = 0; k < sizeof bad / sizeof *bad; k++) {
        run(bad[k].name, bad[k].bytes, bad[k].len, bad[k].wbits, 1 << 30, 1 << 30, Z_NO_FLUSH);
        snprintf(label, sizeof label, "%s, finishing", bad[k].name);
        run(label, bad[k].bytes, bad[k].len, bad[k].wbits, 1 << 30, 1 << 30, Z_FINISH);
    }
    /* Made bit by bit. */
    hand_start();
    dynamic_header(0, 0, 16, 0);
    code(1, 1);
    bits(0, 2);
    run("repeat first", hand, hand_bytes(), -15, 1 << 30, 1 << 30, Z_NO_FLUSH);
    hand_start();
    dynamic_header(0, 0, 0, 16);
    for (int i = 0; i < 258; i++) code(0, 1);
    run("no end of block", hand, hand_bytes(), -15, 1 << 30, 1 << 30, Z_NO_FLUSH);
    hand_start();
    dynamic_header(0, 0, 0, 16);
    for (int i = 0; i < 250; i++) code(0, 1);
    code(1, 1);
    bits(3, 2);
    code(1, 1);
    bits(3, 2);
    run("repeat too long", hand, hand_bytes(), -15, 1 << 30, 1 << 30, Z_NO_FLUSH);
    hand_start();
    dynamic_header(0, 0, 0, 1);
    for (int i = 0; i < 258; i++) code(1, 1);
    run("literal lengths", hand, hand_bytes(), -15, 1 << 30, 1 << 30, Z_NO_FLUSH);
    hand_start();
    dynamic_header(0, 2, 0, 1);
    /* Literals 0..255 none, 256 (end of block) and two more of length 1;
       three distance codes of length 1. */
    for (int i = 0; i < 256; i++) code(0, 1);
    code(1, 1);
    code(1, 1);
    code(0, 1);
    code(1, 1);
    code(1, 1);
    code(1, 1);
    run("distance lengths", hand, hand_bytes(), -15, 1 << 30, 1 << 30, Z_NO_FLUSH);
    hand_start();
    bits(1, 1);
    bits(1, 2);
    code(0xc6, 8);
    run("literal code", hand, hand_bytes(), -15, 1 << 30, 1 << 30, Z_NO_FLUSH);
    hand_start();
    bits(1, 1);
    bits(1, 2);
    code(0x30 + 'a', 8);
    code(1, 7);
    code(30, 5);
    run("distance code", hand, hand_bytes(), -15, 1 << 30, 1 << 30, Z_NO_FLUSH);
    hand_start();
    bits(1, 1);
    bits(1, 2);
    code(0x30 + 'a', 8);
    code(1, 7);
    code(1, 5);
    code(0, 7);
    run("too far back", hand, hand_bytes(), -15, 1 << 30, 1 << 30, Z_NO_FLUSH);
    run("too far back, slowly", hand, hand_bytes(), -15, 1, 1, Z_NO_FLUSH);
    n = pack(sample[2], 10000, 6, 15, Z_DEFAULT_STRATEGY, 0);
    packed[n - 1] ^= 1;
    run("data check", packed, n, 15, 1 << 30, 1 << 30, Z_NO_FLUSH);
    n = pack(sample[2], 10000, 6, 31, Z_DEFAULT_STRATEGY, 0);
    packed[n - 1] ^= 1;
    run("length check", packed, n, 31, 1 << 30, 1 << 30, Z_NO_FLUSH);
    packed[n - 1] ^= 1;
    packed[n - 5] ^= 1;
    run("gzip data check", packed, n, 31, 1 << 30, 1 << 30, Z_NO_FLUSH);

    /* Damaged at random: a byte changed, for streams of each kind. */
    int kinds[][3] = {{2, 6, 15}, {3, 1, -15}, {6, 9, 31}, {4, 6, 15}, {5, 6, -15}, {3, 0, 15}};
    for (int k = 0; k < 6; k++) {
        size_t size = sizes[kinds[k][0]] < 30000 ? sizes[kinds[k][0]] : 30000;
        n = pack(sample[kinds[k][0]], size, kinds[k][1], kinds[k][2], Z_DEFAULT_STRATEGY, 0);
        unsigned char *copy = malloc(n);
        corpus_seed = 99 + (unsigned long)k;
        for (int t = 0; t < 60; t++) {
            memcpy(copy, packed, n);
            size_t at = corpus_next() % (n < 400 ? n : 400);
            if (t >= 30) at = corpus_next() % n;
            copy[at] ^= (unsigned char)(1 + corpus_next() % 255);
            snprintf(label, sizeof label, "damaged %d/%d at %zu", k, t, at);
            run(label, copy, n, kinds[k][2], 1 << 30, 1 << 30, Z_NO_FLUSH);
            if (t % 10 == 0) {
                snprintf(label, sizeof label, "damaged %d/%d at %zu, slowly", k, t, at);
                run(label, copy, n, kinds[k][2], 3, 5, Z_NO_FLUSH);
            }
        }
        free(copy);
    }

    /* gzip headers read. */
    {
        z_stream d;
        memset(&d, 0, sizeof d);
        deflateInit2(&d, 6, Z_DEFLATED, 31, 8, 0);
        gz_header head;
        memset(&head, 0, sizeof head);
        unsigned char extra[100];
        for (int i = 0; i < 100; i++) extra[i] = (unsigned char)(i + 1);
        head.text = 1;
        head.time = 1234567890;
        head.os = 11;
        head.extra = extra;
        head.extra_len = 100;
        head.name = (Bytef *)"name-of-the-file.txt";
        head.comment = (Bytef *)"the comment";
        head.hcrc = 1;
        deflateSetHeader(&d, &head);
        d.next_in = sample[2];
        d.avail_in = 5000;
        d.next_out = packed;
        d.avail_out = sizeof packed;
        deflate(&d, Z_FINISH);
        n = d.total_out;
        deflateEnd(&d);
        for (int piece = 1; piece <= 1000; piece *= 1000) {
            z_stream s;
            memset(&s, 0, sizeof s);
            inflateInit2(&s, 31);
            gz_header got;
            unsigned char gextra[40], gname[10], gcomment[64];
            memset(&got, 0, sizeof got);
            memset(gname, '#', sizeof gname);
            got.extra = gextra;
            got.extra_max = 40;
            got.name = gname;
            got.name_max = 10;
            got.comment = gcomment;
            got.comm_max = 64;
            printf("get header %d done %d\n", inflateGetHeader(&s, &got), got.done);
            size_t pos = 0;
            int ret = Z_OK;
            while (ret == Z_OK && pos < n) {
                size_t k2 = n - pos < (size_t)piece ? n - pos : (size_t)piece;
                s.next_in = packed + pos;
                s.avail_in = (uInt)k2;
                s.next_out = out + s.total_out;
                s.avail_out = 100000;
                ret = inflate(&s, Z_NO_FLUSH);
                pos += k2 - s.avail_in;
                if (got.done && piece == 1 && pos < 60) printf("  done at %zu\n", pos);
            }
            printf("header: ret %d text %d time %lu xflags %d os %d extra_len %u extra %d,%d name %.10s comment %s hcrc %d done %d out %lu\n", ret, got.text, got.time, got.xflags, got.os,
                   got.extra_len, gextra[0], gextra[39], gname, gcomment, got.hcrc, got.done, s.total_out);
            inflateEnd(&s);
        }
        z_stream s;
        memset(&s, 0, sizeof s);
        inflateInit2(&s, 15);
        printf("header on zlib %d\n", inflateGetHeader(&s, &head));
        inflateEnd(&s);
    }
    /* Dictionaries. */
    {
        z_stream d;
        memset(&d, 0, sizeof d);
        deflateInit(&d, 9);
        deflateSetDictionary(&d, sample[2] + 20000, 20000);
        d.next_in = sample[2];
        d.avail_in = 20000;
        d.next_out = packed;
        d.avail_out = sizeof packed;
        deflate(&d, Z_FINISH);
        n = d.total_out;
        deflateEnd(&d);
        for (int k = 0; k < 3; k++) {
            z_stream s;
            memset(&s, 0, sizeof s);
            inflateInit(&s);
            s.next_in = packed;
            s.avail_in = (uInt)n;
            s.next_out = out;
            s.avail_out = (uInt)out_cap;
            int ret = inflate(&s, Z_NO_FLUSH);
            printf("dictionary %d: %d adler %08lx", k, ret, s.adler);
            int set = inflateSetDictionary(&s, k == 1 ? sample[2] : sample[2] + 20000, 20000);
            printf(" set %d", set);
            if (k == 2) printf(" again %d", inflateSetDictionary(&s, sample[2] + 20000, 20000));
            ret = inflate(&s, Z_NO_FLUSH);
            printf(" then %d out %lu crc %08lx\n", ret, s.total_out, crc32(0, out, (uInt)s.total_out));
            unsigned char got[40000];
            uInt got_len = 0;
            printf("  get %d %u crc %08lx\n", inflateGetDictionary(&s, got, &got_len), got_len, crc32(0, got, got_len));
            inflateEnd(&s);
        }
        /* A raw stream with a dictionary. */
        memset(&d, 0, sizeof d);
        deflateInit2(&d, 9, Z_DEFLATED, -15, 8, 0);
        deflateSetDictionary(&d, sample[2] + 20000, 20000);
        d.next_in = sample[2];
        d.avail_in = 20000;
        d.next_out = packed;
        d.avail_out = sizeof packed;
        deflate(&d, Z_FINISH);
        n = d.total_out;
        deflateEnd(&d);
        z_stream s;
        memset(&s, 0, sizeof s);
        inflateInit2(&s, -15);
        printf("raw dictionary set %d", inflateSetDictionary(&s, sample[2] + 20000, 20000));
        s.next_in = packed;
        s.avail_in = (uInt)n;
        s.next_out = out;
        s.avail_out = (uInt)out_cap;
        int ret = inflate(&s, Z_FINISH);
        printf(" inflate %d out %lu crc %08lx\n", ret, s.total_out, crc32(0, out, (uInt)s.total_out));
        inflateEnd(&s);
    }
    /* Synchronization after damage, at the full flush points. */
    {
        n = pack(sample[6], sizes[6], 6, 15, Z_DEFAULT_STRATEGY, 25000);
        memset(packed + 3000, 0x55, 200);
        z_stream s;
        memset(&s, 0, sizeof s);
        inflateInit(&s);
        s.next_in = packed;
        s.avail_in = (uInt)n;
        s.next_out = out;
        s.avail_out = (uInt)out_cap;
        int ret = inflate(&s, Z_NO_FLUSH);
        printf("before sync: %d %s in %lu out %lu\n", ret, s.msg ? s.msg : "-", s.total_in, s.total_out);
        for (int k = 0; k < 4; k++) {
            ret = inflateSync(&s);
            printf("  sync %d in %lu point %d", ret, s.total_in, inflateSyncPoint(&s));
            if (ret != Z_OK) { printf("\n"); break; }
            ret = inflate(&s, Z_SYNC_FLUSH);
            printf(" inflate %d %s out %lu\n", ret, s.msg ? s.msg : "-", s.total_out);
            if (ret == Z_STREAM_END) break;
        }
        s.avail_in = 0;
        printf("  sync with nothing %d\n", inflateSync(&s));
        inflateEnd(&s);
    }
    /* Marks, along a stream inflated a few bytes at a time. */
    {
        n = pack(sample[2], 3000, 6, -15, Z_DEFAULT_STRATEGY, 0);
        z_stream s;
        memset(&s, 0, sizeof s);
        inflateInit2(&s, -15);
        s.next_in = packed;
        s.avail_in = (uInt)n;
        printf("marks:");
        int ret = Z_OK;
        for (int k = 0; ret == Z_OK && k < 200; k++) {
            s.next_out = out + s.total_out;
            s.avail_out = 17;
            ret = inflate(&s, Z_NO_FLUSH);
            if (k % 5 == 0) printf(" %ld", inflateMark(&s));
        }
        printf(" end %d codes %lu\n", ret, inflateCodesUsed(&s));
        inflateEnd(&s);
    }
    /* A copy made on the way. */
    {
        n = pack(sample[6], sizes[6], 6, 31, Z_DEFAULT_STRATEGY, 0);
        z_stream s, t;
        memset(&s, 0, sizeof s);
        inflateInit2(&s, 31);
        s.next_in = packed;
        s.avail_in = (uInt)(n / 2);
        s.next_out = out;
        s.avail_out = 50000;
        int ret = inflate(&s, Z_NO_FLUSH);
        printf("copy %d (%d)", inflateCopy(&t, &s), ret);
        s.avail_in += (uInt)(n - n / 2);
        s.avail_out = (uInt)out_cap - 50000;
        ret = inflate(&s, Z_NO_FLUSH);
        unsigned long first = crc32(0, out, (uInt)s.total_out);
        t.avail_in += (uInt)(n - n / 2);
        t.avail_out = (uInt)out_cap - 50000;
        int ret2 = inflate(&t, Z_NO_FLUSH);
        printf(" original %d %lu %08lx copy %d %lu %08lx\n", ret, s.total_out, first, ret2, t.total_out, crc32(0, out, (uInt)t.total_out));
        inflateEnd(&s);
        inflateEnd(&t);
    }
    /* Primed bits: a raw stream after bits deflatePrime put first. */
    {
        z_stream d;
        memset(&d, 0, sizeof d);
        deflateInit2(&d, 6, Z_DEFLATED, -15, 8, 0);
        deflatePrime(&d, 5, 0x15);
        d.next_in = sample[2];
        d.avail_in = 2000;
        d.next_out = packed;
        d.avail_out = sizeof packed;
        deflate(&d, Z_FINISH);
        n = d.total_out;
        deflateEnd(&d);
        z_stream s;
        memset(&s, 0, sizeof s);
        inflateInit2(&s, -15);
        printf("prime %d %d %d", inflatePrime(&s, 17, 0), inflatePrime(&s, -1, 0), inflatePrime(&s, 0, 0));
        /* Eat the first five bits: prime with the byte's top three. */
        printf(" %d", inflatePrime(&s, 3, packed[0] >> 5));
        s.next_in = packed + 1;
        s.avail_in = (uInt)n - 1;
        s.next_out = out;
        s.avail_out = (uInt)out_cap;
        int ret = inflate(&s, Z_FINISH);
        printf(" inflate %d out %lu crc %08lx\n", ret, s.total_out, crc32(0, out, (uInt)s.total_out));
        inflateEnd(&s);
    }
    /* Reset, reset with another window, validation off. */
    {
        n = pack(sample[2], 20000, 6, 15, Z_DEFAULT_STRATEGY, 0);
        packed[n - 2] ^= 0xff;
        z_stream s;
        memset(&s, 0, sizeof s);
        inflateInit(&s);
        for (int k = 0; k < 5; k++) {
            if (k == 1) printf("reset2 %d %d %d\n", inflateReset2(&s, 7), inflateReset2(&s, 16), inflateReset2(&s, -15 - 1));
            if (k == 2) printf("validate %d\n", inflateValidate(&s, 0));
            if (k == 3) printf("validate %d reset2 %d\n", inflateValidate(&s, 1), inflateReset2(&s, 31));
            if (k == 4) printf("reset2 %d undermine %d\n", inflateReset2(&s, 15), inflateUndermine(&s, 1));
            s.next_in = packed;
            s.avail_in = (uInt)n;
            s.next_out = out;
            s.avail_out = (uInt)out_cap;
            int ret = inflate(&s, Z_FINISH);
            printf("reset %d: %d %s out %lu adler %08lx\n", k, ret, s.msg ? s.msg : "-", s.total_out, s.adler);
            printf("  reset %d\n", inflateReset(&s));
        }
        inflateEnd(&s);
    }
    /* Errors of use. */
    {
        z_stream s;
        memset(&s, 0, sizeof s);
        printf("init %d %d %d %d\n", inflateInit_(&s, "2.0", sizeof s), inflateInit_(&s, ZLIB_VERSION, sizeof s + 1), inflateInit2(&s, 7), inflateInit2(&s, -16));
        printf("null %d %d %d %d %d\n", inflateInit(NULL), inflate(NULL, 0), inflateEnd(NULL), inflateReset(NULL), inflateSync(NULL));
        inflateInit(&s);
        s.next_in = NULL;
        s.avail_in = 5;
        s.next_out = out;
        s.avail_out = 10;
        printf("no input %d\n", inflate(&s, Z_NO_FLUSH));
        s.next_in = packed;
        s.next_out = NULL;
        printf("no output %d\n", inflate(&s, Z_NO_FLUSH));
        s.next_out = out;
        s.avail_in = 0;
        printf("nothing %d, end %d, end again %d\n", inflate(&s, Z_NO_FLUSH), inflateEnd(&s), inflateEnd(&s));
    }
    /* uncompress and uncompress2. */
    {
        uLongf len = (uLongf)out_cap;
        size_t made = pack(sample[3], sizes[3], 6, 15, Z_DEFAULT_STRATEGY, 0);
        int ret = uncompress(out, &len, packed, (uLong)made);
        printf("uncompress %d %lu crc %08lx\n", ret, len, crc32(0, out, (uInt)len));
        len = 1000;
        ret = uncompress(out, &len, packed, (uLong)made);
        printf("uncompress short %d %lu\n", ret, len);
        len = (uLongf)out_cap;
        ret = uncompress(out, &len, packed, (uLong)made / 2);
        printf("uncompress truncated %d %lu\n", ret, len);
        len = 0;
        ret = uncompress(out, &len, packed, (uLong)made);
        printf("uncompress nothing %d %lu\n", ret, len);
        uLong source = (uLong)made + 10;
        len = (uLongf)out_cap;
        ret = uncompress2(out, &len, packed, &source);
        printf("uncompress2 %d %lu used %lu\n", ret, len, source);
        len = (uLongf)out_cap;
        source = 4;
        unsigned char nodict[] = {0x78, 0xbb, 0x00, 0x00, 0x00, 0x01};
        ret = uncompress2(out, &len, nodict, &source);
        printf("uncompress2 dictionary %d %lu used %lu\n", ret, len, source);
        len = 0;
        unsigned char empty[16];
        uLongf elen = sizeof empty;
        compress(empty, &elen, sample[0], 0);
        ret = uncompress(out, &len, empty, elen);
        printf("uncompress empty %d %lu\n", ret, len);
    }
    /* inflateBack. */
    for (int k = 0; k < 4; k++) {
        size_t made = pack(sample[6], sizes[6], 6, -15, Z_DEFAULT_STRATEGY, 0);
        if (k == 2) packed[made / 3] ^= 0x40;
        z_stream s;
        memset(&s, 0, sizeof s);
        int ret = inflateBackInit(&s, 15, window);
        int ins = 0, outs = 0;
        back_in = packed;
        back_left = k == 3 ? made / 2 : made;
        back_piece = k == 1 ? 1000 : 1 << 20;
        back_crc = 0;
        back_total = 0;
        s.next_in = NULL;
        int got = inflateBack(&s, back_input, &ins, back_output, &outs);
        printf("back %d: init %d ret %d %s out %zu crc %08lx left %u calls %d %d\n", k, ret, got, s.msg ? s.msg : "-", back_total, back_crc, s.avail_in, ins > 0, outs);
        printf("  end %d\n", inflateBackEnd(&s));
    }
    {
        z_stream s;
        memset(&s, 0, sizeof s);
        printf("back init %d %d %d\n", inflateBackInit(&s, 7, window), inflateBackInit(&s, 15, NULL), inflateBackInit_(&s, 15, window, "2", sizeof s));
    }
    return 0;
}
