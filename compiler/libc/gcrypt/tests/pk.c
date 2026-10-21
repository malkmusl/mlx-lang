/* mlx-gcrypt's public keys as WebCore's WebCrypto drives them: RSA
   (PKCS #1 v1.5 and PSS signatures, OAEP encryption, keys imported with
   WebCore's u and generated), ECDSA and ECDH on P-256, P-384 and P-521
   (OpenSSL's signatures verified, keys imported, generated and checked
   with EC contexts), Ed25519 (RFC 8032's vectors, generated keys, the
   public key worked out with EC operations) and X25519 (RFC 7748's
   vectors through gcry_mpi_ec_mul). Randomized results (generated keys,
   PSS and ECDSA signatures, OAEP ciphertexts) are printed only as what
   is the same every time: their verification, decryption and sizes. It
   prints what libgcrypt 1.10 prints for the same program. */
#include <gcrypt.h>
#include <stdio.h>
#include <string.h>
#include "pk_vectors.h"

static void status(const char *label, gcry_error_t error)
{
    printf("%s: %u %s\n", label, gcry_err_code(error), gcry_strerror(error));
}

static size_t unhex(const char *text, unsigned char *out)
{
    size_t length = strlen(text) / 2;
    for (size_t i = 0; i < length; i++) {
        unsigned int byte;
        sscanf(text + 2 * i, "%2x", &byte);
        out[i] = (unsigned char)byte;
    }
    return length;
}

static void hex(const char *label, const unsigned char *data, size_t length)
{
    printf("%s [%zu]:", label, length);
    for (size_t i = 0; i < length; i++)
        printf("%02x", data[i]);
    printf("\n");
}

static void show(const char *label, gcry_sexp_t sexp)
{
    if (!sexp) {
        printf("%s: NULL\n", label);
        return;
    }
    char buffer[8192];
    gcry_sexp_sprint(sexp, GCRYSEXP_FMT_ADVANCED, buffer, sizeof buffer);
    printf("%s: %s", label, buffer);
}

static void mpi(const char *label, gcry_mpi_t a)
{
    if (!a) {
        printf("%s: NULL\n", label);
        return;
    }
    if (gcry_mpi_get_flag(a, GCRYMPI_FLAG_OPAQUE)) {
        unsigned int bits;
        const unsigned char *data = gcry_mpi_get_opaque(a, &bits);
        printf("%s: opaque", label);
        hex("", data, (bits + 7) / 8);
        return;
    }
    unsigned char *text = NULL;
    gcry_mpi_aprint(GCRYMPI_FMT_HEX, &text, NULL, a);
    printf("%s: %s\n", label, text);
    gcry_free(text);
}

/* The data of the token's list, element 1, as WebCore's mpiData reads it. */
static size_t token(gcry_sexp_t sexp, const char *name, unsigned char *out)
{
    gcry_sexp_t list = gcry_sexp_find_token(sexp, name, 0);
    if (!list)
        return 0;
    gcry_mpi_t value = gcry_sexp_nth_mpi(list, 1, GCRYMPI_FMT_USG);
    gcry_sexp_release(list);
    if (!value)
        return 0;
    size_t length = 0;
    gcry_mpi_print(GCRYMPI_FMT_USG, out, 1024, &length, value);
    gcry_mpi_release(value);
    return length;
}

static unsigned char abc_sha256[32];

/* ------------------------------------------------------------------------ */

static gcry_sexp_t rsa_key(int private)
{
    unsigned char n[300], d[300], p[200], q[200], e[3] = { 1, 0, 1 };
    size_t nl = unhex(RSA_N, n), dl = unhex(RSA_D, d), pl = unhex(RSA_P, p), ql = unhex(RSA_Q, q);
    gcry_sexp_t key = NULL;
    if (!private) {
        status("rsa public key", gcry_sexp_build(&key, NULL, "(public-key(rsa(n %b)(e %b)))", (int)nl, n, 3, e));
        return key;
    }
    /* As CryptoKeyRSA::importPkcs8 does: u = q^-1 mod p. */
    gcry_mpi_t pm = NULL, qm = NULL, u = gcry_mpi_new(0);
    gcry_mpi_scan(&pm, GCRYMPI_FMT_USG, p, pl, NULL);
    gcry_mpi_scan(&qm, GCRYMPI_FMT_USG, q, ql, NULL);
    gcry_mpi_invm(u, qm, pm);
    status("rsa private key", gcry_sexp_build(&key, NULL, "(private-key(rsa(n %b)(e %b)(d %b)(p %b)(q %b)(u %M)))", (int)nl, n, 3, e, (int)dl, d, (int)pl, p, (int)ql, q, u));
    gcry_mpi_release(pm);
    gcry_mpi_release(qm);
    gcry_mpi_release(u);
    return key;
}

