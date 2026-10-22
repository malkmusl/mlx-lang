// The Mlx port's user agent (PORT.md, as UserAgentPlayStation.cpp is
// PlayStation's): a Linux WebKit's, which sites know, with the embedder's
// name and version after it. No per-site quirks yet.
#include "config.h"
#include "UserAgent.h"

#include <wtf/NeverDestroyed.h>
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

static const String& standardUserAgentStatic()
{
    static NeverDestroyed<const String> userAgent("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/60.5 Safari/605.1.15"_s);
    return userAgent;
}

String standardUserAgent(const String& applicationName, const String& applicationVersion)
{
    if (applicationName.isEmpty())
        return standardUserAgentStatic();
    return makeString(standardUserAgentStatic(), ' ', applicationName, '/', applicationVersion.isEmpty() ? "605.1.15"_s : applicationVersion);
}

// The null string: no site needs another user agent.
String standardUserAgentForURL(const URL&)
{
    return String();
}

} // namespace WebCore
