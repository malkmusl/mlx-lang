/* mlx-gcrypt's S-expressions: gcry_sexp_sscan and gcry_sexp_new on the
   advanced and canonical forms (tokens, quoted strings and their
   escapes, hex, base64, verbatim data, display hints) and their errors
   with offsets, gcry_sexp_build with every format WebCore passes (%s, %b,
   %d, %u, %m, %M, %S), gcry_sexp_sprint in each mode and with short
   buffers, find_token, length, nth, car, cdr, nth_data, nth_buffer,
   nth_string, nth_mpi in each format, extract_param (paths, formats,
   optional and missing parameters) and canon_len. It prints what
   libgcrypt 1.10 prints for the same program. */
#include <gcrypt.h>
#include <stdio.h>
#include <string.h>

static void status(const char *label, gcry_error_t error)
{
    printf("%s: %u %s\n", label, gcry_err_code(error), gcry_strerror(error));
}

static void show(const char *label, gcry_sexp_t sexp)
{
    if (!sexp) {
        printf("%s: NULL\n", label);
        return;
    }
    char buffer[2048];
    size_t length = gcry_sexp_sprint(sexp, GCRYSEXP_FMT_ADVANCED, buffer, sizeof buffer);
    printf("%s [%zu]: %s", label, length, buffer);
}

static void canonical(const char *label, gcry_sexp_t sexp)
{
    char buffer[2048];
    size_t length = gcry_sexp_sprint(sexp, GCRYSEXP_FMT_CANON, buffer, sizeof buffer);
    printf("%s [%zu]: ", label, length);
    for (size_t i = 0; i < length; i++)
        printf(buffer[i] >= 32 && buffer[i] < 127 ? "%c" : "\\x%02x", (unsigned char)buffer[i]);
    printf("\n");
}

static void mpi(const char *label, gcry_mpi_t a)
{
    if (!a) {
        printf("%s: NULL mpi\n", label);
        return;
    }
    if (gcry_mpi_get_flag(a, GCRYMPI_FLAG_OPAQUE)) {
        unsigned int bits;
        const unsigned char *data = gcry_mpi_get_opaque(a, &bits);
        printf("%s: opaque %u bits:", label, bits);
        for (unsigned int i = 0; i < (bits + 7) / 8; i++)
            printf("%02x", data[i]);
        printf("\n");
        return;
    }
    unsigned char *text = NULL;
    gcry_mpi_aprint(GCRYMPI_FMT_HEX, &text, NULL, a);
    printf("%s: %s\n", label, text);
    gcry_free(text);
}

static void scans(void)
{
    const char *texts[] = {
        "(public-key (rsa (n #00C0FFEE#) (e #010001#)))",
        "(a b c)",
        "(data (flags raw) (value \"hello world\"))",
        "(x \"esc\\n\\t\\\"\\\\\\x41\\101\\\nnext\")",
        "(base |aGVsbG8=| |YQ==|)",
        "(3:abc 1:x(2:yz))",
        "(token-with.special_chars:*+=/ 12345 -1)",
        "([text/plain] \"hint\")",
        "(a (b (c (d))))",
        "(odd #ABC#)",
        "(hex # 0 1 a b #)",
        "(a b",
        "(a b))",
        "(a 0:)",
        "(a @)",
        "(a &)",
        "(a {b})",
        "(a #XY#)",
        "(a \"bad \\q escape\")",
        "(a 10:abc)",
        "()",
        "",
        "abc",
        "(a [x [y]])",
        "(a ])",
        "  ( spaced\n\t list )  ",
        "(a \"\")",
        "(a ##)",
    };
    for (size_t i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        gcry_sexp_t sexp = (gcry_sexp_t)1;
        size_t offset = 999;
        gcry_error_t error = gcry_sexp_sscan(&sexp, &offset, texts[i], strlen(texts[i]));
        printf("scan %zu: ", i);
        if (error) {
            printf("offset %zu ", offset);
            status("error", error);
            printf("  sexp %s\n", sexp ? "set" : "NULL");
            continue;
        }
        show("ok", sexp);
        if (sexp)
            canonical("  canon", sexp);
        gcry_sexp_release(sexp);
    }
    gcry_sexp_t sexp;
    status("new canonical", gcry_sexp_new(&sexp, "(3:foo(3:bar1:x))", 0, 0));
    show("  ", sexp);
    gcry_sexp_release(sexp);
    status("new string", gcry_sexp_new(&sexp, "(foo bar)", 0, 1));
    show("  ", sexp);
    gcry_sexp_release(sexp);
    status("new with length", gcry_sexp_new(&sexp, "(foo bar)xyz", 9, 1));
    show("  ", sexp);
    gcry_sexp_release(sexp);
    status("new bad autodetect", gcry_sexp_new(&sexp, "(foo)", 5, 2));
    status("new not canonical", gcry_sexp_new(&sexp, "foo", 0, 0));
}

