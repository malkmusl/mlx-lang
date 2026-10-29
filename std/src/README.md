# Mlx standard library

`std.mlx` is the Stage-1 public facade. It currently exposes the complete
bootstrap foundation, including raw streams, direct text output, typed
`std.fmt.print`/`std.io.printFmt`, files, allocation and collections. Bootstrap
format arguments use explicit constructors such as `std.fmt.string` and
`std.fmt.unsigned`; the normative `anytype` tuple adapter will replace that
surface once Mlx1 can instantiate reflection-heavy generics itself.

Native code uses `std.os.*`; portable POSIX software uses `std.posix`; Wayland
uses `std.wayland` plus generated protocol modules.

`xml.mlx` (`std.xml`) is the allocation-free XML pull tokenizer.
`xml/sax.mlx` (`std.xml.sax`) is a streaming XML 1.0 parser with
namespaces, libxml2's SAX2 push parser in Mlx (its events, errors and
quirks; DTD entities and attribute defaults; UTF-8, UTF-16, Latin-1 and
std.unicode.charmaps' encodings), on `xml/text.mlx`, `xml/encoding.mlx`
and `xml/uri.mlx`; compiler/libc/xml2 is libxml2's C API made of it.
`xml/text.mlx` (`std.xml.text`) also escapes text for XML written out
and checks names (Name, NCName, QName, Nmtoken; test 383).
`wayland.mlx` (`std.wayland`) and `wayland/` contain the protocol parser,
the materializer, the wire format, the Linux transport, and the client and
server runtimes. `wayland/generated/` holds the modules materialized from
`std/protocols/wayland` and must not be edited by hand. Both are imported by
name and are not re-exported from `std.mlx`.

`json.mlx` (`std.json`) is the allocation-free JSON pull tokenizer.
`heap.mlx` (`std.heap`) is the program's heap: `std.heap.allocator()`,
one general-purpose allocator over the page allocator for the whole
process (its state in the tiny-page region every image shares), made on
first use, wrapped in the debug allocator under MLX_LEAK_CHECK (test
390).
`hash.mlx` (`std.hash`) has wyhash (final version 4.3) for bytes,
`hashWord` and `combine`, `autoHash`/`autoEql` derived from a value's
type by reflection, and the `StringContext` the containers take (test
386; aggregates in 391).
`dbus.mlx` (`std.dbus`) is the D-Bus wire format and a client of a bus;
`ipc.mlx` (`std.ipc`) is MLXIPC's native wire format (the same messages,
compactly, with one round trip to connect), its client, and what carries
a message between the two formats (`projects/desktop/ipc`).
`netlink.mlx` (`std.netlink`) speaks netlink: route netlink (links, IPv4
addresses, routes and their change groups) and generic netlink (families
looked up by name, as nl80211).
`sha2.mlx` (`std.sha2`) has SHA-224, SHA-256, SHA-384, SHA-512,
HMAC-SHA256 and PBKDF2-HMAC-SHA256.
`base64.mlx` (`std.base64`) has RFC 4648's base64, base64url, base32,
base32hex and base16, in one call or in pieces, and lower-case hex.
`md5.mlx` (`std.md5`) has MD5 and `blake2.mlx` (`std.blake2`) BLAKE2b,
keyed or not, of any digest length up to 64 bytes.
`random.mlx` (`std.random`) has the kernel's random bytes (getrandom)
and seeded generators: xoshiro256** and, by name, xorshift64*.
`spawn.mlx` (`std.spawn`) starts programs: PATH lookup, the child's
descriptors, directory, session and signals, vfork, a pipe of what it
writes, its exit status. The system calls, the event loop (epoll with
tokens), signal, timer and counter descriptors and Unix sockets with
descriptors passed along are `std.linux`'s (`std/bootstrap/os/linux`).
`pulse.mlx` (`std.pulse`) has PulseAudio's native protocol numbers and
`pulse/tagstruct.mlx` (`std.pulse.tagstruct`) its tagstructs, for MLX
Audio's server and its libpulse.
`pipewire.mlx` (`std.pipewire`) has PipeWire's native protocol numbers
and `pipewire/pod.mlx` (`std.pipewire.pod`) its SPA pods, for MLX
Capture's server and its libpipewire.
`terminal/tty.mlx` (`std.terminal.tty`) is the terminal a program runs
on: whether a descriptor is one, its size, raw mode and the foreground
process group.
`sha1.mlx` (`std.sha1`) has SHA-1, HMAC-SHA1 and PBKDF2-HMAC-SHA1, and
`aes.mlx` (`std.aes`) AES-128 on blocks and the RFC 3394 key wrap: what
WPA2 needs (`tools/check_wifi.sh`); and AES-128/192/256 bitsliced in
constant time, four blocks at once, for TLS.

