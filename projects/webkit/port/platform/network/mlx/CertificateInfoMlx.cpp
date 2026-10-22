// The Mlx port's CertificateInfo kept on disk (WTF::Persistence: the
// inspector's and the caches' records), what WebCorePersistentCoders.cpp
// has for Cocoa, soup and Windows only: the chain as DER, leaf first, and
// the TLS errors when there is a chain (soup's layout).
#include "config.h"
#include "WebCorePersistentCoders.h"

#include "CertificateInfo.h"
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>

namespace WTF::Persistence {

void Coder<WebCore::CertificateInfo>::encodeForPersistence(Encoder& encoder, const WebCore::CertificateInfo& certificateInfo)
{
    encoder << certificateInfo.certificateChain();
    if (certificateInfo.certificateChain().isEmpty())
        return;
    encoder << static_cast<uint32_t>(certificateInfo.tlsErrors());
}

std::optional<WebCore::CertificateInfo> Coder<WebCore::CertificateInfo>::decodeForPersistence(Decoder& decoder)
{
    std::optional<Vector<Vector<uint8_t>>> certificateChain;
    decoder >> certificateChain;
    if (!certificateChain)
        return std::nullopt;
    if (certificateChain->isEmpty())
        return WebCore::CertificateInfo { };

    std::optional<uint32_t> tlsErrors;
    decoder >> tlsErrors;
    if (!tlsErrors)
        return std::nullopt;
    return WebCore::CertificateInfo { WTF::move(*certificateChain), *tlsErrors };
}

} // namespace WTF::Persistence