static void builds(void)
{
    unsigned char bytes[] = { 0x00, 0x80, 0xff, 0x41, 0x0a };
    gcry_mpi_t positive = NULL, negative = NULL, zero = gcry_mpi_new(0);
    gcry_mpi_scan(&positive, GCRYMPI_FMT_HEX, "80FF", 0, NULL);
    gcry_mpi_scan(&negative, GCRYMPI_FMT_HEX, "-1234", 0, NULL);
    gcry_sexp_t sexp = NULL, inner = NULL;
    size_t offset = 0;
    status("build %s %b", gcry_sexp_build(&sexp, &offset, "(data(flags oaep)(hash-algo %s)(label %b)(value %b))", "sha256", 0, "", (int)sizeof bytes, bytes));
    show("  ", sexp);
    canonical("  canon", sexp);
    gcry_sexp_release(sexp);
    status("build %d %u", gcry_sexp_build(&sexp, NULL, "(genkey(rsa(nbits %d)(rsa-use-e %d)(salt-length %u)(neg %d)))", 2048, 65537, 4000000000u, -5));
    show("  ", sexp);
    gcry_sexp_release(sexp);
    status("build %m %M", gcry_sexp_build(&sexp, NULL, "(key (a %m) (b %M) (c %m) (z %m) (zu %M))", positive, positive, negative, zero, zero));
    show("  ", sexp);
    canonical("  canon", sexp);
    gcry_sexp_release(sexp);
    status("build %M negative", gcry_sexp_build(&sexp, &offset, "(b %M)", negative));
    gcry_sexp_build(&inner, NULL, "(inner (x y))");
    status("build %S", gcry_sexp_build(&sexp, NULL, "(outer %S (after))", inner));
    show("  ", sexp);
    gcry_sexp_release(sexp);
    status("build bad spec", gcry_sexp_build(&sexp, &offset, "(a %x)", 1));
    printf("  offset %zu\n", offset);
    status("build unmatched", gcry_sexp_build(&sexp, &offset, "(a (b %s)", "x"));
    printf("  offset %zu\n", offset);
    status("build hex", gcry_sexp_build(&sexp, NULL, "(sig-val(ecdsa(r #0102#)(s %b)))", 3, "\x01\x02\x03"));
    show("  ", sexp);
    gcry_sexp_release(sexp);
    gcry_sexp_release(inner);
    gcry_mpi_release(positive);
    gcry_mpi_release(negative);
    gcry_mpi_release(zero);
}

static void prints(void)
{
    gcry_sexp_t sexp;
    gcry_sexp_new(&sexp, "(private-key (ecc (curve \"NIST P-256\") (q #04AABB#) (d #00FF#)) (x 1:\x01) (\"a b\" token \"\\x7f\"))", 0, 1);
    const int modes[] = { GCRYSEXP_FMT_DEFAULT, GCRYSEXP_FMT_CANON, GCRYSEXP_FMT_BASE64, GCRYSEXP_FMT_ADVANCED };
    for (int m = 0; m < 4; m++) {
        size_t needed = gcry_sexp_sprint(sexp, modes[m], NULL, 0);
        char buffer[1024];
        size_t length = gcry_sexp_sprint(sexp, modes[m], buffer, sizeof buffer);
        printf("mode %d: needed %zu, written %zu\n", modes[m], needed, length);
        for (size_t i = 0; i < length; i++)
            printf(buffer[i] >= 32 && buffer[i] < 127 ? "%c" : "\\x%02x", (unsigned char)buffer[i]);
        printf("\n");
        printf("  exact %zu, one short %zu\n", gcry_sexp_sprint(sexp, modes[m], buffer, needed), gcry_sexp_sprint(sexp, modes[m], buffer, needed - 1));
    }
    printf("null sexp: %zu\n", gcry_sexp_sprint(NULL, GCRYSEXP_FMT_ADVANCED, NULL, 0));
    gcry_sexp_release(sexp);
}

