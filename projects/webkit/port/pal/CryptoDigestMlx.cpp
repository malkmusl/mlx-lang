// PAL's CryptoDigest for the Mlx port (PORT.md, "Compression, WOFF,
// crypto"): the digests of std.sha1 and std.sha2 through the port's Mlx
// side, in place of pal/crypto/gcrypt (libgcrypt) or openssl.
#include "config.h"
#include "CryptoDigest.h"

#include "port/MlxWebPort.h"
#include <wtf/TZoneMallocInlines.h>

namespace PAL::Crypto {

struct CryptoDigestContext {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(CryptoDigestContext);

    explicit CryptoDigestContext(uintptr_t state)
        : state(state)
    {
    }

    ~CryptoDigestContext()
    {
        mlxweb_digest_destroy(state);
    }

    uintptr_t state;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(CryptoDigest);

static uint32_t algorithmIdentifier(CryptoDigest::Algorithm algorithm)
{
    switch (algorithm) {
    case CryptoDigest::Algorithm::SHA_1:
        return MLXWEB_DIGEST_SHA1;
    case CryptoDigest::Algorithm::DEPRECATED_SHA_224:
        return MLXWEB_DIGEST_SHA224;
    case CryptoDigest::Algorithm::SHA_256:
        return MLXWEB_DIGEST_SHA256;
    case CryptoDigest::Algorithm::SHA_384:
        return MLXWEB_DIGEST_SHA384;
    case CryptoDigest::Algorithm::SHA_512:
        return MLXWEB_DIGEST_SHA512;
    }
    return 0;
}

CryptoDigest::CryptoDigest() = default;

CryptoDigest::~CryptoDigest() = default;

std::unique_ptr<CryptoDigest> CryptoDigest::create(CryptoDigest::Algorithm algorithm)
{
    auto state = mlxweb_digest_create(algorithmIdentifier(algorithm));
    RELEASE_ASSERT(state);
    auto digest = makeUnique<CryptoDigest>();
    digest->m_context = makeUnique<CryptoDigestContext>(state);
    return digest;
}

void CryptoDigest::addBytes(std::span<const uint8_t> input)
{
    ASSERT(m_context);
    mlxweb_digest_update(m_context->state, input.data(), input.size());
}

Vector<uint8_t> CryptoDigest::computeHash()
{
    ASSERT(m_context);
    uint8_t buffer[64];
    size_t length = mlxweb_digest_finish(m_context->state, buffer);
    return Vector<uint8_t>(std::span<const uint8_t>(buffer, length));
}

} // namespace PAL::Crypto
