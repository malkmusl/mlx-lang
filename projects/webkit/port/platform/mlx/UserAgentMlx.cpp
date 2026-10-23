// The Mlx port's user agent (PORT.md, "The user agent"): WebKit's
// compatibility string as the GLib ports build it (UserAgentGLib.cpp's
// buildUserAgentString: "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/
// 605.1.15 (KHTML, like Gecko) Version/60.5 Safari/605.1.15", Version/60.5
// on purpose: sites turn away low and some high Safari versions), then
// the embedder's name and version ("Voyager/0.1") and the engine's token
// (ENGINE_TOKEN). Per-site quirks are WebKit's UserAgentQuirks.cpp
// (platform/glib, compiled as it is), the quirk's string built the same
// way, without the embedder's and the engine's tokens.
#include "config.h"
#include "UserAgent.h"

#include "HTTPParsers.h"
#include "platform/glib/UserAgentQuirks.h"
#include <sys/utsname.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/URL.h>
#include <wtf/glib/ChassisType.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WTF {

// UserAgentQuirks.cpp asks for the chassis (GLib's reads
// /etc/machine-info through GLib): the port runs on desktops.
ChassisType chassisType()
{
    return ChassisType::Desktop;
}

} // namespace WTF

namespace WebCore {

// The engine's token, last in the user agent: the one place it is named
// (Lattice makes it "Lattice/0.0.1").
static constexpr auto ENGINE_TOKEN = "WebKit/2.54.1"_s;

// "Linux x86_64": the system's name and machine, as GLib's.
static const String& platformVersionForUAString()
{
    static NeverDestroyed<const String> version = [] {
        struct utsname name;
        uname(&name);
        return makeString(unsafeSpan(name.sysname), ' ', unsafeSpan(name.machine));
    }();
    return version;
}

// UserAgentGLib.cpp's buildUserAgentString on a desktop, without branding.
static String buildUserAgentString(const UserAgentQuirks& quirks)
{
    StringBuilder uaString;
    uaString.append("Mozilla/5.0 ("_s);

    if (quirks.contains(UserAgentQuirks::NeedsMacintoshPlatform))
        uaString.append(UserAgentQuirks::stringForQuirk(UserAgentQuirks::NeedsMacintoshPlatform));
    else if (quirks.contains(UserAgentQuirks::NeedsAndroidPlatform))
        uaString.append(UserAgentQuirks::stringForQuirk(UserAgentQuirks::NeedsAndroidPlatform));
    else
        uaString.append("X11; "_s, platformVersionForUAString());

    if (quirks.contains(UserAgentQuirks::NeedsFirefoxBrowser)) {
        uaString.append(UserAgentQuirks::stringForQuirk(UserAgentQuirks::NeedsFirefoxBrowser));
        return uaString.toString();
    }

    uaString.append(") AppleWebKit/605.1.15 (KHTML, like Gecko) "_s);
    if (quirks.contains(UserAgentQuirks::NeedsChromeBrowser))
        uaString.append(UserAgentQuirks::stringForQuirk(UserAgentQuirks::NeedsChromeBrowser), ' ');
    else
        uaString.append("Version/60.5 "_s);
    uaString.append("Safari/605.1.15"_s);
    return uaString.toString();
}

static const String& standardUserAgentStatic()
{
    static NeverDestroyed<const String> userAgent(buildUserAgentString(UserAgentQuirks()));
    return userAgent;
}

String standardUserAgent(const String& applicationName, const String& applicationVersion)
{
    String userAgent;
    if (applicationName.isEmpty())
        userAgent = makeString(standardUserAgentStatic(), ' ', ENGINE_TOKEN);
    else
        userAgent = makeString(standardUserAgentStatic(), ' ', applicationName, '/', applicationVersion.isEmpty() ? "605.1.15"_s : applicationVersion, ' ', ENGINE_TOKEN);
    RELEASE_ASSERT_WITH_MESSAGE(isValidHTTPHeaderValue(userAgent), "%s is not a valid user agent header", userAgent.utf8().data());
    return userAgent;
}

// A site's quirk (UserAgentQuirks): its own string; the null string for
// a site that needs none.
String standardUserAgentForURL(const URL& url)
{
    auto quirks = UserAgentQuirks::quirksForURL(url);
    if (quirks.isEmpty())
        return String();
    return buildUserAgentString(quirks);
}

} // namespace WebCore