static void rsa(void)
{
    gcry_sexp_t public = rsa_key(0), private = rsa_key(1);
    printf("rsa nbits public %u private %u\n", gcry_pk_get_nbits(public), gcry_pk_get_nbits(private));
    status("rsa testkey", gcry_pk_testkey(private));
    unsigned char buffer[1024];

    /* PKCS #1 v1.5: deterministic. */
    gcry_sexp_t data = NULL, signature = NULL;
    gcry_sexp_build(&data, NULL, "(data(flags pkcs1)(hash %s %b))", "sha256", 32, abc_sha256);
    status("pkcs1 sign", gcry_pk_sign(&signature, data, private));
    show("  signature", signature);
    size_t length = token(signature, "s", buffer);
    hex("  s", buffer, length);
    gcry_sexp_t given = NULL;
    gcry_sexp_build(&given, NULL, "(sig-val(rsa(s %b)))", (int)length, buffer);
    status("pkcs1 verify", gcry_pk_verify(given, data, public));
    gcry_sexp_release(given);
    buffer[10] ^= 1;
    gcry_sexp_build(&given, NULL, "(sig-val(rsa(s %b)))", (int)length, buffer);
    status("pkcs1 verify forged", gcry_pk_verify(given, data, public));
    gcry_sexp_release(given);
    gcry_sexp_release(data);
    gcry_sexp_release(signature);
    const char *hashes[] = { "sha1", "sha384", "sha512" };
    const int sizes[] = { 20, 48, 64 };
    for (int h = 0; h < 3; h++) {
        unsigned char digest[64];
        memset(digest, 0x5a, sizeof digest);
        gcry_sexp_build(&data, NULL, "(data(flags pkcs1)(hash %s %b))", hashes[h], sizes[h], digest);
        gcry_pk_sign(&signature, data, private);
        length = token(signature, "s", buffer);
        printf("pkcs1 %s:", hashes[h]);
        hex("", buffer, length);
        status("  verify", gcry_pk_verify(signature, data, public));
        gcry_sexp_release(data);
        gcry_sexp_release(signature);
    }

    /* PSS: OpenSSL's signature, and our own (random salt). */
    unsigned char pss[256];
    size_t pssLength = unhex(RSA_PSS_SIG, pss);
    gcry_sexp_build(&data, NULL, "(data(flags pss)(salt-length %u)(hash %s %b))", 32, "sha256", 32, abc_sha256);
    gcry_sexp_build(&given, NULL, "(sig-val(rsa(s %b)))", (int)pssLength, pss);
    status("pss verify openssl", gcry_pk_verify(given, data, public));
    gcry_sexp_release(given);
    status("pss sign", gcry_pk_sign(&signature, data, private));
    length = token(signature, "s", buffer);
    printf("  length %zu\n", length);
    gcry_sexp_build(&given, NULL, "(sig-val(rsa(s %b)))", (int)length, buffer);
    status("  verify", gcry_pk_verify(given, data, public));
    gcry_sexp_release(given);
    gcry_sexp_release(data);
    gcry_sexp_build(&data, NULL, "(data(flags pss)(salt-length %u)(hash %s %b))", 20, "sha256", 32, abc_sha256);
    status("  verify with another salt length", gcry_pk_verify(signature, data, public));
    gcry_sexp_release(data);
    gcry_sexp_release(signature);
    gcry_sexp_build(&data, NULL, "(data(flags pss)(salt-length %u)(hash %s %b))", 0, "sha384", 32, abc_sha256);
    status("pss sign sha384 salt 0", gcry_pk_sign(&signature, data, private));
    length = token(signature, "s", buffer);
    hex("  deterministic s", buffer, length);
    status("  verify", gcry_pk_verify(signature, data, public));
    gcry_sexp_release(data);
    gcry_sexp_release(signature);

    /* OAEP: OpenSSL's ciphertexts, and round trips. */
    const char *ciphertexts[] = { RSA_OAEP_SHA256, RSA_OAEP_SHA1_LABEL };
    const char *oaepHashes[] = { "sha256", "sha1" };
    const char *labels[] = { "", "label" };
    for (int i = 0; i < 2; i++) {
        unsigned char ct[256];
        size_t ctLength = unhex(ciphertexts[i], ct);
        gcry_sexp_t encrypted = NULL, plain = NULL;
        gcry_sexp_build(&encrypted, NULL, "(enc-val(flags oaep)(hash-algo %s)(label %b)(rsa(a %b)))", oaepHashes[i], (int)strlen(labels[i]), labels[i], (int)ctLength, ct);
        status("oaep decrypt openssl", gcry_pk_decrypt(&plain, encrypted, private));
        show("  plain", plain);
        gcry_sexp_release(plain);
        gcry_sexp_release(encrypted);
        gcry_sexp_build(&encrypted, NULL, "(enc-val(flags oaep)(hash-algo %s)(label %b)(rsa(a %b)))", oaepHashes[i], 3, "bad", (int)ctLength, ct);
        status("  wrong label", gcry_pk_decrypt(&plain, encrypted, private));
        gcry_sexp_release(plain);
        gcry_sexp_release(encrypted);
        plain = NULL;
        gcry_sexp_build(&data, NULL, "(data(flags oaep)(hash-algo %s)(label %b)(value %b))", oaepHashes[i], (int)strlen(labels[i]), labels[i], 11, "round trip!");
        gcry_sexp_t cipher = NULL;
        status("oaep encrypt", gcry_pk_encrypt(&cipher, data, public));
        length = token(cipher, "a", buffer);
        printf("  a length %s\n", length > 240 && length <= 256 ? "about 256" : "wrong");
        gcry_sexp_build(&encrypted, NULL, "(enc-val(flags oaep)(hash-algo %s)(label %b)(rsa(a %b)))", oaepHashes[i], (int)strlen(labels[i]), labels[i], (int)length, buffer);
        status("  decrypt", gcry_pk_decrypt(&plain, encrypted, private));
        show("  plain", plain);
        gcry_sexp_release(plain);
        gcry_sexp_release(encrypted);
        gcry_sexp_release(cipher);
        gcry_sexp_release(data);
    }
    gcry_sexp_build(&data, NULL, "(data(flags oaep)(hash-algo %s)(label %b)(value %b))", "sha256", 0, "", 200, buffer);
    gcry_sexp_t cipher = NULL;
    status("oaep encrypt too long", gcry_pk_encrypt(&cipher, data, public));
    gcry_sexp_release(data);
    gcry_sexp_release(public);
    gcry_sexp_release(private);
}

