# mlx-gcrypt

libgcrypt for the C and C++ programs built here, in Mlx, with libgcrypt's
C API (1.10.3), the part WebKit calls: WebCore's WebCrypto
(Source/WebCore/crypto/gcrypt) and PAL's digests
(Source/WebCore/PAL/pal/crypto/gcrypt) include `<gcrypt.h>` as they would
libgcrypt's header and link `libmlxgcrypt.so` in place of
`libgcrypt.so.20`. The library is this repository's own; the
cryptography under it is the standard library's `std.crypto`
(std/src/crypto/: hashes, HMAC, AES, GCM, bignum, the NIST curves,
X25519 and Ed25519), which TLS (`std.tls`) and the certificates of
`std.x509` use as well. This directory is libgcrypt's layer over it: its
handles, its MPIs, its S-expressions, its key formats and its errors,
checked against what the system's libgcrypt prints for the same
programs.

The interface is written once, in Mlx: the modules of `api/` declare the
types, constants, functions and macros C sees (`gcry_md_open`,
`GCRY_CIPHER_AES256`, `gcry_err_code`, `GCRYMPI_CONST_ONE`,
`gcry_buffer_t`, gpg-error.h's codes...). `mlx-cheaders` (compiler/cc)
makes the C headers of them into `mlx-out/include/mlx-gcrypt/`; there are
no headers written by hand.

    ./mlx build gcrypt          the library and its headers
    tools/check_gcrypt.sh       builds it and runs the tests (and them against the system's libgcrypt)
    tests/362_crypto_symmetric_runtime.mlx, tests/363_crypto_public_key_runtime.mlx
                                std.crypto itself, against the standards' vectors

## What it does

- **Digests and MACs** (md.mlx): `gcry_md_*` with SHA-1, SHA-224,
  SHA-256, SHA-384 and SHA-512 (HMAC by `GCRY_MD_FLAG_HMAC` and
  `gcry_md_setkey`), `gcry_md_hash_buffer`, `gcry_md_map_name`;
  `gcry_mac_*` (HMAC with the same hashes, reset, truncated reads,
  verify); `gcry_kdf_derive` (PBKDF2).
- **Ciphers** (cipher.mlx): `gcry_cipher_*` with AES-128/192/256 in ECB,
  CBC, CTR (any piece size, the counter carried), GCM (AAD and text in
  pieces, IVs of any length, tags of 4 to 16 bytes, `checktag`) and the
  AES key wrap (RFC 3394), with libgcrypt's errors for misuse.
- **MPIs** (mpi.mlx): signed numbers of 64-bit limbs; scanning and
  printing in every format (STD, USG, PGP, SSH, HEX), arithmetic,
  Knuth's division in both roundings, the modular operations, `powm`
  (an odd modulus by std.crypto.bignum's constant-time Montgomery
  ladder), `gcd`, `invm`, bits and shifts, opaque values, the constants,
  flags. libgcrypt's quirks are kept: `gcry_mpi_cmp` answers the
  difference of the limb counts when they differ, `invm` works on the
  magnitudes, `mulm` keeps the product's sign, a buffer too short
  clears `*nwritten`, an opaque MPI prints its bytes.
- **S-expressions** (sexp.mlx): libgcrypt's tagged layout; read from the
  advanced and the canonical forms (tokens, quoted strings and their
  escapes, hex, base64, verbatim data, display hints) with libgcrypt's
  errors and offsets; `gcry_sexp_build` with `%s %b %d %u %m %M %S` from
  C's variable arguments; printed in every mode; walked (`find_token`,
  `nth`, `car`, `cdr`, `nth_data`, `nth_mpi`...); `extract_param` with
  paths, every mode and optional parameters; `canon_len`.
- **Curves** (ec.mlx): NIST P-256, P-384, P-521, Curve25519 and Ed25519
  under libgcrypt's names and aliases; points; contexts from keys or
  names, `get_mpi` (`q` worked out of `d`, `q@eddsa`), `get_point`,
  `set_mpi`, `decode_point` (SEC 1, EdDSA's and Montgomery's
  encodings), `get_affine`, `mul`, `add`, `dup` and `curve_point` (the
  checks libgcrypt makes, Euler's criterion on Curve25519 included).
  Products are std.crypto's, constant time.
- **Public keys** (pk.mlx): RSA signatures (PKCS #1 v1.5, PSS),
  encryption (OAEP, PKCS #1 v1.5), the private operation by the CRT with
  the key's p, q and u (checked with e after signing, as libgcrypt
  does), key generation (sieved random primes, Miller-Rabin, p < q,
  `u = p^-1 mod q`, `d` modulo the lcm); ECDSA (sign and verify,
  generated keys), ECDH through `gcry_pk_encrypt` as WebCore derives
  bits, EdDSA on Ed25519 (RFC 8032; generated keys); `get_nbits`,
  `testkey`. Data and results are libgcrypt's S-expressions:
  `(data (flags pkcs1) (hash sha256 ...))`, `(sig-val (rsa (s ...)))` of
  the modulus's length, `(enc-val (ecdh (s ...) (e ...)))`, genkey's
  `(key-data (public-key ...) (private-key ...))`.
- Errors and their texts are libgpg-error's (`gcry_strerror`,
  `gcry_strsource`); `gcry_check_version`, `gcry_control`, the random
  functions (the kernel's `getrandom`), `gcry_malloc` and `gcry_free`.

## Tests

`tools/check_gcrypt.sh` builds the library and the headers (also as
C++), checks that every function the headers declare is defined, and
runs `tests/symmetric.c`, `tests/mpi.c`, `tests/sexp.c` and `tests/pk.c`
built by mlx-cc: each prints exactly what `NAME.expected` says, which is
what the system's libgcrypt 1.10.3 prints for the same program (the
script checks that again when gcc and libgcrypt are installed).
Randomized results (generated keys, PSS and ECDSA signatures, OAEP
ciphertexts) are printed only as what is the same every time: their
verification, decryption, sizes and consistency (`n = pq`, `u p = 1 mod
q`...). `tests/pk_vectors.h` has OpenSSL's keys, signatures and
ciphertexts the tests check against.

## What is missing

Only what WebKit uses is here: no other hashes (MD5, SHA-3, BLAKE2),
ciphers (ChaCha20 is in std.crypto but not offered), modes (CFB, OFB,
XTS, OCB) or curves (brainpool, secp256k1, X448, Ed448); no DSA or
ElGamal; `gcry_pk_decrypt` is RSA's only (ECDH decryption is not there);
no secure memory (the functions succeed and do nothing special). The
MPI arithmetic outside `powm` is schoolbook and not constant time (for
public values, as libgcrypt's own general MPI functions); RSA key
generation takes a few seconds for 2048 bits (Mlx's code generation is
far from libgcrypt's assembly).
