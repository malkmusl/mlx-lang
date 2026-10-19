// The Mlx side of PlatformMlx as the port's C++ sees it: the functions of
// libmlxwebport.so (projects/webkit/port/mlx/webport.mlx), with the C ABI.
// Handles are addresses of state the Mlx side keeps on the C library's
// heap; a function given a null handle does nothing.
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Digests (std.sha1, std.sha2) for PAL's CryptoDigest. The algorithm is
// the digest's length in bits, 1 for SHA-1; create gives 0 for another.
enum {
    MLXWEB_DIGEST_SHA1 = 1,
    MLXWEB_DIGEST_SHA224 = 224,
    MLXWEB_DIGEST_SHA256 = 256,
    MLXWEB_DIGEST_SHA384 = 384,
    MLXWEB_DIGEST_SHA512 = 512,
};
uintptr_t mlxweb_digest_create(uint32_t algorithm);
void mlxweb_digest_update(uintptr_t digest, const void* bytes, size_t length);
// Writes the digest (at most 64 bytes) and gives its length.
size_t mlxweb_digest_finish(uintptr_t digest, uint8_t* out);
void mlxweb_digest_destroy(uintptr_t digest);

#ifdef __cplusplus
}
#endif
