// The Mlx port's AuthenticationChallenge (PORT.md, "Network"): WebCore's
// generic challenge (AuthenticationChallengeBase). HTTP authentication is
// the loader strategy's (std.http's Basic and Digest, M6); WebCore only
// carries the challenge to the embedder's client, which answers with a
// credential.
#pragma once

#include <WebCore/AuthenticationChallengeBase.h>
#include <WebCore/AuthenticationClient.h>

namespace WebCore {

class AuthenticationChallenge final : public AuthenticationChallengeBase {
public:
    AuthenticationChallenge()
    {
    }

    AuthenticationChallenge(const ProtectionSpace& protectionSpace, const Credential& proposedCredential, unsigned previousFailureCount, const ResourceResponse& response, const ResourceError& error)
        : AuthenticationChallengeBase(protectionSpace, proposedCredential, previousFailureCount, response, error)
    {
    }

    AuthenticationClient* authenticationClient() const { return m_authenticationClient.get(); }
    void setAuthenticationClient(AuthenticationClient* client) { m_authenticationClient = client; }

private:
    friend class AuthenticationChallengeBase;

    RefPtr<AuthenticationClient> m_authenticationClient;
};

} // namespace WebCore
