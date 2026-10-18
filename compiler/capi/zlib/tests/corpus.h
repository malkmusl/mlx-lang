/* The data the zlib tests compress: made by the program itself, the same
   on every run (a linear congruential generator), from nothing to 200 KB:
   words, binary records, random bytes, zeros, and a mix with repeats
   farther back than a 32 KB window reaches. */
#ifndef CORPUS_H
#define CORPUS_H

#include <stdlib.h>
#include <string.h>

#define CORPUS_COUNT 7

static unsigned long corpus_seed;

static unsigned corpus_next(void) {
    corpus_seed = corpus_seed * 6364136223846793005UL + 1442695040888963407UL;
    return (unsigned)(corpus_seed >> 33);
}

static const char *corpus_words[] = {
    "the", "of", "and", "to", "in", "a", "is", "that", "for", "it", "as", "was", "with", "be", "by",
    "on", "not", "he", "this", "are", "or", "his", "from", "at", "which", "but", "have", "an", "had",
    "they", "you", "were", "their", "one", "all", "we", "can", "her", "has", "there", "been", "if",
    "more", "when", "will", "would", "who", "so", "no", "deflate", "inflate", "window", "stream",
    "Huffman", "compression", "dictionary", "length", "distance", "literal", "block"
};

static void corpus_text(unsigned char *out, size_t size) {
    size_t at = 0;
    unsigned column = 0;
    while (at < size) {
        const char *word = corpus_words[corpus_next() % (sizeof corpus_words / sizeof *corpus_words)];
        size_t n = strlen(word);
        for (size_t i = 0; i < n && at < size; i++) out[at++] = (unsigned char)word[i];
        column += (unsigned)n + 1;
        if (at < size) out[at++] = column > 70 ? '\n' : ' ';
        if (column > 70) column = 0;
    }
}

static void corpus_records(unsigned char *out, size_t size) {
    unsigned id = 1000;
    for (size_t at = 0; at < size; at++) {
        unsigned field = (unsigned)(at % 24);
        if (field == 0) id += corpus_next() % 7;
        out[at] = field < 4 ? (unsigned char)(id >> (8 * field)) : field < 12 ? (unsigned char)(field * 17) : (unsigned char)(corpus_next() % (field < 18 ? 4 : 256));
    }
}

static void corpus_random(unsigned char *out, size_t size) {
    for (size_t at = 0; at < size; at++) out[at] = (unsigned char)(corpus_next() >> 7);
}

/* The sample INDEX: its bytes (from malloc) and its size. */
static unsigned char *corpus_sample(int index, size_t *size) {
    static const size_t sizes[CORPUS_COUNT] = {0, 5, 100000, 70000, 40000, 150000, 200000};
    *size = sizes[index];
    unsigned char *out = malloc(*size + 1);
    corpus_seed = 12345 + (unsigned long)index;
    switch (index) {
    case 1: memcpy(out, "hello", 5); break;
    case 2: corpus_text(out, *size); break;
    case 3: corpus_records(out, *size); break;
    case 4: corpus_random(out, *size); break;
    case 5: memset(out, 0, *size); break;
    case 6:
        corpus_text(out, 50000);
        corpus_random(out + 50000, 20000);
        memset(out + 70000, 'z', 30000);
        memcpy(out + 100000, out + 10000, 40000);
        corpus_records(out + 140000, 30000);
        memcpy(out + 170000, out + 120000, 30000);
        break;
    }
    return out;
}

#endif
