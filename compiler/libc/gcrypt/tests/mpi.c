/* mlx-gcrypt's multi-precision integers: gcry_mpi_scan and print in
   every format (USG, STD, PGP, SSH, HEX, signs and zero, buffers too
   short), aprint, arithmetic with signs (add, sub, mul, div and mod in
   both roundings, the modular ones, powm with odd and even moduli,
   gcd, invm), bits and shifts, comparisons, the constants, opaque
   values and get_ui. It prints what libgcrypt 1.10 prints for the same
   program. */
#include <gcrypt.h>
#include <stdio.h>
#include <string.h>

static void status(const char *label, gcry_error_t error)
{
    printf("%s: %u %s\n", label, gcry_err_code(error), gcry_strerror(error));
}

static void show(const char *label, gcry_mpi_t a)
{
    unsigned char *text = NULL;
    gcry_error_t error = gcry_mpi_aprint(GCRYMPI_FMT_HEX, &text, NULL, a);
    if (error) {
        status(label, error);
        return;
    }
    printf("%s = %s (%u bits, neg %d)\n", label, (char *)text, gcry_mpi_get_nbits(a), gcry_mpi_is_neg(a));
    gcry_free(text);
}

static gcry_mpi_t number(const char *hex)
{
    gcry_mpi_t a = NULL;
    gcry_error_t error = gcry_mpi_scan(&a, GCRYMPI_FMT_HEX, hex, 0, NULL);
    if (error)
        status(hex, error);
    return a;
}

static void bytes(const char *label, const unsigned char *data, size_t length)
{
    printf("%s [%zu]:", label, length);
    for (size_t i = 0; i < length; i++)
        printf("%02x", data[i]);
    printf("\n");
}

static const char *values[] = {
    "0", "1", "-1", "7F", "80", "-80", "-81", "FF", "100", "-100",
    "123456789ABCDEF0123456789ABCDEF", "-FEDCBA9876543210FEDCBA9876543210FF",
    "8000000000000000", "10000000000000000",
};

static void formats(void)
{
    const int formats[] = { GCRYMPI_FMT_USG, GCRYMPI_FMT_STD, GCRYMPI_FMT_PGP, GCRYMPI_FMT_SSH, GCRYMPI_FMT_HEX };
    const char *names[] = { "usg", "std", "pgp", "ssh", "hex" };
    for (size_t v = 0; v < sizeof values / sizeof values[0]; v++) {
        gcry_mpi_t a = number(values[v]);
        show(values[v], a);
        for (int f = 0; f < 5; f++) {
            unsigned char buffer[64];
            size_t written = 12345;
            char label[64];
            gcry_error_t error = gcry_mpi_print(formats[f], NULL, 0, &written, a);
            snprintf(label, sizeof label, "  %s size", names[f]);
            if (error) {
                status(label, error);
                continue;
            }
            printf("%s %zu\n", label, written);
            error = gcry_mpi_print(formats[f], buffer, sizeof buffer, &written, a);
            snprintf(label, sizeof label, "  %s", names[f]);
            if (error) {
                status(label, error);
                continue;
            }
            bytes(label, buffer, written);
            if (written > 0) {
                error = gcry_mpi_print(formats[f], buffer, written - 1, &written, a);
                status("  one short", error);
            }
            gcry_mpi_t back = NULL;
            size_t scanned = 0;
            error = gcry_mpi_scan(&back, formats[f], buffer, formats[f] == GCRYMPI_FMT_HEX ? 0 : written, &scanned);
            if (error) {
                status("  back", error);
                continue;
            }
            printf("  back %zu cmp %d\n", scanned, gcry_mpi_cmp(a, back));
            gcry_mpi_release(back);
        }
        gcry_mpi_release(a);
    }
    const unsigned char raw[] = { 0, 0, 0x80, 1, 0xff };
    /* (Not SSH: it takes its length from the data, past the buffer.) */
    for (int f = 1; f <= 5; f++) {
        if (f == GCRYMPI_FMT_SSH)
            continue;
        gcry_mpi_t a = NULL;
        size_t scanned = 99;
        gcry_error_t error = gcry_mpi_scan(&a, f, raw, sizeof raw, &scanned);
        char label[32];
        snprintf(label, sizeof label, "scan raw %d", f);
        if (error) {
            status(label, error);
            continue;
        }
        printf("%s scanned %zu\n", label, scanned);
        show(label, a);
        gcry_mpi_release(a);
    }
    const char *texts[] = { "0x10", "-0", "abc", "12 34", "", "-", "g1", "0000FF" };
    for (int t = 0; t < 8; t++) {
        gcry_mpi_t a = NULL;
        gcry_error_t error = gcry_mpi_scan(&a, GCRYMPI_FMT_HEX, texts[t], 0, NULL);
        printf("hex \"%s\": ", texts[t]);
        if (error)
            status("error", error);
        else
            show("value", a);
        gcry_mpi_release(a);
    }
    gcry_mpi_t a = NULL;
    status("hex with a length", gcry_mpi_scan(&a, GCRYMPI_FMT_HEX, "12", 2, NULL));
    status("pgp short", gcry_mpi_scan(&a, GCRYMPI_FMT_PGP, "\x00\x10\x01", 3, NULL));
    unsigned char *out = NULL;
    size_t length = 0;
    a = number("-1234");
    status("aprint usg of negative", gcry_mpi_aprint(GCRYMPI_FMT_USG, &out, &length, a));
    if (out) bytes("  ", out, length);
    gcry_free(out);
    out = NULL;
    status("aprint pgp of negative", gcry_mpi_aprint(GCRYMPI_FMT_PGP, &out, &length, a));
    gcry_free(out);
    gcry_mpi_release(a);
    a = gcry_mpi_new(0);
    out = NULL;
    status("aprint usg of zero", gcry_mpi_aprint(GCRYMPI_FMT_USG, &out, &length, a));
    printf("  length %zu, buffer %s\n", length, out ? "set" : "null");
    gcry_free(out);
    gcry_mpi_release(a);
}