`mp4.mlx` (`std.mp4`) reads MP4 (ISO base media) files and MSE segments:
the tracks (H.264, H.265, VP9, AV1, AAC, Opus) with their codec
configuration (avcC/hvcC parameter sets, AudioSpecificConfig), a plain
file's samples from its sample tables with seeking to sync samples, a
fragmented file's samples from each `moof`+`mdat` (test 416;
tests/support/mp4_corpus.py makes the files).

`flate/gzip.mlx` (`std.flate.gzip`) reads and writes gzip files in
pieces for Mlx programs, what zlib's gz* functions do (test 385).

`crypto/` (`std.crypto.*`) is what TLS and WebCrypto compute with, all
in Mlx and constant time where a secret is involved: `crypto/hash.mlx`
(SHA-1/2 behind one state, HMAC, HKDF), `crypto/gcm.mlx` (AES-GCM),
`crypto/chacha.mlx` (ChaCha20, Poly1305, their AEAD),
`crypto/bignum.mlx` (Montgomery arithmetic up to 4096 bits),
`crypto/mpi.mlx` (`std.crypto.mpi`: signed integers of any size in an
allocator's memory, division, gcd, inverses, `powm`, primes found; test
384), `crypto/rsa.mlx` (PKCS #1 v1.5 and PSS verification; with a
private key, both signatures, PKCS #1 v1.5 and OAEP encryption and
decryption by the CRT, key generation; test 384), `crypto/ecc.mlx`
(P-256 and P-384: ECDSA, ECDH) and `crypto/curve25519.mlx` (X25519,
Ed25519). `asn1.mlx` (`std.asn1`) reads DER; `x509.mlx` (`std.x509`)
parses certificates, verifies chains against a trust store (the system's
CA bundle) and hostnames (RFC 6125). `tls.mlx` (`std.tls`, with
`tls/record.mlx`, `tls/schedule.mlx`, `tls/wire.mlx`) is a TLS 1.3 and
1.2 client that does no I/O of its own; `tools/check_tls.sh` runs it
against `openssl s_server`, and tests 362 to 364 check the rest against
the standards' vectors.