static int same_mpi(gcry_mpi_t a, gcry_mpi_t b) { return gcry_mpi_cmp(a, b) == 0; }

static void rsa_generate(int bits)
{
    gcry_sexp_t parameters = NULL, pair = NULL;
    gcry_sexp_build(&parameters, NULL, "(genkey(rsa(nbits %d)(rsa-use-e %d)))", bits, 65537);
    status("rsa genkey", gcry_pk_genkey(&pair, parameters));
    gcry_sexp_release(parameters);
    printf("  key-data length %d\n", gcry_sexp_length(pair));
    gcry_sexp_t public = gcry_sexp_find_token(pair, "public-key", 0);
    gcry_sexp_t private = gcry_sexp_find_token(pair, "private-key", 0);
    printf("  nbits %u %u\n", gcry_pk_get_nbits(public), gcry_pk_get_nbits(private));
    gcry_mpi_t n = NULL, e = NULL, d = NULL, p = NULL, q = NULL, u = NULL;
    status("  extract", gcry_sexp_extract_param(private, NULL, "nedpqu", &n, &e, &d, &p, &q, &u, NULL));
    mpi("  e", e);
    gcry_mpi_t t = gcry_mpi_new(0), one = gcry_mpi_set_ui(NULL, 1);
    gcry_mpi_mul(t, p, q);
    printf("  n = pq %d, p < q %d\n", same_mpi(t, n), gcry_mpi_cmp(p, q) < 0);
    gcry_mpi_mulm(t, u, p, q);
    printf("  u p = 1 mod q %d\n", same_mpi(t, one));
    gcry_mpi_t p1 = gcry_mpi_new(0), q1 = gcry_mpi_new(0), phi = gcry_mpi_new(0), g = gcry_mpi_new(0);
    gcry_mpi_sub_ui(p1, p, 1);
    gcry_mpi_sub_ui(q1, q, 1);
    gcry_mpi_mul(phi, p1, q1);
    gcry_mpi_gcd(g, p1, q1);
    gcry_mpi_div(phi, NULL, phi, g, 0);
    gcry_mpi_mulm(t, d, e, phi);
    printf("  d e = 1 mod lcm %d\n", same_mpi(t, one));
    printf("  p bits %u q bits %u\n", gcry_mpi_get_nbits(p), gcry_mpi_get_nbits(q));
    gcry_sexp_t data = NULL, signature = NULL;
    gcry_sexp_build(&data, NULL, "(data(flags pkcs1)(hash %s %b))", "sha256", 32, abc_sha256);
    status("  sign", gcry_pk_sign(&signature, data, private));
    status("  verify", gcry_pk_verify(signature, data, public));
    gcry_mpi_release(n); gcry_mpi_release(e); gcry_mpi_release(d); gcry_mpi_release(p); gcry_mpi_release(q); gcry_mpi_release(u);
    gcry_mpi_release(t); gcry_mpi_release(one); gcry_mpi_release(p1); gcry_mpi_release(q1); gcry_mpi_release(phi); gcry_mpi_release(g);
    gcry_sexp_release(data);
    gcry_sexp_release(signature);
    gcry_sexp_release(public);
    gcry_sexp_release(private);
    gcry_sexp_release(pair);
}