static void arithmetic(void)
{
    const char *pairs[][2] = {
        { "123456789ABCDEF0123456789", "FEDCBA987654321" },
        { "-123456789ABCDEF0123456789", "FEDCBA987654321" },
        { "123456789ABCDEF0123456789", "-FEDCBA987654321" },
        { "-123456789ABCDEF0123456789", "-FEDCBA987654321" },
        { "5", "7" },
        { "-5", "7" },
        { "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", "FFFFFFFFFFFFFFFF0000000000000001" },
        { "100000000000000000000000000000000", "FFFFFFFFFFFFFFFF" },
        { "D2A8F67BD9E0F7A5C2B1E4D3C6F5A8B7E0D9C2F1A4B3E6D5C8F7A0B9E2D1C4F3A6B5E8D7CAF9A2B1E4D3C6F5A8B7E0D9C2F1A4B3E6D5C8F7A0B9E2D1C4F3A6B5E8D7", "8000000000000000000000000000000000000001" },
        { "0", "3" },
        { "C", "-4" },
    };
    for (size_t p = 0; p < sizeof pairs / sizeof pairs[0]; p++) {
        gcry_mpi_t u = number(pairs[p][0]);
        gcry_mpi_t v = number(pairs[p][1]);
        gcry_mpi_t w = gcry_mpi_new(0);
        gcry_mpi_t r = gcry_mpi_new(0);
        printf("pair %zu: cmp %d\n", p, gcry_mpi_cmp(u, v));
        gcry_mpi_add(w, u, v);
        show("  add", w);
        gcry_mpi_sub(w, u, v);
        show("  sub", w);
        gcry_mpi_mul(w, u, v);
        show("  mul", w);
        gcry_mpi_div(w, r, u, v, 0);
        show("  div q", w);
        show("  div r", r);
        gcry_mpi_div(w, r, u, v, -1);
        show("  floor q", w);
        show("  floor r", r);
        gcry_mpi_div(NULL, r, u, v, -1);
        show("  floor r only", r);
        gcry_mpi_mod(r, u, v);
        show("  mod", r);
        if (gcry_mpi_cmp_ui(v, 0) > 0) {
            gcry_mpi_t m = gcry_mpi_new(0);
            gcry_mpi_add_ui(m, v, 2);
            gcry_mpi_addm(w, u, v, m);
            show("  addm", w);
            gcry_mpi_subm(w, u, v, m);
            show("  subm", w);
            gcry_mpi_mulm(w, u, v, m);
            show("  mulm", w);
            gcry_mpi_t g = gcry_mpi_new(0);
            int one = gcry_mpi_gcd(g, u, v);
            printf("  gcd one %d\n", one);
            show("  gcd", g);
            int invertible = gcry_mpi_invm(g, u, v);
            printf("  invm %d\n", invertible);
            if (invertible)
                show("  inverse", g);
            gcry_mpi_release(g);
            gcry_mpi_release(m);
        }
        gcry_mpi_add(u, u, u);
        show("  u + u in place", u);
        gcry_mpi_mul(v, v, v);
        show("  v * v in place", v);
        gcry_mpi_release(u);
        gcry_mpi_release(v);
        gcry_mpi_release(w);
        gcry_mpi_release(r);
    }
    /* Small operands. */
    gcry_mpi_t a = number("-10");
    gcry_mpi_t w = gcry_mpi_new(0);
    gcry_mpi_add_ui(w, a, 5);
    show("-16 + 5", w);
    gcry_mpi_add_ui(w, a, 100);
    show("-16 + 100", w);
    gcry_mpi_sub_ui(w, a, 5);
    show("-16 - 5", w);
    gcry_mpi_mul_ui(w, a, 1000);
    show("-16 * 1000", w);
    gcry_mpi_mul_2exp(w, a, 70);
    show("-16 << 70", w);
    printf("cmp_ui -16 5: %d, 5 5: %d\n", gcry_mpi_cmp_ui(a, 5), gcry_mpi_cmp_ui(gcry_mpi_set_ui(w, 5), 5));
    gcry_mpi_neg(w, a);
    show("neg", w);
    gcry_mpi_abs(a);
    show("abs", a);
    gcry_mpi_release(a);
    gcry_mpi_release(w);
}

