// The Mlx port's CertificateInfo (PORT.md, "Network"): what WebCore keeps
// of a TLS connection's server certificates for a response or an error.
// The chain is kept as DER, leaf first, with the TLS errors std.tls found
// when it checked the chain (std.x509); the HTTP side of the port fills it
// in at M6. Until then responses carry an empty one, as the soup and curl
// ports' synthetic responses do.
#pragma once

#include <WebCore/CertificateSummary.h>
#include <optional>
#include <wtf/Vector.h>

namespace WebCore {

class ResourceError;
class ResourceResponse;

class CertificateInfo {
public:
    // TLS errors, as std.tls reports them (the GTlsCertificateFlags values
    // libsoup's ports use, so error pages read alike).
    enum TLSError : unsigned {
        UnknownCA = 1 << 0,
        BadIdentity = 1 << 1,
        NotActivated = 1 << 2,
        Expired = 1 << 3,
        Revoked = 1 << 4,
        Insecure = 1 << 5,
        GenericError = 1 << 6,
    };

    CertificateInfo() = default;
    CertificateInfo(Vector<Vector<uint8_t>>&& certificateChain, unsigned tlsErrors)
        : m_certificateChain(WTF::move(certificateChain))
        , m_tlsErrors(tlsErrors)
    {
    }

    CertificateInfo isolatedCopy() const { return *this; }

    const Vector<Vector<uint8_t>>& certificateChain() const LIFETIME_BOUND { return m_certificateChain; }
    unsigned tlsErrors() const { return m_tlsErrors; }
    void setTLSErrors(unsigned tlsErrors) { m_tlsErrors = tlsErrors; }

    bool containsNonRootSHA1SignedCertificate() const { return false; }

    // The summary the inspector shows; std.x509's parse of the leaf at M6.
    std::optional<CertificateSummary> summary() const { return std::nullopt; }

    bool isEmpty() const { return m_certificateChain.isEmpty(); }

    friend bool operator==(const CertificateInfo&, const CertificateInfo&) = default;

private:
    Vector<Vector<uint8_t>> m_certificateChain;
    unsigned m_tlsErrors { 0 };
};

} // namespace WebCore