/* ------------------------------------------------------------------------ */

struct curve {
    const char *name;
    const char *d, *q, *r, *s;
    size_t size;
};

static const struct curve curves[] = {
    { "NIST P-256", EC_P256_D, EC_P256_Q, EC_P256_R, EC_P256_S, 32 },
    { "NIST P-384", EC_P384_D, EC_P384_Q, EC_P384_R, EC_P384_S, 48 },
    { "NIST P-521", EC_P521_D, EC_P521_Q, EC_P521_R, EC_P521_S, 66 },
};

static void ecdsa(const struct curve *curve)
{
    printf("== %s\n", curve->name);
    unsigned char d[80], q[160], r[80], s[80], buffer[1024];
    size_t dl = unhex(curve->d, d), ql = unhex(curve->q, q), rl = unhex(curve->r, r), sl = unhex(curve->s, s);
    /* The private key as importJWK makes it, and as importPkcs8 makes it
       (d alone, q computed by the context). */
    gcry_sexp_t public = NULL, private = NULL, onlyD = NULL;
    gcry_sexp_build(&public, NULL, "(public-key(ecc(curve %s)(q %b)))", curve->name, (int)ql, q);
    gcry_sexp_build(&private, NULL, "(private-key(ecc(curve %s)(q %b)(d %b)))", curve->name, (int)ql, q, (int)dl, d);
    gcry_sexp_build(&onlyD, NULL, "(private-key(ecc(curve %s)(d %b)))", curve->name, (int)dl, d);
    printf("nbits %u %u\n", gcry_pk_get_nbits(public), gcry_pk_get_nbits(private));
    gcry_ctx_t context = NULL;
    status("ec_new from d", gcry_mpi_ec_new(&context, onlyD, NULL));
    gcry_mpi_t qm = gcry_mpi_ec_get_mpi("q", context, 0);
    gcry_mpi_t dm = gcry_mpi_ec_get_mpi("d", context, 0);
    size_t length = 0;
    gcry_mpi_print(GCRYMPI_FMT_USG, buffer, sizeof buffer, &length, qm);
    printf("q from d matches %d\n", length == ql && memcmp(buffer, q, ql) == 0);
    mpi("d", dm);
    gcry_mpi_point_t point = gcry_mpi_ec_get_point("q", context, 1);
    printf("q on curve %d\n", gcry_mpi_ec_curve_point(point, context));
    status("set_mpi q", gcry_mpi_ec_set_mpi("q", qm, context));
    gcry_mpi_point_release(point);
    gcry_mpi_release(qm);
    gcry_mpi_release(dm);
    gcry_ctx_release(context);

    /* importSpki: the point from x and y, checked on a context of the name. */
    gcry_mpi_t x = NULL, y = NULL;
    gcry_mpi_scan(&x, GCRYMPI_FMT_USG, q + 1, curve->size, NULL);
    gcry_mpi_scan(&y, GCRYMPI_FMT_USG, q + 1 + curve->size, curve->size, NULL);
    point = gcry_mpi_point_set(NULL, x, y, GCRYMPI_CONST_ONE);
    status("ec_new by name", gcry_mpi_ec_new(&context, NULL, curve->name));
    printf("spki point on curve %d\n", gcry_mpi_ec_curve_point(point, context));
    gcry_mpi_add_ui(y, y, 1);
    gcry_mpi_point_set(point, x, y, GCRYMPI_CONST_ONE);
    printf("moved point on curve %d\n", gcry_mpi_ec_curve_point(point, context));
    mpi("p", gcry_mpi_ec_get_mpi("p", context, 1));
    mpi("n", gcry_mpi_ec_get_mpi("n", context, 1));
    mpi("g", gcry_mpi_ec_get_mpi("g", context, 1));
    gcry_mpi_point_release(point);
    gcry_mpi_release(x);
    gcry_mpi_release(y);
    gcry_ctx_release(context);

    /* exportJWK: get_mpi q and d from a key with both. */
    gcry_mpi_ec_new(&context, private, NULL);
    qm = gcry_mpi_ec_get_mpi("q", context, 0);
    gcry_mpi_print(GCRYMPI_FMT_USG, buffer, sizeof buffer, &length, qm);
    printf("q export matches %d\n", length == ql && memcmp(buffer, q, ql) == 0);
    gcry_mpi_release(qm);
    gcry_ctx_release(context);

    /* OpenSSL's signature, and ours. */
    gcry_sexp_t data = NULL, signature = NULL, given = NULL;
    gcry_sexp_build(&data, NULL, "(data(flags raw)(hash %s %b))", "sha256", 32, abc_sha256);
    gcry_sexp_build(&given, NULL, "(sig-val(ecdsa(r %b)(s %b)))", (int)rl, r, (int)sl, s);
    status("verify openssl", gcry_pk_verify(given, data, public));
    status("verify openssl with the private key", gcry_pk_verify(given, data, private));
    gcry_sexp_release(given);
    s[sl - 1] ^= 1;
    gcry_sexp_build(&given, NULL, "(sig-val(ecdsa(r %b)(s %b)))", (int)rl, r, (int)sl, s);
    status("verify forged", gcry_pk_verify(given, data, public));
    gcry_sexp_release(given);
    status("sign", gcry_pk_sign(&signature, data, private));
    length = token(signature, "r", buffer);
    printf("  r at most %zu %d\n", curve->size, length <= curve->size);
    status("  verify", gcry_pk_verify(signature, data, public));
    gcry_sexp_release(signature);
    status("sign with d alone", gcry_pk_sign(&signature, data, onlyD));
    status("  verify", gcry_pk_verify(signature, data, public));
    gcry_sexp_release(signature);
    gcry_sexp_release(data);
    unsigned char digest[64];
    memset(digest, 0xa5, sizeof digest);
    gcry_sexp_build(&data, NULL, "(data(flags raw)(hash %s %b))", "sha512", 64, digest);
    status("sign sha512", gcry_pk_sign(&signature, data, private));
    status("  verify", gcry_pk_verify(signature, data, public));
    gcry_sexp_release(signature);
    gcry_sexp_release(data);

    /* ECDH as CryptoAlgorithmECDH derives: the private key's d against a
       public key, both ways with a generated key. */
    gcry_sexp_t parameters = NULL, pair = NULL;
    gcry_sexp_build(&parameters, NULL, "(genkey(ecc(curve %s)))", curve->name);
    status("genkey", gcry_pk_genkey(&pair, parameters));
    gcry_sexp_release(parameters);
    gcry_sexp_t otherPublic = gcry_sexp_find_token(pair, "public-key", 0);
    gcry_sexp_t otherPrivate = gcry_sexp_find_token(pair, "private-key", 0);
    printf("  key-data length %d, nbits %u\n", gcry_sexp_length(pair), gcry_pk_get_nbits(otherPrivate));
    length = token(otherPublic, "q", buffer);
    printf("  q length %zu\n", length);
    unsigned char secrets[2][80];
    gcry_sexp_t keys[2][2] = { { private, otherPublic }, { otherPrivate, public } };
    for (int way = 0; way < 2; way++) {
        size_t dLength = token(keys[way][0], "d", buffer);
        gcry_sexp_build(&data, NULL, "(data(flags raw)(value %b))", (int)dLength, buffer);
        gcry_sexp_t cipher = NULL;
        status("ecdh encrypt", gcry_pk_encrypt(&cipher, data, keys[way][1]));
        gcry_sexp_t sList = gcry_sexp_find_token(cipher, "s", 0);
        gcry_mpi_t sm = gcry_sexp_nth_mpi(sList, 1, GCRYMPI_FMT_USG);
        gcry_mpi_point_t shared = gcry_mpi_point_new(0);
        status("  decode", gcry_mpi_ec_decode_point(shared, sm, NULL));
        gcry_mpi_t xs = gcry_mpi_new(0);
        gcry_mpi_point_snatch_get(xs, NULL, NULL, shared);
        memset(secrets[way], 0, sizeof secrets[way]);
        size_t xLength = 0;
        gcry_mpi_print(GCRYMPI_FMT_USG, NULL, 0, &xLength, xs);
        gcry_mpi_print(GCRYMPI_FMT_USG, secrets[way] + curve->size - xLength, xLength, NULL, xs);
        gcry_mpi_release(xs);
        gcry_mpi_release(sm);
        gcry_sexp_release(sList);
        gcry_sexp_release(cipher);
        gcry_sexp_release(data);
    }
    printf("  shared secrets agree %d\n", memcmp(secrets[0], secrets[1], curve->size) == 0);
    /* With OpenSSL's key against itself: deterministic. */
    gcry_sexp_build(&data, NULL, "(data(flags raw)(value %b))", (int)dl, d);
    gcry_sexp_t cipher = NULL;
    status("ecdh with itself", gcry_pk_encrypt(&cipher, data, public));
    show("  result", cipher);
    gcry_sexp_release(cipher);
    gcry_sexp_release(data);
    gcry_sexp_build(&data, NULL, "(data(flags raw)(hash %s %b))", "sha256", 32, abc_sha256);
    status("sign with generated", gcry_pk_sign(&signature, data, otherPrivate));
    status("  verify", gcry_pk_verify(signature, data, otherPublic));
    gcry_sexp_release(signature);
    gcry_sexp_release(data);
    gcry_sexp_release(otherPublic);
    gcry_sexp_release(otherPrivate);
    gcry_sexp_release(pair);
    gcry_sexp_release(public);
    gcry_sexp_release(private);
    gcry_sexp_release(onlyD);
}

