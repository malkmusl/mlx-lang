/* The gzip file functions: files written with gzwrite, gzputs, gzputc,
   gzprintf, gzfwrite, flushes, forward seeks, changed parameters,
   appended members and transparent writing; read back with gzread,
   gzgets, gzgetc, gzungetc, gzfread, seeks both ways, gztell, gzoffset,
   gzeof, gzdirect, gzrewind; files that are not gzip, damaged and
   truncated ones, and the errors with their messages. The files are made
   in the current directory. (Each call is a statement of its own: the
   order C evaluates a call's arguments in is not fixed.) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>
#include "corpus.h"

#define SAY(label, call) do { long value_ = (long)(call); printf("%s %ld\n", label, value_); } while (0)

static unsigned long file_crc(const char *path, long *size) {
    FILE *f = fopen(path, "rb");
    *size = 0;
    if (!f) return 0;
    unsigned char buf[4096];
    unsigned long crc = 0;
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        crc = crc32(crc, buf, (uInt)n);
        *size += (long)n;
    }
    fclose(f);
    return crc;
}

static void show_file(const char *path) {
    long size;
    unsigned long crc = file_crc(path, &size);
    printf("  file %s: %ld bytes crc %08lx\n", path, size, crc);
}

static void show_error(const char *label, gzFile file) {
    int err = 0;
    const char *msg = gzerror(file, &err);
    printf("  %s: err %d \"%s\"\n", label, err, msg ? msg : "(null)");
}

/* Reads the whole file in pieces of PIECE bytes. */
static void read_all(const char *path, unsigned piece) {
    gzFile file = gzopen(path, "rb");
    if (!file) {
        printf("read %s: cannot open\n", path);
        return;
    }
    unsigned char *buf = malloc(piece);
    unsigned long crc = 0;
    long total = 0;
    int got;
    while ((got = gzread(file, buf, piece)) > 0) {
        crc = crc32(crc, buf, (uInt)got);
        total += got;
    }
    printf("read %s by %u: last %d total %ld crc %08lx\n", path, piece, got, total, crc);
    SAY("  eof", gzeof(file));
    SAY("  direct", gzdirect(file));
    SAY("  tell", gztell(file));
    show_error("error", file);
    SAY("  close", gzclose(file));
    free(buf);
}

static void copy_file(const char *from, const char *to) {
    FILE *f = fopen(from, "rb");
    FILE *g = fopen(to, "wb");
    int ch;
    while ((ch = fgetc(f)) != EOF) fputc(ch, g);
    fclose(f);
    fclose(g);
}

