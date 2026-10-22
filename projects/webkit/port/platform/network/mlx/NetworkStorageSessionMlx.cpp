// The Mlx port's NetworkStorageSession: the cookie functions
// NetworkStorageSession.h declares for every port and each port defines
// over its cookie store (NSHTTPCookieStorage, SoupCookieJar, curl's
// CookieJarDB), with the generic constructor (a session and the networking
// context it belongs to) WebCore names for a port without one.
//
// STUB (M6: the cookie jar of std.http, kept by the Mlx side): no cookie
// is kept yet; a cookie set is dropped, a lookup finds none. Nothing loads
// over http before M6, so no cookie can come from a response either.
#include "config.h"
#include "NetworkStorageSession.h"

#include "Cookie.h"
#include "CookieRequestHeaderFieldProxy.h"
#include "CookieStoreGetOptions.h"
#include "HTTPCookieAcceptPolicy.h"
#include "NetworkingContext.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {

NetworkStorageSession::NetworkStorageSession(PAL::SessionID sessionID, NetworkingContext* context)
    : m_sessionID(sessionID)
    , m_context(context)
{
}

NetworkStorageSession::~NetworkStorageSession()
{
    clearCookiesVersionChangeCallbacks();
}

NetworkingContext* NetworkStorageSession::context() const
{
    return m_context.get();
}

HTTPCookieAcceptPolicy NetworkStorageSession::cookieAcceptPolicy() const
{
    return HTTPCookieAcceptPolicy::Never;
}

void NetworkStorageSession::setCookie(const Cookie&)
{
}

void NetworkStorageSession::setCookie(const Cookie&, const URL&, const URL&)
{
}

void NetworkStorageSession::setCookies(const Vector<Cookie>&, const URL&, const URL&)
{
}

void NetworkStorageSession::setCookiesFromDOM(const URL&, const SameSiteInfo&, const URL&, std::optional<FrameIdentifier>, std::optional<PageIdentifier>, ApplyTrackingPrevention, RequiresScriptTrackingPrivacy, const String&, ShouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker) const
{
}

bool NetworkStorageSession::setCookieFromDOM(const URL&, const SameSiteInfo&, const URL&, std::optional<FrameIdentifier>, std::optional<PageIdentifier>, ApplyTrackingPrevention, RequiresScriptTrackingPrivacy, const Cookie&, ShouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker) const
{
    return false;
}

void NetworkStorageSession::deleteCookie(const Cookie&, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void NetworkStorageSession::deleteCookie(const URL&, const URL&, const String&, CompletionHandler<void()>&& completionHandler) const
{
    completionHandler();
}

void NetworkStorageSession::deleteAllCookies(CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void NetworkStorageSession::deleteAllCookiesModifiedSince(WallTime, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void NetworkStorageSession::deleteCookiesForHostnames(std::span<const String>, IncludeHttpOnlyCookies, ScriptWrittenCookiesOnly, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

Vector<Cookie> NetworkStorageSession::getAllCookies()
{
    return { };
}

Vector<Cookie> NetworkStorageSession::getCookies(const URL&)
{
    return { };
}

void NetworkStorageSession::hasCookies(const RegistrableDomain&, CompletionHandler<void(bool)>&& completionHandler) const
{
    completionHandler(false);
}

bool NetworkStorageSession::getRawCookies(const URL&, const SameSiteInfo&, const URL&, std::optional<FrameIdentifier>, std::optional<PageIdentifier>, ApplyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking, Vector<Cookie>& rawCookies) const
{
    rawCookies.clear();
    return true;
}

void NetworkStorageSession::getHostnamesWithCookies(HashSet<String>&)
{
}

std::pair<String, bool> NetworkStorageSession::cookiesForDOM(const URL&, const SameSiteInfo&, const URL&, std::optional<FrameIdentifier>, std::optional<PageIdentifier>, IncludeSecureCookies, ApplyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker) const
{
    return { emptyString(), false };
}

std::optional<Vector<Cookie>> NetworkStorageSession::cookiesForDOMAsVector(const URL&, const SameSiteInfo&, const URL&, std::optional<FrameIdentifier>, std::optional<PageIdentifier>, IncludeSecureCookies, ApplyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker, CookieStoreGetOptions&&) const
{
    return Vector<Cookie> { };
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const URL&, const SameSiteInfo&, const URL&, std::optional<FrameIdentifier>, std::optional<PageIdentifier>, IncludeSecureCookies, ApplyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker) const
{
    return { emptyString(), false };
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const CookieRequestHeaderFieldProxy&) const
{
    return { emptyString(), false };
}

} // namespace WebCore