/* ------------------------------------------------------------------------ */

static void reverse(unsigned char *out, const unsigned char *in, size_t length)
{
    for (size_t i = 0; i < length; i++)
        out[i] = in[length - 1 - i];
}

/* GCrypt::RFC7748::X25519 as WebCore has it. */
static void x25519(const unsigned char *k, const unsigned char *u, unsigned char *out)
{
    gcry_ctx_t context = NULL;
    gcry_mpi_ec_new(&context, NULL, "Curve25519");
    unsigned char kb[32], ub[32];
    reverse(kb, k, 32);
    reverse(ub, u, 32);
    gcry_mpi_t km = NULL, um = NULL;
    gcry_mpi_scan(&km, GCRYMPI_FMT_USG, kb, 32, NULL);
    gcry_mpi_clear_bit(km, 0);
    gcry_mpi_clear_bit(km, 1);
    gcry_mpi_clear_bit(km, 2);
    gcry_mpi_set_bit(km, 254);
    gcry_mpi_clear_bit(km, 255);
    gcry_mpi_scan(&um, GCRYMPI_FMT_USG, ub, 32, NULL);
    gcry_mpi_clear_bit(um, 255);
    gcry_mpi_point_t Q = gcry_mpi_point_new(0);
    gcry_mpi_point_t P = gcry_mpi_point_set(NULL, um, NULL, GCRYMPI_CONST_ONE);
    gcry_mpi_ec_mul(Q, km, P, context);
    int infinite = gcry_mpi_ec_get_affine(km, NULL, Q, context);
    memset(out, 0, 32);
    if (!infinite) {
        unsigned char data[64];
        size_t length = 0;
        gcry_mpi_print(GCRYMPI_FMT_USG, data, sizeof data, &length, km);
        for (size_t i = 0; i < length && i < 32; i++)
            out[i] = data[length - 1 - i];
    }
    printf("  get_affine %d\n", infinite);
    gcry_mpi_point_release(Q);
    gcry_mpi_point_release(P);
    gcry_mpi_release(km);
    gcry_mpi_release(um);
    gcry_ctx_release(context);
}

