/* mlx-gcrypt's digests, MACs, PBKDF2 and AES modes as WebCore's
   WebCrypto and PAL call them: gcry_md_* (open, write, final, read,
   hash_buffer), gcry_mac_* (HMAC, reset for HKDF, truncated reads,
   verify), gcry_kdf_derive (PBKDF2), gcry_cipher_* (CBC, ECB, CTR across
   pieces and counter wrap, GCM with AAD in pieces, other IV lengths,
   tags, the key wrap and its integrity check) and the errors of misuse.
   It prints what libgcrypt 1.10 prints for the same program. */
#include <gcrypt.h>
#include <stdio.h>
#include <string.h>

static void hex(const char *label, const unsigned char *data, size_t length)
{
    printf("%s:", label);
    for (size_t i = 0; i < length; i++)
        printf("%02x", data[i]);
    printf("\n");
}

static void status(const char *label, gcry_error_t error)
{
    printf("%s: %u %s/%s\n", label, gcry_err_code(error), gcry_strsource(error), gcry_strerror(error));
}

static unsigned char data[1000];

static void digests(void)
{
    const int algos[] = { GCRY_MD_SHA1, GCRY_MD_SHA224, GCRY_MD_SHA256, GCRY_MD_SHA384, GCRY_MD_SHA512 };
    for (int i = 0; i < 5; i++) {
        gcry_md_hd_t md;
        gcry_error_t error = gcry_md_open(&md, algos[i], 0);
        if (error) {
            status("md_open", error);
            continue;
        }
        gcry_md_write(md, "abc", 3);
        gcry_md_final(md);
        printf("%s len %u ", gcry_md_algo_name(algos[i]), gcry_md_get_algo_dlen(algos[i]));
        hex("abc", gcry_md_read(md, 0), gcry_md_get_algo_dlen(algos[i]));
        gcry_md_close(md);
        unsigned char digest[64];
        gcry_md_hash_buffer(algos[i], digest, data, sizeof data);
        hex("  1000 bytes", digest, gcry_md_get_algo_dlen(algos[i]));
        gcry_md_open(&md, algos[i], 0);
        for (int piece = 0; piece < 10; piece++)
            gcry_md_write(md, data + piece * 100, 100);
        hex("  in pieces", gcry_md_read(md, algos[i]), gcry_md_get_algo_dlen(algos[i]));
        gcry_md_close(md);
    }
    gcry_md_hd_t md;
    status("unknown digest", gcry_md_open(&md, 9999, 0));
    printf("map sha256 %d, SHA-512 %d, nothing %d\n", gcry_md_map_name("sha256"), gcry_md_map_name("SHA-512"), gcry_md_map_name("md5x"));
}

static void macs(void)
{
    const int algos[] = { GCRY_MAC_HMAC_SHA1, GCRY_MAC_HMAC_SHA224, GCRY_MAC_HMAC_SHA256, GCRY_MAC_HMAC_SHA384, GCRY_MAC_HMAC_SHA512 };
    unsigned char key[200];
    for (int i = 0; i < 200; i++)
        key[i] = i * 7 + 3;
    for (int i = 0; i < 5; i++) {
        gcry_mac_hd_t mac;
        gcry_error_t error = gcry_mac_open(&mac, algos[i], 0, NULL);
        if (error) {
            status("mac_open", error);
            continue;
        }
        size_t length = gcry_mac_get_algo_maclen(algos[i]);
        unsigned char out[64];
        gcry_mac_setkey(mac, key, 20);
        gcry_mac_write(mac, "what do ya want for nothing?", 28);
        gcry_mac_read(mac, out, &length);
        printf("hmac %d len %zu ", algos[i], length);
        hex("", out, length);
        /* Reset: the same key, a new message (HKDF's expansion). */
        gcry_mac_reset(mac);
        gcry_mac_write(mac, data, 300);
        length = 64;
        gcry_mac_read(mac, out, &length);
        printf("  after reset %zu ", length);
        hex("", out, length);
        status("  verify", gcry_mac_verify(mac, out, length));
        out[0] ^= 1;
        gcry_mac_reset(mac);
        gcry_mac_write(mac, data, 300);
        status("  verify changed", gcry_mac_verify(mac, out, length));
        /* A long key, a truncated read. */
        gcry_mac_setkey(mac, key, 200);
        gcry_mac_write(mac, data, 1000);
        length = 10;
        gcry_mac_read(mac, out, &length);
        printf("  long key, truncated %zu ", length);
        hex("", out, length);
        gcry_mac_close(mac);
    }
    gcry_mac_hd_t mac;
    status("unknown mac", gcry_mac_open(&mac, 9999, 0, NULL));
}

