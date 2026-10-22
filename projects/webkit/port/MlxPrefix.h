// The Mlx port's prefix header, included after WebCorePrefix.h in every
// WebCore source (build.mlx's WebCorePort.prefix, as a CMake port's
// WEBKIT_ADD_PREFIX_HEADER): the types WebCore's headers expect a port's
// configuration to name and that WebKit names only for the ports it has.
#pragma once

#ifdef __cplusplus

#include <memory>

namespace WebCore {

// WebCrypto's platform keys: CryptoKeyRSA.h and CryptoKeyEC.h name their
// key containers for Darwin, GCrypt and OpenSSL only. The port's keys are
// its own (MlxCryptoKey, crypto/CryptoMlx.cpp): none is made until
// crypto/mlx's algorithms over std.crypto (M7).
struct MlxCryptoKey;
struct MlxCryptoKeyDeleter {
    void operator()(MlxCryptoKey*) const;
};
using PlatformRSAKey = MlxCryptoKey*;
using PlatformRSAKeyContainer = std::unique_ptr<MlxCryptoKey, MlxCryptoKeyDeleter>;
using PlatformECKeyContainer = std::unique_ptr<MlxCryptoKey, MlxCryptoKeyDeleter>;

} // namespace WebCore

#endif // __cplusplus