static void curve25519(void)
{
    printf("== X25519\n");
    unsigned char k[32], u[32], out[32];
    unhex("A546E36BF0527C9D3B16154B82465EDD62144C0AC1FC5A18506A2244BA449AC4", k);
    unhex("E6DB6867583030DB3594C1A424B15F7C726624EC26B3353B10A903A6D0AB1C4C", u);
    x25519(k, u, out);
    hex("rfc 7748 5.2 1", out, 32);
    unsigned char alice[32], bob[32], base[32] = { 9 }, alicePublic[32], bobPublic[32], s1[32], s2[32];
    unhex("77076D0A7318A57D3C16C17251B26645DF4C2F87EBC0992AB177FBA51DB92C2A", alice);
    unhex("5DAB087E624A8A4B79E17F8B83800EE66F3BB1292618B6FD1C2F8B27FF88E0EB", bob);
    x25519(alice, base, alicePublic);
    hex("alice public", alicePublic, 32);
    x25519(bob, base, bobPublic);
    hex("bob public", bobPublic, 32);
    x25519(alice, bobPublic, s1);
    x25519(bob, alicePublic, s2);
    hex("shared", s1, 32);
    printf("agree %d\n", memcmp(s1, s2, 32) == 0);
    unsigned char zero[32] = { 0 };
    x25519(alice, zero, out);
    hex("with u = 0", out, 32);
    gcry_ctx_t context = NULL;
    status("ec_new Curve25519", gcry_mpi_ec_new(&context, NULL, "Curve25519"));
    mpi("p", gcry_mpi_ec_get_mpi("p", context, 1));
    gcry_ctx_release(context);
    status("ec_new unknown", gcry_mpi_ec_new(&context, NULL, "NIST P-999"));
}