static void powers(void)
{
    const char *cases[][3] = {
        { "4", "D", "1F1" },
        { "123456789ABCDEF", "10001", "C0FFEE1234567890ABCDEF1234567891" },
        { "123456789ABCDEF", "10001", "C0FFEE1234567890ABCDEF1234567890" },
        { "2", "0", "7" },
        { "-3", "3", "B" },
        { "5", "3", "1" },
        { "AB", "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF000000000000000000000001" },
    };
    for (size_t c = 0; c < sizeof cases / sizeof cases[0]; c++) {
        gcry_mpi_t b = number(cases[c][0]);
        gcry_mpi_t e = number(cases[c][1]);
        gcry_mpi_t m = number(cases[c][2]);
        gcry_mpi_t w = gcry_mpi_new(0);
        gcry_mpi_powm(w, b, e, m);
        show("powm", w);
        gcry_mpi_release(b);
        gcry_mpi_release(e);
        gcry_mpi_release(m);
        gcry_mpi_release(w);
    }
    gcry_mpi_t a = number("3");
    gcry_mpi_t m = number("6");
    gcry_mpi_t x = gcry_mpi_new(0);
    printf("invm 3 mod 6: %d\n", gcry_mpi_invm(x, a, m));
    gcry_mpi_release(a);
    gcry_mpi_release(m);
    gcry_mpi_release(x);
}

