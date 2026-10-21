# mlx-soup

libsoup 3's C API (3.4) for the C and C++ programs built here, in Mlx:
the HTTP library of WebKit's GLib ports. WebCore's soup platform code
and WebKit's NetworkProcess include `<libsoup/soup.h>` as they would
libsoup's and link `libmlxsoup-3.0.so` in place of `libsoup-3.0`. No
libsoup code is used, nor libsoup's own dependencies (libpsl, nghttp2,
brotli, SQLite, zlib, GnuTLS): the library is this repository's own,
over mlx-glib's GLib, GObject and GIO (compiler/libc/glib, its TLS
backend for https://) and std's HTTP pieces (`std.http.*`), and its
tests print what the system's libsoup prints for the same programs.

The interface is written once, in Mlx: the modules of `api/` declare
the types, constants, functions and macros C sees; `mlx-cheaders`
(compiler/cc) makes the C headers of them into
`mlx-out/include/mlx-soup/libsoup/`. There are no headers written by
hand. Every function the headers declare is defined by the library
(454; the 143 that WebCore and WebKit for WPE call among them).

    ./mlx build soup           the library and its headers (and std.http.psldata)
    tools/check_soup.sh        builds it, checks the headers, runs the tests

## Parts

| File | What |
|---|---|
| `api/` | The interface, a module per header (`soup.mlx` is `libsoup/soup.h`) |
| `soup.mlx` | The library's root (`--plugin --stack-arena`), importing the parts |
| `g.mlx`, `common.mlx` | mlx-glib's functions as the parts call them; types, properties, signals, errors made easy |
| `version.mlx`, `enumtypes.mlx` | `soup-version.h`; the GTypes of libsoup's enums and flags (`soup-enum-types.h`) |
| `headers.mlx` | `soup-message-headers.h`, `soup-headers.h`: headers kept in order with libsoup's names, parsing of requests, responses and header lists (`std.http.message`), parameters, ranges, encodings and expectations (on `std.http.headers`) |
| `body.mlx` | `soup-message-body.h`: bodies of chunks, accumulated or not |
| `misc.mlx` | `soup-date-utils.h`, `soup-form.h`, `soup-uri-utils.h`, `soup-status.h`, `soup-method.h`: dates, forms (and multipart forms), URI helpers, status phrases |
| `cookie.mlx`, `cookiejar.mlx`, `tld.mlx` | `soup-cookie.h`, `soup-cookie-jar*.h`, `soup-tld.h`: SoupCookie in libsoup's layout over `std.http.cookie` (RFC 6265: domains, paths, SameSite, prefixes, expiry), SoupCookieJar as the GObject around `std.http.cookiejar` (libsoup's policies and third-party rules, the text form); the Public Suffix List (`std.http.psl`) |
| `message.mlx`, `metrics.mlx` | `soup-message.h`, `soup-message-metrics.h`: SoupMessage, its signals, properties, flags, header and status handlers, TLS facts, metrics |
| `session.mlx`, `item.mlx` | `soup-session.h`: SoupSession, its queue of items with libsoup's states, the connections it keeps (limits, reuse, idle ones closed), redirects, requeues, synchronous and asynchronous sends, `send_and_read`, `send_and_splice`, preconnecting, features |
| `conn.mlx`, `tls.mlx` | A session's connection: GIO's socket client, the TLS client connection over it (mlx-glib's, by GIO's API), its buffer, reads and writes blocking or not |
| `io.mlx`, `bodystream.mlx`, `inflate.mlx` | HTTP/1.1's message I/O as libsoup's state machines: request writing (Content-Length, chunked, 100-continue), response reading; the body's pollable streams (framing, Content-Encoding undone by std.flate, sniffing, the client stream, the logger's tap, multipart parts) |
| `feature.mlx` | `soup-session-feature.h`: the feature interface and content processors |
| `auth.mlx`, `authmanager.mlx` | `soup-auth.h`, `soup-auth-manager.h`: SoupAuth's objects over `std.http.auth` (Basic and Digest: MD5, MD5-sess, qop auth; the challenges told apart), the manager's cache, retries and pausing for an answer |
| `decoder.mlx`, `sniffer.mlx` | `soup-content-decoder.h`, `soup-content-sniffer.h`: gzip and deflate; MIME sniffing (`std.http.sniff`) |
| `hsts.mlx` | `soup-hsts-*.h`: SoupHSTSPolicy in libsoup's layout and SoupHSTSEnforcer as the GObject around `std.http.hsts` (the policies, which hosts go over https, the file) |
| `logger.mlx` | `soup-logger.h`: SoupLogger's lines as libsoup writes them |
| `multipart.mlx` | `soup-multipart*.h`: whole multipart bodies (cut and written by `std.http.multipart`), and the stream of parts |
| `websocket.mlx`, `wsext.mlx` | `soup-websocket*.h`: the handshake (its keys, accepts, protocols and extensions read by `std.http.websocket`), SoupWebsocketConnection (frames by `std.http.websocket`), the extensions with permessage-deflate (`std.http.websocket.deflate`), the session's WebSocket connections |
| `server.mlx`, `authdomain.mlx`, `pathmap.mlx` | `soup-server*.h`, `soup-auth-domain*.h`: SoupServer, SoupServerMessage, Basic and Digest auth domains; the path map handlers and domains are found in |
| `cache.mlx` | `soup-cache.h` (below) |
| `tests/` | C programs built by mlx-cc against the library, each with what it prints built by gcc against the system's libsoup 3.4 (`NAME.expected`, the exit status last) |

## Tests

`tools/check_soup.sh` builds the library, checks that it defines every
function its headers declare and that `<libsoup/soup.h>` compiles as C
and as C++, and runs the tests (servers in the tests themselves, forked
or in process, on 127.0.0.1): `headers.c`, `misc.c`, `session.c`
(SoupSession and SoupLogger), `cookies.c` (jars and the Public Suffix
List), `hsts.c`, `multipart.c`, `websocket.c` (an echo server in the
test, permessage-deflate), `server.c` (the bytes on the wire, handlers, ranges,
auth domains, a WebSocket upgrade) and `https.c` (TLS 1.3 and 1.2 to
`openssl s_server`, skipped without openssl). `SOUP_COMPARE_SYSTEM=1`
also runs each test built against the system's libsoup and compares it
with the expected output.

## Not as libsoup

- **HTTP/2** is not here: every connection speaks HTTP/1.1 (libsoup
  negotiates HTTP/2 by ALPN with nghttp2).
- **Brotli** is not decoded (std has no decoder): "Accept-Encoding"
  offers gzip and deflate only.
- **Proxies**: the session keeps its proxy resolver but connects
  directly; there is no CONNECT tunnelling.
- **NTLM and Negotiate** are types that take no challenge.
- **SoupCache** is its object and settings only: it neither stores nor
  serves responses (WebKit keeps its own cache).
- **SoupCookieJarDB and SoupHSTSEnforcerDB** keep their contents in
  text files rather than SQLite.
- **TLS servers** are not here (mlx-glib's TLS is a client's):
  `SOUP_SERVER_LISTEN_HTTPS` fails; neither are client certificates.
- `soup_multipart_input_stream_next_part_async` waits on the stream on
  the main context rather than reading in a thread; reads of a part
  never give more than asked for (libsoup's give one byte more past a
  small buffer).
