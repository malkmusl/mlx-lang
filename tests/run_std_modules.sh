#!/usr/bin/env bash
# The small standard modules with the self-hosted compiler, each against
# its standard's own test vectors:
#   tests/347_base64_runtime.mlx        std.base64 (RFC 4648 section 10)
#   tests/348_md5_blake2_runtime.mlx    std.md5 (RFC 1321 A.5) and
#                                       std.blake2 (RFC 7693, the KAT)
#   tests/349_sha2_runtime.mlx          std.sha2's SHA-224 and SHA-384
#                                       (FIPS 180-4's examples)
#   tests/350_math_helpers_runtime.mlx  std.math's min, max, clamp,
#                                       absolute, sqrt and smoothstep
#   tests/351_random_runtime.mlx        std.random (xoshiro256**'s and
#                                       splitmix64's reference outputs)
#   tests/352_terminal_tty_runtime.mlx  std.terminal.tty on a pty of its own
#   tests/353_fs_whole_files_runtime.mlx
#                                       std.fs's whole files, folders made
#                                       on the way and executablePath
#   tests/354_xdg_runtime.mlx           std.xdg (the base directories over
#                                       environments of its own)
#   tests/355_passwd_runtime.mlx        std.passwd (accounts and groups, by
#                                       name and by number)
#   tests/356_conf_runtime.mlx          std.conf ("key = value" files)
#   tests/357_glob_runtime.mlx          std.glob (fnmatch patterns)
#   tests/358_timezone_names_runtime.mlx
#                                       std.timezone's abbreviations and
#                                       loadSetting
#   tests/359_linux_descriptors_runtime.mlx
#                                       std.linux: pipes, the event loop,
#                                       signal, timer and counter
#                                       descriptors, poll, Unix sockets,
#                                       descriptors passed, children
#   tests/360_spawn_runtime.mlx         std.spawn (PATH, output captured,
#                                       descriptors, vfork, detached)
#   tests/361_dbus_keep_signals_runtime.mlx
#                                       std.dbus's client (signals kept
#                                       across a call, bus addresses) and
#                                       std.spawn.find
#   tests/362_crypto_symmetric_runtime.mlx
#                                       std.crypto's HMAC, HKDF, AES-GCM,
#                                       ChaCha20-Poly1305 and std.aes's
#                                       constant-time AES (RFC 4231, 5869,
#                                       8439, FIPS 197, the GCM test cases)
#   tests/363_crypto_public_key_runtime.mlx
#                                       std.crypto's X25519, Ed25519, ECDSA
#                                       and ECDH on P-256 and P-384, RSA
#                                       PKCS #1 v1.5 and PSS (RFC 7748,
#                                       8032; OpenSSL's signatures)
#   tests/364_x509_runtime.mlx          std.asn1 and std.x509: fields,
#                                       names, hostnames (RFC 6125),
#                                       chains, the system's CA bundle
#   tests/365_png_decode_runtime.mlx    std.png's decoder: every colour type,
#                                       interlacing, transparency, damage
#   tests/366_jpeg_decode_runtime.mlx   std.jpeg's decoder: sampling,
#                                       progressive, grey, CMYK, restarts,
#                                       EXIF orientation, damage
#   tests/367_webp_decode_runtime.mlx   std.webp's decoders: lossy, lossless,
#                                       alpha, animation (demuxer), rows
#                                       done incrementally, damage; the
#                                       encoder's round trip
#   tests/370_http_headers_runtime.mlx  std.http.headers (libsoup's header
#                                       lists, q-values and parameters)
#   tests/371_http_protocol_runtime.mlx
#                                       std.http.websocket (keys, frame
#                                       headers, masks, UTF-8), chunk
#                                       sizes, the Public Suffix List,
#                                       MIME sniffing
#   tests/372_colorprofile_runtime.mlx  std.colorprofile: transforms whose
#                                       pixels are Little CMS's (matrix-
#                                       shaper, grey, 16-bit grid,
#                                       perceptual with black point
#                                       compensation), the sRGB curve
#
#   tests/run_std_modules.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
failures=0

run() {
    local test=$1 words=$2
    local status=0
    if ! "$compiler" --quiet "tests/$test.mlx" -o "$work/$test"; then
        echo "FAIL $test did not compile"
        failures=$((failures + 1))
        return
    fi
    "$work/$test" || status=$?
    if [[ $status -eq 13 ]]; then
        echo "ok   $test: $words"
    else
        echo "FAIL $test: check $status failed"
        failures=$((failures + 1))
    fi
}
run 347_base64_runtime "base64, base64url, base32, base32hex, base16 and hex"
run 348_md5_blake2_runtime "MD5 and BLAKE2b, plain, keyed and in pieces"
run 349_sha2_runtime "SHA-224 and SHA-384 (and SHA-1, SHA-256, SHA-512)"
run 350_math_helpers_runtime "min, max, clamp, absolute, sqrt and smoothstep"
run 351_random_runtime "getrandom, xoshiro256**, splitmix64 and xorshift64*"
run 352_terminal_tty_runtime "a pty's size, raw mode and process groups"
run 353_fs_whole_files_runtime "files read and written whole, mkdir -p and the program's path"
run 354_xdg_runtime "the XDG base directories, their defaults and lists"
run 355_passwd_runtime "accounts and groups by name and number, malformed lines passed over"
run 356_conf_runtime "settings lines, comments, groups and switches"
run 357_glob_runtime "stars, sets, ranges, escapes and quoted bytes"
run 358_timezone_names_runtime "zone abbreviations from rules and TZif files"
run 359_linux_descriptors_runtime "pipes, epoll tokens, signalfd, timerfd, eventfd, poll, Unix sockets, SCM_RIGHTS, wait4"
run 360_spawn_runtime "PATH lookup, captured output, exit statuses, moved descriptors, vfork, detach"
run 361_dbus_keep_signals_runtime "a client against a child bus: a signal kept across a call with its descriptor; bus addresses; PATH lookup"
run 362_crypto_symmetric_runtime "HMAC, HKDF, AES-128/192/256, AES-GCM, ChaCha20, Poly1305 and their AEAD"
run 363_crypto_public_key_runtime "X25519, Ed25519, ECDSA and ECDH on P-256, P-384 and P-521, RSA PKCS #1 v1.5 and PSS"
run 364_x509_runtime "DER, certificates' fields and names, hostnames, chains to an anchor, the CA bundle"
run 365_png_decode_runtime "PNG images of each colour type, interlaced and transparent, decoded to RGBA; damaged ones refused"
run 366_jpeg_decode_runtime "JPEG images (4:2:0, progressive 4:2:2, grey, CMYK, restarts) decoded as libjpeg decodes them; EXIF orientation; damage"
run 367_webp_decode_runtime "WebP images (lossy, lossless, alpha, animated) decoded as libwebp decodes them; incremental rows; damage; encoding"
run 370_http_headers_runtime "HTTP header lists, q-values, parameters (RFC 5987) read and written as libsoup does"
run 371_http_protocol_runtime "WebSocket keys, frames, masks and UTF-8; chunk sizes; public suffixes; MIME sniffing"
run 372_colorprofile_runtime "ICC transforms as Little CMS makes them: BGRA to sRGB, grey, 16-bit, perceptual; the sRGB curve"

if [[ $failures -ne 0 ]]; then
    echo "$failures standard module checks failed"
    exit 1
fi
echo "all standard module checks passed"