/* GCrypt::RFC8032::validateEd25519KeyPair's computation of the public key. */
static void ed25519_public(const unsigned char *secret, unsigned char *out)
{
    gcry_ctx_t context = NULL;
    gcry_mpi_ec_new(&context, NULL, "Ed25519");
    unsigned char digest[64], hd[32];
    gcry_md_hash_buffer(GCRY_MD_SHA512, digest, secret, 32);
    reverse(hd, digest, 32);
    gcry_mpi_t hdm = NULL;
    gcry_mpi_scan(&hdm, GCRYMPI_FMT_USG, hd, 32, NULL);
    gcry_mpi_clear_bit(hdm, 0);
    gcry_mpi_clear_bit(hdm, 1);
    gcry_mpi_clear_bit(hdm, 2);
    gcry_mpi_set_bit(hdm, 254);
    gcry_mpi_clear_bit(hdm, 255);
    gcry_mpi_point_t G = gcry_mpi_ec_get_point("g", context, 1);
    gcry_mpi_point_t Q = gcry_mpi_point_new(0);
    gcry_mpi_t x = gcry_mpi_new(0), y = gcry_mpi_new(0);
    gcry_mpi_ec_mul(Q, hdm, G, context);
    int ret = gcry_mpi_ec_get_affine(x, y, Q, context);
    if (gcry_mpi_test_bit(x, 0))
        gcry_mpi_set_bit(y, 255);
    else
        gcry_mpi_clear_bit(y, 255);
    unsigned char data[64];
    size_t length = 0;
    gcry_mpi_print(GCRYMPI_FMT_USG, data, sizeof data, &length, y);
    memset(out, 0, 32);
    for (size_t i = 0; i < length && i < 32; i++)
        out[i] = data[length - 1 - i];
    printf("  get_affine %d, Q on curve %d, G on curve %d\n", ret, gcry_mpi_ec_curve_point(Q, context), gcry_mpi_ec_curve_point(G, context));
    gcry_mpi_point_release(G);
    gcry_mpi_point_release(Q);
    gcry_mpi_release(x);
    gcry_mpi_release(y);
    gcry_mpi_release(hdm);
    gcry_ctx_release(context);
}