static void bits(void)
{
    gcry_mpi_t a = number("F0F0");
    gcry_mpi_set_bit(a, 100);
    show("set 100", a);
    printf("test 100 %d, 99 %d, 4 %d, 1000 %d\n", gcry_mpi_test_bit(a, 100), gcry_mpi_test_bit(a, 99), gcry_mpi_test_bit(a, 4), gcry_mpi_test_bit(a, 1000));
    gcry_mpi_clear_bit(a, 100);
    show("clear 100", a);
    gcry_mpi_clear_bit(a, 5000);
    show("clear 5000", a);
    gcry_mpi_set_highbit(a, 6);
    show("set high 6", a);
    gcry_mpi_set_bit(a, 70);
    gcry_mpi_clear_highbit(a, 70);
    show("clear high 70", a);
    gcry_mpi_t w = gcry_mpi_new(0);
    gcry_mpi_t b = number("123456789ABCDEF0123456789ABCDEF");
    gcry_mpi_rshift(w, b, 4);
    show("rshift 4", w);
    gcry_mpi_rshift(w, b, 67);
    show("rshift 67", w);
    gcry_mpi_rshift(w, b, 500);
    show("rshift 500", w);
    gcry_mpi_lshift(w, b, 3);
    show("lshift 3", w);
    gcry_mpi_lshift(w, b, 129);
    show("lshift 129", w);
    gcry_mpi_rshift(b, b, 8);
    show("rshift in place", b);
    gcry_mpi_t n = number("-123456789ABCDEF");
    gcry_mpi_rshift(w, n, 8);
    show("rshift negative", w);
    gcry_mpi_lshift(w, n, 8);
    show("lshift negative", w);
    gcry_mpi_release(n);
    gcry_mpi_release(a);
    gcry_mpi_release(b);
    gcry_mpi_release(w);
}

static void misc(void)
{
    show("const one", GCRYMPI_CONST_ONE);
    show("const two", GCRYMPI_CONST_TWO);
    show("const three", GCRYMPI_CONST_THREE);
    show("const four", GCRYMPI_CONST_FOUR);
    show("const eight", GCRYMPI_CONST_EIGHT);
    gcry_mpi_t a = gcry_mpi_set_ui(NULL, 4000000000u);
    show("set_ui", a);
    unsigned int value = 0;
    status("get_ui", gcry_mpi_get_ui(&value, a));
    printf("  %u\n", value);
    gcry_mpi_t big = number("100000000");
    status("get_ui too big", gcry_mpi_get_ui(&value, big));
    gcry_mpi_t copy = gcry_mpi_copy(big);
    show("copy", copy);
    gcry_mpi_swap(a, copy);
    show("swapped a", a);
    show("swapped copy", copy);
    gcry_mpi_t set = gcry_mpi_set(NULL, a);
    show("set new", set);
    gcry_mpi_snatch(set, copy);
    show("snatched", set);
    gcry_mpi_release(set);
    gcry_mpi_release(a);
    gcry_mpi_release(big);

    unsigned char data[5] = { 1, 2, 3, 4, 5 };
    gcry_mpi_t opaque = gcry_mpi_set_opaque_copy(NULL, data, 36);
    unsigned int nbits = 0;
    const unsigned char *back = gcry_mpi_get_opaque(opaque, &nbits);
    printf("opaque %u bits: %02x%02x%02x%02x%02x, nbits %u\n", nbits, back[0], back[1], back[2], back[3], back[4], gcry_mpi_get_nbits(opaque));
    void *owned = gcry_malloc(3);
    memcpy(owned, "xyz", 3);
    gcry_mpi_set_opaque(opaque, owned, 24);
    back = gcry_mpi_get_opaque(opaque, &nbits);
    printf("opaque again %u bits: %.3s\n", nbits, back);
    gcry_mpi_release(opaque);

    gcry_mpi_t r = gcry_mpi_new(0);
    gcry_mpi_randomize(r, 100, GCRY_STRONG_RANDOM);
    printf("random 100 bits: at most 104 %d\n", gcry_mpi_get_nbits(r) <= 104);
    gcry_mpi_release(r);
    gcry_mpi_t s = gcry_mpi_snew(64);
    show("snew", s);
    gcry_mpi_release(s);
    gcry_mpi_release(NULL);
}

int main(void)
{
    if (!gcry_check_version(GCRYPT_VERSION))
        return 1;
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
    formats();
    arithmetic();
    powers();
    bits();
    misc();
    return 0;
}
