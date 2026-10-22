// The Mlx port's ResourceHandle platform functions (PORT.md, "Network"):
// in the single process, WebCore's ResourceLoader runs every load but
// data: URLs through a ResourceHandle, as WebKitLegacy's ports did, so the
// port's network is here. ResourceHandle.cpp defines these only for soup
// and curl (as stubs: those ports load in WebKit2's network process).
//
// STUB (M2): no scheme is loaded yet; every load fails as an invalid URL,
// asynchronously, as WebCore expects of a handle. file: comes with M3
// (std.fs), http and https with M6 (std.http, std.tls, through the Mlx
// side). No HTTP authentication until then either.
#include "config.h"
#include "ResourceHandle.h"

#include "ResourceError.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceResponse.h"

namespace WebCore {

ResourceHandleInternal::~ResourceHandleInternal() = default;

ResourceHandle::~ResourceHandle() = default;

bool ResourceHandle::start()
{
    scheduleFailure(InvalidURLFailure);
    return true;
}

void ResourceHandle::cancel()
{
}

void ResourceHandle::platformSetDefersLoading(bool)
{
}

void ResourceHandle::platformLoadResourceSynchronously(NetworkingContext*, const ResourceRequest& request, StoredCredentialsPolicy, SecurityOrigin*, ResourceError& error, ResourceResponse&, Vector<uint8_t>&)
{
    error = ResourceError::transportError(request.url(), 0, "The port does not load this URL yet"_s);
}

bool ResourceHandle::shouldUseCredentialStorage()
{
    return false;
}

void ResourceHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge&)
{
}

void ResourceHandle::receivedCredential(const AuthenticationChallenge&, const Credential&)
{
}

void ResourceHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&)
{
}

void ResourceHandle::receivedCancellation(const AuthenticationChallenge&)
{
}

void ResourceHandle::receivedRequestToPerformDefaultHandling(const AuthenticationChallenge&)
{
}

void ResourceHandle::receivedChallengeRejection(const AuthenticationChallenge&)
{
}

} // namespace WebCore
