// The Mlx port's WebCrypto (PORT.md, "Crypto"): what the files of
// port/crypto/mlx share. The work is the Mlx side's (port/mlx/crypto.mlx
// over std.crypto, std.aes, std.asn1 and std.random, through
// MlxWebPort.h's mlxweb_crypto_* functions); these files are the
// algorithm classes' platform halves, as crypto/gcrypt and crypto/openssl
// are for their ports.
#pragma once

#include "CryptoAlgorithmIdentifier.h"
#include "port/MlxWebPort.h"
#include <span>
#include <wtf/Vector.h>

namespace WebCore {

// The platform key of CryptoKeyEC and CryptoKeyRSA (MlxPrefix.h names
// the containers): an EC key's curve (its bits), scalar (empty for a
// public key) and point, uncompressed; an RSA key's numbers (n, e, and
// for a private key d, p, q, dp, dq, qi) and the Mlx side's key made of
// them.
struct MlxCryptoKey {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(MlxCryptoKey);

    ~MlxCryptoKey();

    uint32_t curve { 0 };
    Vector<uint8_t> scalar;
    Vector<uint8_t> point;
    Vector<Vector<uint8_t>> numbers;
    uintptr_t rsa { 0 };
};

namespace MlxCrypto {

// MlxWebPort.h's number for a WebCrypto hash; 0 for one it has not.
uint32_t hashIdentifier(CryptoAlgorithmIdentifier);

// The Mlx side's parts (byte strings in order), released with this.
class Parts {
    WTF_MAKE_NONCOPYABLE(Parts);
public:
    Parts();
    explicit Parts(uintptr_t handle)
        : m_handle(handle)
    {
    }
    Parts(Parts&& other)
        : m_handle(std::exchange(other.m_handle, 0))
    {
    }
    ~Parts();

    explicit operator bool() const { return m_handle; }
    uintptr_t handle() const { return m_handle; }
    size_t size() const;
    std::span<const uint8_t> at(size_t) const;
    Vector<uint8_t> vectorAt(size_t index) const { return Vector<uint8_t>(at(index)); }
    void add(std::span<const uint8_t>);

private:
    uintptr_t m_handle { 0 };
};

} // namespace MlxCrypto

} // namespace WebCore
