// The profile of the Mlx port's embedding (PORT.md, M7): where what a
// browser keeps between runs is kept, and the providers that keep it,
// which every page of the process shares.
//
// DATA (Voyager's: $XDG_DATA_HOME/voyager, ~/.local/share/voyager):
//   LocalStorage/   localStorage, a database an origin (MlxStorage.h)
//   IndexedDB/      IndexedDB's databases (MlxIndexedDB.h)
//   cookies.sqlite  the cookie jar's cookies that outlive the session
//                   (port/mlx/cookies.mlx: libsoup's moz_cookies table)
// CACHE (Voyager's: $XDG_CACHE_HOME/voyager, ~/.cache/voyager):
//   http/           the HTTP cache (port/mlx/httpcache.mlx)
//
// With no profile opened (or empty paths) nothing is kept: local storage
// and IndexedDB are in memory, cookies too, and no response is cached on
// disk. sessionStorage is never kept (a tab's, in memory).
#pragma once

#include <wtf/Forward.h>

namespace WebCore {

class PageConfiguration;
class Page;

namespace MlxProfile {

// Opens the profile: its directories made when missing, the cookie jar
// read, the HTTP cache's index read. Before the first page is made.
WEBCORE_EXPORT void open(const String& dataDirectory, const String& cacheDirectory);

WEBCORE_EXPORT const String& dataDirectory();
WEBCORE_EXPORT const String& cacheDirectory();

// The page configuration's providers of the profile: the cookie jar, web
// sockets, web storage and IndexedDB.
WEBCORE_EXPORT void configure(PageConfiguration&);

// A page opened by another (window.open, a link's target): it starts
// with a copy of the opener's session storage, as the HTML standard has
// a new browsing context's.
WEBCORE_EXPORT void pageOpened(Page& opener, Page&);

// A page closed (its session storage forgotten).
WEBCORE_EXPORT void pageClosed(Page&);

// Writes what is pending (local storage's changes; the IndexedDB server's
// work done) and closes what is open: before the process exits.
WEBCORE_EXPORT void close();

} // namespace MlxProfile

} // namespace WebCore
