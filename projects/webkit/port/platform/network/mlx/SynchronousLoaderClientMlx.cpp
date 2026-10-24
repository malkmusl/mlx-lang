// The Mlx port's part of SynchronousLoaderClient (a synchronous load:
// sync XHR, a module script's import): what SynchronousLoaderClient.cpp
// defines for soup and curl only.
//
// STUB (M6: std.http's loads; an authentication challenge answered by
// the embedder's credentials): ResourceHandleMlx loads nothing yet, so
// neither function is reached, as in soup's and curl's ports.
#include "config.h"
#include "SynchronousLoaderClient.h"

#include "AuthenticationChallenge.h"
#include "ResourceError.h"

namespace WebCore {

void SynchronousLoaderClient::didReceiveAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

ResourceError SynchronousLoaderClient::platformBadResponseError()
{
    ASSERT_NOT_REACHED();
    return { };
}

} // namespace WebCore