`http/` (`std.http.*`) is HTTP's pieces that do not depend on how bytes
move, written to answer as libsoup does for mlx-soup
(compiler/libc/soup): `http/headers.mlx` (header lists, tokens,
q-values, parameters with RFC 5987's form, the names libsoup knows),
`http/date.mlx` (HTTP's and cookies' date forms), `http/cookie.mlx`
(cookies, RFC 6265: read from Set-Cookie with libsoup's rules, written
back, matched) and `http/cookiejar.mlx` (the jar: domains, accept
policies and third parties by the Public Suffix List, SameSite, the
cookies.txt file; test 377), `http/status.mlx` (reason phrases),
`http/form.mlx` (application/x-www-form-urlencoded),
`http/chunked.mlx` (the chunked coding's size lines),
`http/sniff.mlx` (MIME sniffing, the WHATWG standard's rules),
`http/mime.mlx` (`std.http.mime`: a local file's media type from its
name's extension or first bytes, as GIO guesses; test 383),
`http/websocket.mlx` (RFC 6455's keys, accepts, protocols and extensions
of the handshake, frame headers, masking, UTF-8) with
`http/websocket/deflate.mlx` (permessage-deflate, RFC 7692),
`http/multipart.mlx` (multipart bodies cut and written as libsoup does;
test 379), `http/message.mlx` (request and status lines, header lines
folded, bodies' framing, the rules of redirects; test 380 with `uri.mlx`,
`std.uri`: RFC 3986's parts, references resolved, percent-coding),
`http/client.mlx` (`std.http.client`: an HTTP/1.1 client with sessions,
kept connections, redirects, Basic and Digest auth, cookies, HSTS, gzip,
https on std.tls; test 382 against a server in a child process) on
`net.mlx` (`std.net`: TCP over IPv4 and IPv6, addresses, `std.net.dns`'s
resolver over /etc/hosts and /etc/resolv.conf; test 381),
`http/auth.mlx` (RFC 7235's challenges told apart, Basic tokens, Digest
responses and Authorization values; test 376), `http/hsts.mlx` (RFC
6797: Strict-Transport-Security read strictly, an enforcer's policies
and which hosts go over https, their file; test 378)
and `http/psl.mlx` with `http/psldata.mlx` (the Public Suffix List, made
by `./mlx build http-psl` from `std/registry/publicsuffix`). Tests 370
and 371 check them.

`sql.mlx` (`std.sql`) with `sql/` is an SQLite-compatible database
engine: SQLite's file format (B-tree pages, records, overflow chains,
the freelist), its rollback journal and file locks, so that SQLite and
it read each other's files; SQL as SQLite parses, plans and evaluates it
(values, affinities, collations, constraints, foreign keys, savepoints,
the built-in functions, the authorizer); and connections and prepared
statements as SQLite's C API presents them. mlx-sqlite3
(compiler/libc/sqlite3) is that C API over it, for WebKit's databases;
test 372 and `tools/check_sqlite.sh` check it.

`crypt.mlx` (`std.crypt`) checks a password against the hashes of
`/etc/shadow`, as crypt(3) writes them: `crypt/yescrypt.mlx` for the
yescrypt ($y$, Ubuntu's default) and scrypt ($7$) ones,
`crypt/shacrypt.mlx` for SHA-256 and SHA-512 ($5$, $6$);
`tools/check_crypt.sh` compares it with the system's crypt(3).
`vulkan.mlx` (`std.vulkan`) is the Vulkan API, materialized from
`std/registry/vulkan/vk.xml` by `vulkan/registry.mlx` and
`vulkan/materialize.mlx` — do not edit it by hand. `vulkan/loader.mlx`
(`std.vulkan.loader`) finds and opens drivers without a C loader, and
`vulkan/icd.mlx` (`std.vulkan.icd`) is the driver side of that interface.
`spirv/core.mlx` (`std.spirv.core`, generated from
`std/registry/spirv/spirv.core.grammar.json`), `spirv/builder.mlx` and
`spirv/module.mlx` write, read and validate SPIR-V, and
`spirv/compute.mlx` (`std.spirv.compute`) declares compute kernels.
`gpu.mlx` (`std.gpu`) runs compute kernels on pixel buffers (one device and
queue, dma-buf and host-memory sharing), with `gpu/shaders.mlx`
(`std.gpu.shaders`), `gpu/text.mlx` (`std.gpu.text`) and
`gpu/swapchain.mlx` (`std.gpu.swapchain`). See `docs/reference/vulkan.md`.

`truetype.mlx` (`std.truetype`) and `truetype/` are the font engine:
TrueType and OpenType (CFF, CFF2) fonts, variable and color fonts, read
and rendered as FreeType 2.13 does it; its `Font`, `Atlas` and `Run` lay
out and draw lines of text. FreeType's C API (`compiler/libc/freetype`)
is a layer over it; see `docs/reference/truetype.md`.

`ui.mlx` (`std.ui`) has layout building blocks for pixel user interfaces:
rectangles, insets, alignment, containers, stacks and reserved bars; see
`docs/reference/ui.md`.

`crash.mlx` (`std.crash`) reports a program's crash (signal, function,
callers from the compiler's symbol table) and keeps it in
`$XDG_STATE_HOME/mlx/crashes.log`; `wayland/crash.mlx`
(`std.wayland.crash`) treats a Wayland client's failed connection like a
crash. `timezone.mlx` (`std.timezone`) gives local time from `$TZ` or
`/etc/localtime` (TZif files and POSIX rules) and the civil calendar. See
`docs/reference/stdlib.md`.

`unicode.mlx` (`std.unicode`) with `unicode/` is Unicode and CLDR's
locale data in Mlx: `std.unicode.utf` (UTF-8 and UTF-16, no tables),
`.properties`, `.casing`, `.normalize`, `.breaks`, `.collation` (with
`collation/`), `.idna`, `.charsets`, `.locale` (with `locale/`),
`.calendar` (with `calendar/`) and `.format` (`format/`: number, plural,
date, list, reltime), each with a native API (UTF-8 slices, allocators,
`unicode.Error`) beside ICU's functions, which `compiler/libc/icu` exports
to C as `libmlxunicode.so`. `timezone/database.mlx`
(`std.timezone.database`) is the IANA time zone database built in
(`timezone/tzdata.mlx`). Their tables (`unicode/tables.mlx`,
`unicode/locale/data.mlx`, `unicode/collation/rules.mlx`,
`timezone/tzdata.mlx`) are generated from `std/registry/unicode` and
`std/registry/tz` by the generators there (`./mlx build unicode`); do not
edit them by hand. See `docs/reference/unicode.md`.

`ui/appearance.mlx` (`std.ui.appearance`) follows the desktop's colour
scheme (`~/.config/mlx/appearance.conf`) for every window and panel, and
`ui/theme.mlx`'s `at` gives the look for it, gliding when it is switched.

`ui.mlx` (`std.ui`) lays out rectangles; `ui/canvas.mlx`, `ui/text.mlx`,
`ui/gpu_canvas.mlx` and `ui/gpu_upload.mlx` draw on the CPU or the GPU;
`ui/view.mlx` with `ui/controls.mlx`, `ui/field.mlx`, `ui/scroll.mlx`,
`ui/lists.mlx`, `ui/tabs.mlx`, `ui/popup.mlx`, `ui/glyphs.mlx`,
`ui/theme.mlx` and `ui/keys.mlx` are widgets for any platform;
`ui/app.mlx` is what a platform gives an app of them (events, `feed`).
`ui/host.linux.mlx` (a Wayland window, `ui/wayland/`) and
`ui/host.android.mlx` (a NativeActivity) are `std.ui.host`: one API, so an
app's source is the same on both (the compiler takes the target's file).
See `docs/reference/ui.md`.

`xkb.mlx` (`std.xkb`) is the keymap model: `xkb/parse.mlx` reads XKB
keymaps and source files, `xkb/compile.mlx` makes keymaps from the XKB
data through `xkb/rules.mlx` and `xkb/merge.mlx`, `xkb/write.mlx` writes
them, `xkb/state.mlx` (`std.xkb.state`) follows keys and types with them
and `xkb/keysym.mlx` (`std.xkb.keysym`) names keysyms (the table
`xkb/keysyms.mlx` is generated by `tools/materialize_xkb_keysyms.py`); no
libxkbcommon.

`std.path`, `std.mode`, `std.parse`, `std.shell` and `std.calendar` provide
coreutils-grade building blocks (path manipulation/canonicalization, chmod-style
permission parsing, sized-number/duration parsing, POSIX-ish word splitting and
calendar/timestamp parsing) that were generalized out of `projects/coreutils`.