int main(void) {
    size_t text_size, mix_size;
    unsigned char *text = corpus_sample(2, &text_size);
    unsigned char *mix = corpus_sample(6, &mix_size);
    char line[64];
    char buf[100];

    /* Writing, every way. */
    gzFile file = gzopen("one.gz", "wb");
    SAY("open", file != NULL);
    SAY("buffer", gzbuffer(file, 4096));
    SAY("write", gzwrite(file, text, 30000));
    SAY("puts", gzputs(file, "a line of text\n"));
    SAY("putc", gzputc(file, 'x'));
    SAY("putc", gzputc(file, 0x1ff));
    SAY("printf", gzprintf(file, "%s %d %5.2f %c %lu %x\n", "formatted", -42, 3.14159, 'q', 123456789012UL, 0xbeef));
    SAY("printf many", gzprintf(file, "%d %d %d %d %d %d %d %d %d %d %s %f %f %f %f %f %f %f %f %f\n", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, "eleven",
                                1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5));
    SAY("fwrite", gzfwrite(text + 30000, 100, 50, file));
    SAY("flush", gzflush(file, Z_SYNC_FLUSH));
    SAY("tell", gztell(file));
    SAY("setparams", gzsetparams(file, 9, Z_FILTERED));
    SAY("setparams same", gzsetparams(file, 9, Z_FILTERED));
    SAY("write", gzwrite(file, mix, (unsigned)mix_size));
    SAY("seek", gzseek(file, 1000, SEEK_CUR));
    SAY("tell", gztell(file));
    SAY("seek back", gzseek(file, 10, SEEK_SET));
    SAY("write", gzwrite(file, "after the gap", 13));
    SAY("full flush", gzflush(file, Z_FULL_FLUSH));
    SAY("setparams", gzsetparams(file, 0, Z_DEFAULT_STRATEGY));
    SAY("write", gzwrite(file, text, 20000));
    SAY("finish", gzflush(file, Z_FINISH));
    SAY("write after finish", gzwrite(file, "second member", 13));
    SAY("tell", gztell(file));
    SAY("eof while writing", gzeof(file));
    SAY("direct", gzdirect(file));
    SAY("read while writing", gzread(file, text + 90000, 10));
    show_error("error", file);
    SAY("close", gzclose(file));
    show_file("one.gz");

    /* Reading it back. */
    unsigned pieces[] = {1, 100, 8192, 1 << 20};
    for (int k = 0; k < 4; k++) read_all("one.gz", pieces[k]);
    file = gzopen("one.gz", "r");
    for (int k = 0; k < 3; k++) {
        char *got = gzgets(file, line, sizeof line);
        printf("gets %d \"%s\"\n", got == line, got ? line : "");
    }
    SAY("getc", gzgetc(file));
    SAY("getc", gzgetc(file));
    SAY("tell", gztell(file));
    SAY("ungetc", gzungetc('A', file));
    SAY("ungetc", gzungetc('B', file));
    SAY("getc", gzgetc(file));
    SAY("getc", gzgetc(file));
    SAY("ungetc -1", gzungetc(-1, file));
    SAY("seek", gzseek(file, 30000, SEEK_SET));
    SAY("tell", gztell(file));
    printf("gets %s", gzgets(file, line, sizeof line));
    SAY("seek back", gzseek(file, 5, SEEK_SET));
    SAY("getc", gzgetc(file));
    SAY("seek end", gzseek(file, 0, SEEK_END));
    SAY("seek forward", gzseek(file, 100, SEEK_CUR));
    SAY("read", gzread(file, line, 10));
    SAY("offset", gzoffset(file) > 0);
    static unsigned char big[300000];
    SAY("fread", gzfread(big, 1000, 400, file));
    SAY("eof", gzeof(file));
    SAY("rewind", gzrewind(file));
    SAY("tell", gztell(file));
    SAY("eof", gzeof(file));
    SAY("getc", gzgetc(file));
    gzclearerr(file);
    SAY("write while reading", gzwrite(file, "x", 1));
    SAY("puts while reading", gzputs(file, "x"));
    SAY("flush while reading", gzflush(file, Z_FINISH));
    SAY("close_w on read", gzclose_w(file));
    SAY("close_r", gzclose_r(file));

    /* Appending, levels and strategies in the mode. */
    const char *modes[] = {"ab", "a9", "w1h", "w0", "wR", "wF", "wf6"};
    for (int k = 0; k < 7; k++) {
        char name[32];
        snprintf(name, sizeof name, "mode%d.gz", k);
        if (k == 0) copy_file("one.gz", name);
        file = gzopen(name, modes[k]);
        printf("mode %s\n", modes[k]);
        SAY("  write", gzwrite(file, mix, 50000));
        SAY("  close", gzclose(file));
        show_file(name);
        read_all(name, 5000);
    }
    /* Transparent writing; files that are not gzip. */
    file = gzopen("plain.txt", "wT");
    SAY("transparent direct", gzdirect(file));
    SAY("write", gzwrite(file, text, 10000));
    SAY("printf", gzprintf(file, "%d", 12345));
    SAY("close", gzclose(file));
    show_file("plain.txt");
    read_all("plain.txt", 777);
    file = gzopen("plain.txt", "r");
    SAY("plain direct", gzdirect(file));
    SAY("seek", gzseek(file, 5000, SEEK_SET));
    SAY("getc", gzgetc(file));
    SAY("seek", gzseek(file, -10, SEEK_CUR));
    SAY("tell", gztell(file));
    SAY("close", gzclose(file));
    file = gzopen("t.gz", "w");
    gzwrite(file, text, 3000);
    gzclose(file);
    copy_file("t.gz", "trail.gz");
    FILE *f = fopen("trail.gz", "ab");
    fputs("trailing garbage", f);
    fclose(f);
    read_all("trail.gz", 1000);
    f = fopen("empty.gz", "wb");
    fclose(f);
    read_all("empty.gz", 1000);
    f = fopen("one-byte.gz", "wb");
    fputc(0x1f, f);
    fclose(f);
    read_all("one-byte.gz", 1000);
    /* Damaged and truncated. */
    file = gzopen("damaged.gz", "wb");
    gzwrite(file, text, 50000);
    gzclose(file);
    f = fopen("damaged.gz", "r+b");
    fseek(f, 2000, SEEK_SET);
    fputc(0x55, f);
    fputc(0xaa, f);
    fclose(f);
    read_all("damaged.gz", 4096);
    file = gzopen("cut.gz", "wb");
    gzwrite(file, text, 50000);
    gzclose(file);
    SAY("truncate", truncate("cut.gz", 5000));
    read_all("cut.gz", 4096);
    file = gzopen("cut.gz", "rb");
    while (gzgets(file, buf, sizeof buf)) {}
    show_error("gets to the end", file);
    SAY("  eof", gzeof(file));
    SAY("  close", gzclose(file));
    SAY("truncate", truncate("cut.gz", 3));
    read_all("cut.gz", 4096);
    /* By descriptor; errors. */
    int fd = open("one.gz", O_RDONLY);
    file = gzdopen(fd, "rb");
    SAY("dopen", file != NULL);
    SAY("read", gzread(file, buf, 50));
    show_error("dopen", file);
    SAY("close", gzclose(file));
    SAY("mode r+", gzopen("x.gz", "r+") == NULL);
    SAY("mode b", gzopen("x.gz", "b") == NULL);
    SAY("mode rT", gzopen("x.gz", "rT") == NULL);
    SAY("missing", gzopen("no/such/dir/file.gz", "rb") == NULL);
    SAY("dopen -1", gzdopen(-1, "rb") == NULL);
    SAY("null read", gzread(NULL, buf, 1));
    SAY("null write", gzwrite(NULL, buf, 1));
    SAY("null close", gzclose(NULL));
    SAY("null eof", gzeof(NULL));
    SAY("null flush", gzflush(NULL, 0));
    SAY("null seek", gzseek(NULL, 0, 0));
    SAY("null gets", gzgets(NULL, buf, 10) == NULL);
    file = gzopen("one.gz", "rb");
    SAY("bad whence", gzseek(file, 0, SEEK_END + 5));
    SAY("before start", gzseek(file, -5, SEEK_SET));
    SAY("getc", gzgetc(file));
    SAY("buffer after read", gzbuffer(file, 1000));
    SAY("gets 1", gzgets(file, buf, 1) == NULL);
    SAY("close", gzclose(file));
    file = gzopen("w.gz", "wb");
    SAY("seek back while writing", gzseek(file, -1, SEEK_CUR));
    SAY("flush bad", gzflush(file, 7));
    SAY("close", gzclose(file));
    show_file("w.gz");
    return 0;
}
