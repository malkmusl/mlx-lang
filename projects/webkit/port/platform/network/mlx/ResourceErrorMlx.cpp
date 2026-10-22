// The Mlx port's ResourceError functions (ResourceError.h): the errors the
// loader strategy reports for what std.http and std.tls could not do.
#include "config.h"
#include "ResourceError.h"

#include "LocalizedStrings.h"

namespace WebCore {

// The error domains of the port's network errors (what an error page or
// the embedder tells them apart by).
static constexpr ASCIILiteral networkErrorDomain = "MlxNetworkError"_s;
static constexpr ASCIILiteral tlsErrorDomain = "MlxTLSError"_s;

// The codes of the timeout and of a certificate not accepted, among the
// HTTP side's transport codes (which are negative: errno's).
static constexpr int timeoutErrorCode = 1;
static constexpr int certificateErrorCode = 2;

ResourceError ResourceError::transportError(const URL& failingURL, int errorCode, const String& description)
{
    return ResourceError(networkErrorDomain, errorCode, failingURL, description);
}

ResourceError ResourceError::tlsError(const URL& failingURL, CertificateInfo&& certificateInfo)
{
    ResourceError error(tlsErrorDomain, certificateErrorCode, failingURL, "Unacceptable TLS certificate"_s);
    error.m_certificateInfo = WTF::move(certificateInfo);
    return error;
}

ResourceError ResourceError::timeoutError(const URL& failingURL)
{
    return ResourceError(networkErrorDomain, timeoutErrorCode, failingURL, "Request timed out"_s, Type::Timeout);
}

} // namespace WebCore