static void pbkdf2(void)
{
    const int algos[] = { GCRY_MD_SHA1, GCRY_MD_SHA256, GCRY_MD_SHA384, GCRY_MD_SHA512 };
    const unsigned long counts[] = { 1, 2, 1000 };
    unsigned char out[100];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            gcry_error_t error = gcry_kdf_derive("password", 8, GCRY_KDF_PBKDF2, algos[i], "salt", 4, counts[j], 70, out);
            printf("pbkdf2 %d %lu %u ", algos[i], counts[j], gcry_err_code(error));
            hex("", out, 70);
        }
    }
    status("pbkdf2 zero iterations", gcry_kdf_derive("p", 1, GCRY_KDF_PBKDF2, GCRY_MD_SHA256, "s", 1, 0, 16, out));
    status("pbkdf2 empty salt", gcry_kdf_derive("p", 1, GCRY_KDF_PBKDF2, GCRY_MD_SHA256, "", 0, 3, 16, out));
    hex("  ", out, 16);
}

static void ciphers(void)
{
    const int algos[] = { GCRY_CIPHER_AES128, GCRY_CIPHER_AES192, GCRY_CIPHER_AES256 };
    unsigned char key[32], iv[16], buffer[128], back[128], tag[16];
    for (int i = 0; i < 32; i++)
        key[i] = 0x10 + i;
    for (int i = 0; i < 16; i++)
        iv[i] = 0xa0 + i;
    for (int a = 0; a < 3; a++) {
        int algo = algos[a];
        size_t keylen = gcry_cipher_get_algo_keylen(algo);
        printf("aes %d key %zu block %zu\n", algo, keylen, gcry_cipher_get_algo_blklen(algo));
        gcry_cipher_hd_t h;

        /* CBC, as WebCore pads it. */
        gcry_cipher_open(&h, algo, GCRY_CIPHER_MODE_CBC, 0);
        gcry_cipher_setkey(h, key, keylen);
        gcry_cipher_setiv(h, iv, 16);
        gcry_cipher_final(h);
        status("  cbc", gcry_cipher_encrypt(h, buffer, 64, data, 64));
        hex("  cbc", buffer, 64);
        gcry_cipher_reset(h);
        gcry_cipher_setiv(h, iv, 16);
        gcry_cipher_decrypt(h, back, 64, buffer, 64);
        printf("  cbc back %d\n", memcmp(back, data, 64) == 0);
        status("  cbc partial block", gcry_cipher_encrypt(h, buffer, 64, data, 20));
        status("  cbc short output", gcry_cipher_encrypt(h, buffer, 16, data, 32));
        gcry_cipher_close(h);

        /* ECB, in place. */
        gcry_cipher_open(&h, algo, GCRY_CIPHER_MODE_ECB, 0);
        gcry_cipher_setkey(h, key, keylen);
        memcpy(buffer, data, 48);
        gcry_cipher_encrypt(h, buffer, 48, NULL, 0);
        hex("  ecb", buffer, 48);
        gcry_cipher_decrypt(h, buffer, 48, NULL, 0);
        printf("  ecb back %d\n", memcmp(buffer, data, 48) == 0);
        gcry_cipher_close(h);

        /* CTR across pieces and over the counter's wrap. */
        gcry_cipher_open(&h, algo, GCRY_CIPHER_MODE_CTR, 0);
        gcry_cipher_setkey(h, key, keylen);
        unsigned char counter[16];
        memset(counter, 0xff, 16);
        counter[0] = 0x42;
        gcry_cipher_setctr(h, counter, 16);
        gcry_cipher_encrypt(h, buffer, 5, data, 5);
        gcry_cipher_encrypt(h, buffer + 5, 20, data + 5, 20);
        gcry_cipher_encrypt(h, buffer + 25, 30, data + 25, 30);
        hex("  ctr", buffer, 55);
        gcry_cipher_reset(h);
        gcry_cipher_setctr(h, counter, 16);
        gcry_cipher_decrypt(h, back, 55, buffer, 55);
        printf("  ctr back %d\n", memcmp(back, data, 55) == 0);
        gcry_cipher_close(h);

        /* GCM, as WebCore runs it, then with other IVs. */
        const size_t ivlens[] = { 12, 8, 60 };
        for (int v = 0; v < 3; v++) {
            unsigned char longiv[60];
            for (int i = 0; i < 60; i++)
                longiv[i] = i + 1;
            gcry_cipher_open(&h, algo, GCRY_CIPHER_MODE_GCM, GCRY_CIPHER_SECURE);
            gcry_cipher_setkey(h, key, keylen);
            gcry_cipher_setiv(h, longiv, ivlens[v]);
            gcry_cipher_authenticate(h, data, 7);
            gcry_cipher_authenticate(h, data + 7, 13);
            gcry_cipher_final(h);
            gcry_cipher_encrypt(h, buffer, 60, data + 100, 60);
            gcry_cipher_gettag(h, tag, 16);
            printf("  gcm iv %zu ", ivlens[v]);
            hex("", buffer, 60);
            hex("  tag", tag, 16);
            gcry_cipher_close(h);
            gcry_cipher_open(&h, algo, GCRY_CIPHER_MODE_GCM, 0);
            gcry_cipher_setkey(h, key, keylen);
            gcry_cipher_setiv(h, longiv, ivlens[v]);
            gcry_cipher_authenticate(h, data, 20);
            gcry_cipher_final(h);
            gcry_cipher_decrypt(h, back, 60, buffer, 60);
            unsigned char again[16];
            gcry_cipher_gettag(h, again, 12);
            printf("  gcm back %d, tag %d\n", memcmp(back, data + 100, 60) == 0, memcmp(again, tag, 12) == 0);
            status("  checktag", gcry_cipher_checktag(h, tag, 16));
            tag[5] ^= 1;
            status("  checktag changed", gcry_cipher_checktag(h, tag, 16));
            gcry_cipher_close(h);
        }
        /* No AAD, no text. */
        gcry_cipher_open(&h, algo, GCRY_CIPHER_MODE_GCM, 0);
        gcry_cipher_setkey(h, key, keylen);
        gcry_cipher_setiv(h, iv, 12);
        gcry_cipher_gettag(h, tag, 16);
        hex("  gcm empty tag", tag, 16);
        gcry_cipher_close(h);

        /* The key wrap. */
        gcry_cipher_open(&h, algo, GCRY_CIPHER_MODE_AESWRAP, 0);
        gcry_cipher_setkey(h, key, keylen);
        gcry_cipher_final(h);
        for (size_t length = 16; length <= 32; length += 8) {
            status("  wrap", gcry_cipher_encrypt(h, buffer, length + 8, data + 200, length));
            hex("  wrapped", buffer, length + 8);
            status("  unwrap", gcry_cipher_decrypt(h, back, length, buffer, length + 8));
            printf("  unwrapped %d\n", memcmp(back, data + 200, length) == 0);
        }
        buffer[3] ^= 1;
        status("  unwrap changed", gcry_cipher_decrypt(h, back, 24, buffer, 32));
        status("  wrap odd length", gcry_cipher_encrypt(h, buffer, 40, data, 20));
        status("  wrap short output", gcry_cipher_encrypt(h, buffer, 20, data, 16));
        gcry_cipher_close(h);

        gcry_cipher_open(&h, algo, GCRY_CIPHER_MODE_CBC, 0);
        status("  wrong key length", gcry_cipher_setkey(h, key, keylen - 1));
        gcry_cipher_close(h);
    }
    gcry_cipher_hd_t h;
    status("unknown cipher", gcry_cipher_open(&h, 9999, GCRY_CIPHER_MODE_CBC, 0));
}

int main(void)
{
    printf("version %s, at least 1.8 %d, at least 9.0 %d\n", gcry_check_version(NULL), gcry_check_version("1.8.0") != NULL, gcry_check_version("9.0.0") != NULL);
    gcry_control(GCRYCTL_INIT_SECMEM, 16384, 0);
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
    for (int i = 0; i < 1000; i++)
        data[i] = (i * 31 + 7) & 255;
    digests();
    macs();
    pbkdf2();
    ciphers();
    status("no error", 0);
    status("bad signature", gcry_err_make(GPG_ERR_SOURCE_GCRYPT, GPG_ERR_BAD_SIGNATURE));
    return 0;
}