static void walks(void)
{
    gcry_sexp_t key;
    gcry_sexp_new(&key, "(key-data (public-key (rsa (n #00C0FFEE#) (e #010001#))) (private-key (rsa (n #00C0FFEE#) (e #010001#) (d #4321#) (p #0B#) (q #0D#))) atom (x \"s\" #FF80#))", 0, 1);
    printf("length %d\n", gcry_sexp_length(key));
    for (int i = 0; i < 6; i++) {
        char label[32];
        snprintf(label, sizeof label, "nth %d", i);
        gcry_sexp_t nth = gcry_sexp_nth(key, i);
        show(label, nth);
        size_t length = 77;
        const char *data = gcry_sexp_nth_data(key, i, &length);
        printf("  data %s %zu\n", data ? "set" : "NULL", length);
        gcry_sexp_release(nth);
    }
    gcry_sexp_t car = gcry_sexp_car(key);
    show("car", car);
    gcry_sexp_release(car);
    gcry_sexp_t cdr = gcry_sexp_cdr(key);
    show("cdr", cdr);
    gcry_sexp_release(cdr);
    gcry_sexp_t pub = gcry_sexp_find_token(key, "public-key", 0);
    show("find public-key", pub);
    gcry_sexp_t n = gcry_sexp_find_token(key, "n", 0);
    show("find n", n);
    gcry_sexp_t d = gcry_sexp_find_token(key, "dxyz", 1);
    show("find d by length", d);
    show("find missing", gcry_sexp_find_token(key, "missing", 0));
    show("find in NULL", gcry_sexp_find_token(NULL, "n", 0));
    mpi("n usg", gcry_sexp_nth_mpi(n, 1, GCRYMPI_FMT_USG));
    mpi("n std", gcry_sexp_nth_mpi(n, 1, GCRYMPI_FMT_STD));
    mpi("n default", gcry_sexp_nth_mpi(n, 1, 0));
    mpi("n opaque", gcry_sexp_nth_mpi(n, 1, GCRYMPI_FMT_OPAQUE));
    mpi("n hex", gcry_sexp_nth_mpi(n, 1, GCRYMPI_FMT_HEX));
    mpi("n token", gcry_sexp_nth_mpi(n, 0, GCRYMPI_FMT_USG));
    mpi("n past", gcry_sexp_nth_mpi(n, 2, GCRYMPI_FMT_USG));
    gcry_sexp_t x = gcry_sexp_find_token(key, "x", 0);
    char *string = gcry_sexp_nth_string(x, 1);
    printf("string %s\n", string ? string : "NULL");
    gcry_free(string);
    size_t length = 0;
    unsigned char *buffer = gcry_sexp_nth_buffer(x, 2, &length);
    printf("buffer %zu %02x%02x\n", length, buffer ? buffer[0] : 0, buffer ? buffer[1] : 0);
    gcry_free(buffer);
    mpi("x std", gcry_sexp_nth_mpi(x, 2, GCRYMPI_FMT_STD));
    string = gcry_sexp_nth_string(key, 3);
    printf("atom string %s\n", string ? string : "NULL");
    gcry_free(string);
    gcry_sexp_t atom;
    gcry_sexp_new(&atom, "(single)", 0, 1);
    printf("single length %d\n", gcry_sexp_length(atom));
    show("single cdr", gcry_sexp_cdr(atom));
    show("single nth 1", gcry_sexp_nth(atom, 1));
    gcry_sexp_release(atom);
    gcry_sexp_release(x);
    gcry_sexp_release(pub);
    gcry_sexp_release(n);
    gcry_sexp_release(d);
    gcry_sexp_release(key);
}

