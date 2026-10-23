// The Mlx port's NetworkStorageSession: the cookie functions
// NetworkStorageSession.h declares for every port and each port defines
// over its cookie store (NSHTTPCookieStorage, SoupCookieJar, curl's
// CookieJarDB), with the generic constructor (a session and the networking
// context it belongs to) WebCore names for a port without one.
//
// The cookies are the Mlx side's jar (port/mlx/http.mlx: std.http's
// cookiejar, which the client sends from and takes Set-Cookie into),
// reached on the network queue (MlxNetwork.h) the jar belongs to:
// document.cookie reads and writes it, the Cookie header of a request is
// the client's. STUB (later: the cookie lists of the inspector and the
// cookie store API, deleting cookies): those see no cookie.
#include "config.h"
#include "NetworkStorageSession.h"

#include "Cookie.h"
#include "CookieRequestHeaderFieldProxy.h"
#include "CookieStoreGetOptions.h"
#include "HTTPCookieAcceptPolicy.h"
#include "NetworkingContext.h"
#include "port/MlxWebPort.h"
#include "port/embed/MlxNetwork.h"
#include <wtf/CompletionHandler.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/CString.h>

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
    return HTTPCookieAcceptPolicy::AlwaysAccept;
}

// The jar's cookies for URL (FOR_HTTP false: none of the HttpOnly ones),
// as a Cookie header lists them.
static String jarCookies(const URL& url, bool forHttp)
{
    String cookies;
    CString target = url.string().utf8();
    MlxNetwork::queue().dispatchSync([&] {
        char* text = mlxweb_cookies_for(target.data(), forHttp ? 1 : 0);
        if (text) {
            cookies = String::fromUTF8(text);
            mlxweb_text_release(text);
        }
    });
    return cookies.isNull() ? emptyString() : cookies;
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

void NetworkStorageSession::setCookiesFromDOM(const URL&, const SameSiteInfo&, const URL& url, std::optional<FrameIdentifier>, std::optional<PageIdentifier>, ApplyTrackingPrevention, RequiresScriptTrackingPrivacy, const String& cookieString, ShouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker) const
{
    CString target = url.string().utf8();
    CString line = cookieString.utf8();
    MlxNetwork::queue().dispatchSync([&] {
        mlxweb_cookie_set(target.data(), line.data());
    });
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

std::pair<String, bool> NetworkStorageSession::cookiesForDOM(const URL&, const SameSiteInfo&, const URL& url, std::optional<FrameIdentifier>, std::optional<PageIdentifier>, IncludeSecureCookies, ApplyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker) const
{
    return { jarCookies(url, false), false };
}

std::optional<Vector<Cookie>> NetworkStorageSession::cookiesForDOMAsVector(const URL&, const SameSiteInfo&, const URL&, std::optional<FrameIdentifier>, std::optional<PageIdentifier>, IncludeSecureCookies, ApplyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker, CookieStoreGetOptions&&) const
{
    return Vector<Cookie> { };
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const URL&, const SameSiteInfo&, const URL& url, std::optional<FrameIdentifier>, std::optional<PageIdentifier>, IncludeSecureCookies, ApplyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker) const
{
    return { jarCookies(url, true), false };
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const CookieRequestHeaderFieldProxy&) const
{
    return { emptyString(), false };
}

} // namespace WebCore
