// The Mlx port's ResourceError (PORT.md, "Network"): WebCore's generic
// error (ResourceErrorBase) with the certificate of a failed TLS
// connection, as the network ports keep it for their error pages. The
// loader strategy makes errors of std.http's and std.tls's failures with
// the functions below (ResourceErrorMlx.cpp).
#pragma once

#include <WebCore/CertificateInfo.h>
#include <WebCore/ResourceErrorBase.h>

namespace WebCore {

class ResourceError : public ResourceErrorBase {
public:
    ResourceError(Type type = Type::Null)
        : ResourceErrorBase(type)
    {
    }

    ResourceError(const String& domain, int errorCode, const URL& failingURL, const String& localizedDescription, Type type = Type::General, IsSanitized isSanitized = IsSanitized::No)
        : ResourceErrorBase(domain, errorCode, failingURL, localizedDescription, type, isSanitized)
    {
    }

    // A transport failure (connection refused, a name not found, a broken
    // stream), with the code the HTTP side gives it.
    static ResourceError transportError(const URL&, int errorCode, const String& description);
    // A TLS connection whose certificate std.tls did not accept.
    static ResourceError tlsError(const URL&, CertificateInfo&&);
    static ResourceError timeoutError(const URL&);

    const CertificateInfo& certificateInfo() const LIFETIME_BOUND { return m_certificateInfo; }
    unsigned tlsErrors() const { return m_certificateInfo.tlsErrors(); }

    ErrorRecoveryMethod errorRecoveryMethod() const { return ErrorRecoveryMethod::NoRecovery; }

    static bool platformCompare(const ResourceError& a, const ResourceError& b) { return a.m_certificateInfo == b.m_certificateInfo; }

private:
    friend class ResourceErrorBase;
    void doPlatformIsolatedCopy(const ResourceError& other) { m_certificateInfo = other.m_certificateInfo.isolatedCopy(); }

    CertificateInfo m_certificateInfo;
};

} // namespace WebCore