static void ed25519(void)
{
    printf("== Ed25519\n");
    const char *secrets[] = { "9D61B19DEFFD5A60BA844AF492EC2CC44449C5697B326919703BAC031CAE7F60", "4CCD089B28FF96DA9DB6C346EC114E0F5B8A319F35ABA624DA8CF6ED4FB8A6FB" };
    const char *messages[] = { "", "72" };
    for (int i = 0; i < 2; i++) {
        unsigned char secret[32], message[8], public[32], buffer[128];
        unhex(secrets[i], secret);
        size_t ml = unhex(messages[i], message);
        ed25519_public(secret, public);
        hex("public", public, 32);
        gcry_sexp_t data = NULL, key = NULL, signature = NULL, publicKey = NULL;
        gcry_sexp_build(&data, NULL, "(data(flags eddsa)(hash-algo sha512) (value %b))", (int)ml, message);
        gcry_sexp_build(&key, NULL, "(private-key(ecc(curve Ed25519)(flags eddsa)(d %b)))", 32, secret);
        status("sign", gcry_pk_sign(&signature, data, key));
        show("  signature", signature);
        size_t rl = token(signature, "r", buffer);
        size_t sl = token(signature, "s", buffer + 64);
        printf("  r %zu s %zu\n", rl, sl);
        gcry_sexp_build(&publicKey, NULL, "(public-key(ecc(curve Ed25519)(q %b)))", 32, public);
        status("  verify", gcry_pk_verify(signature, data, publicKey));
        gcry_sexp_t forged = NULL;
        gcry_sexp_build(&forged, NULL, "(data(flags eddsa)(hash-algo sha512) (value %b))", 3, "xyz");
        status("  verify other data", gcry_pk_verify(signature, forged, publicKey));
        gcry_sexp_release(forged);
        /* The key's context, as exportRaw reads q from it. */
        gcry_ctx_t context = NULL;
        status("  ec_new", gcry_mpi_ec_new(&context, key, NULL));
        mpi("  q@eddsa", gcry_mpi_ec_get_mpi("q@eddsa", context, 0));
        gcry_mpi_point_t point = gcry_mpi_ec_get_point("q", context, 1);
        printf("  q on curve %d\n", point ? gcry_mpi_ec_curve_point(point, context) : -1);
        gcry_mpi_point_release(point);
        gcry_ctx_release(context);
        /* The public key's context, as importRaw checks the point. */
        status("  ec_new public", gcry_mpi_ec_new(&context, publicKey, NULL));
        point = gcry_mpi_ec_get_point("q", context, 1);
        printf("  public q on curve %d\n", point ? gcry_mpi_ec_curve_point(point, context) : -1);
        gcry_mpi_point_release(point);
        gcry_ctx_release(context);
        gcry_sexp_release(data);
        gcry_sexp_release(key);
        gcry_sexp_release(signature);
        gcry_sexp_release(publicKey);
    }
    gcry_sexp_t parameters = NULL, pair = NULL;
    gcry_sexp_build(&parameters, NULL, "(genkey (ecdsa (curve Ed25519) (flags eddsa)))");
    status("genkey", gcry_pk_genkey(&pair, parameters));
    gcry_mpi_t q = NULL, d = NULL;
    status("  extract qd", gcry_sexp_extract_param(pair, "private-key", "qd", &q, &d, NULL));
    size_t ql = 0, dl = 0;
    gcry_mpi_print(GCRYMPI_FMT_USG, NULL, 0, &ql, q);
    gcry_mpi_print(GCRYMPI_FMT_USG, NULL, 0, &dl, d);
    printf("  q %s, d %s\n", ql >= 31 && ql <= 32 ? "32 bytes" : "wrong", dl >= 31 && dl <= 32 ? "32 bytes" : "wrong");
    gcry_sexp_t public = gcry_sexp_find_token(pair, "public-key", 0);
    gcry_sexp_t private = gcry_sexp_find_token(pair, "private-key", 0);
    printf("  nbits %u\n", gcry_pk_get_nbits(public));
    gcry_sexp_t data = NULL, signature = NULL;
    gcry_sexp_build(&data, NULL, "(data(flags eddsa)(hash-algo sha512) (value %b))", 3, "abc");
    status("  sign", gcry_pk_sign(&signature, data, private));
    status("  verify", gcry_pk_verify(signature, data, public));
    gcry_mpi_release(q);
    gcry_mpi_release(d);
    gcry_sexp_release(data);
    gcry_sexp_release(signature);
    gcry_sexp_release(public);
    gcry_sexp_release(private);
    gcry_sexp_release(pair);
    gcry_sexp_release(parameters);
}

int main(void)
{
    if (!gcry_check_version(GCRYPT_VERSION))
        return 1;
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
    gcry_md_hash_buffer(GCRY_MD_SHA256, abc_sha256, "abc", 3);
    rsa();
    rsa_generate(2048);
    rsa_generate(1024);
    for (int i = 0; i < 3; i++)
        ecdsa(&curves[i]);
    curve25519();
    ed25519();
    return 0;
}