static void extracts(void)
{
    gcry_sexp_t key;
    gcry_sexp_new(&key, "(private-key (ecc (curve Ed25519) (flags eddsa) (q #40ABCD#) (d #0102#) (count \"42\") (name \"label\") (neg #FF00#)))", 0, 1);
    gcry_mpi_t q = NULL, d = NULL, neg = NULL, opaque = NULL;
    status("extract qd", gcry_sexp_extract_param(key, "private-key", "qd", &q, &d, NULL));
    mpi("  q", q);
    mpi("  d", d);
    gcry_mpi_release(q);
    gcry_mpi_release(d);
    q = d = NULL;
    status("extract path ecc", gcry_sexp_extract_param(key, "private-key!ecc", "q", &q, NULL));
    mpi("  q", q);
    gcry_mpi_release(q);
    q = NULL;
    status("extract no path", gcry_sexp_extract_param(key, NULL, "q-'neg'/d", &q, &neg, &opaque, NULL));
    mpi("  q", q);
    mpi("  neg std", neg);
    mpi("  d opaque", opaque);
    gcry_mpi_release(q);
    gcry_mpi_release(neg);
    gcry_mpi_release(opaque);
    q = neg = opaque = NULL;
    unsigned int count = 0;
    char *name = NULL;
    char *flags = NULL;
    status("extract %u %s %#s", gcry_sexp_extract_param(key, NULL, "%u'count'%s'name'%#s'flags'", &count, &name, &flags, NULL));
    printf("  count %u name %s flags %s\n", count, name ? name : "NULL", flags ? flags : "NULL");
    gcry_free(name);
    gcry_free(flags);
    status("extract optional", gcry_sexp_extract_param(key, NULL, "qz?", &q, &d, NULL));
    printf("  z %s\n", d ? "set" : "NULL");
    mpi("  q", q);
    gcry_mpi_release(q);
    q = NULL;
    status("extract missing", gcry_sexp_extract_param(key, NULL, "qz", &q, &d, NULL));
    printf("  q after failure %s\n", q ? "set" : "NULL");
    status("extract bad path", gcry_sexp_extract_param(key, "public-key", "q", &q, NULL));
    status("extract null pointer", gcry_sexp_extract_param(key, NULL, "qd", &q, NULL, NULL));
    status("extract too many", gcry_sexp_extract_param(key, NULL, "q", &q, &d, NULL));
    status("extract bad quote", gcry_sexp_extract_param(key, NULL, "'q", &q, NULL));
    gcry_buffer_t buffer = { 0, 0, 0, NULL };
    status("extract buffer", gcry_sexp_extract_param(key, NULL, "&q", &buffer, NULL));
    printf("  size %zu off %zu len %zu first %02x\n", buffer.size, buffer.off, buffer.len, buffer.data ? ((unsigned char *)buffer.data)[0] : 0);
    gcry_free(buffer.data);
    gcry_sexp_release(key);
}

static void lengths(void)
{
    const char *texts[] = { "(3:abc)", "(3:abc(1:x))trailing", "3:abc", "(3:abc", "(03:abc)", "(a)", "(3:abc))", "(1:a[1:b]1:c)", "((1:a)", "()" };
    for (int i = 0; i < 10; i++) {
        size_t offset = 77;
        gcry_error_t code = 77;
        size_t length = gcry_sexp_canon_len((const unsigned char *)texts[i], 0, &offset, &code);
        printf("canon_len %d: %zu offset %zu code %u\n", i, length, offset, code);
        length = gcry_sexp_canon_len((const unsigned char *)texts[i], 4, &offset, &code);
        printf("  limited: %zu offset %zu code %u\n", length, offset, code);
    }
}

int main(void)
{
    if (!gcry_check_version(GCRYPT_VERSION))
        return 1;
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
    scans();
    builds();
    prints();
    walks();
    extracts();
    lengths();
    return 0;
}
