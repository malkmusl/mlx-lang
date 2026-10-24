// The Mlx port's public suffixes (PublicSuffixStore's platform functions):
// the Public Suffix List of std.http.psl through the port's Mlx side,
// answering as PublicSuffixStoreSoup.cpp does with libsoup's soup_tld
// functions (which mlx-soup implements over the same list).
#include "config.h"
#include "PublicSuffixStore.h"

#include "port/MlxWebPort.h"
#include <wtf/URL.h>
#include <wtf/text/CString.h>

namespace WebCore {

bool PublicSuffixStore::platformIsPublicSuffix(StringView domain) const
{
    if (domain.isEmpty())
        return false;
    auto lowercase = domain.convertToASCIILowercase().utf8();
    return mlxweb_psl_is_public_suffix(lowercase.data(), lowercase.length());
}

// A host whose suffix is not on the list: its last two labels (as
// PublicSuffixStoreSoup.cpp, for web-platform.test).
static String permissiveTopPrivateDomain(StringView domain)
{
    auto position = domain.length();
    bool foundDot = false;
    while (position-- > 0) {
        if (domain[position] == '.') {
            if (foundDot)
                return domain.substring(position + 1).toString();
            foundDot = true;
        }
    }
    return foundDot ? domain.toString() : String();
}

String PublicSuffixStore::platformTopPrivatelyControlledDomain(StringView domain) const
{
    // As cookies give domains: without their leading dots.
    unsigned position = 0;
    while (position < domain.length() && domain[position] == '.')
        position++;
    auto host = domain.substring(position);
    if (host.isEmpty())
        return String();
    if (URL::hostIsIPAddress(host))
        return domain.toString();

    auto bytes = host.utf8();
    auto start = mlxweb_psl_base_domain(bytes.data(), bytes.length());
    if (start == MLXWEB_PSL_NO_BASE_DOMAIN)
        return permissiveTopPrivateDomain(host);
    if (start < 0)
        return String();
    return String::fromUTF8(std::span { bytes.data() + start, bytes.length() - static_cast<size_t>(start) });
}

} // namespace WebCore
