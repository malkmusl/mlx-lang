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
#   tests/411_xdg_open_runtime.mlx      std.xdg.open (default applications,
#                                       desktop entries' Exec lines)
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
#   tests/416_mp4_demux_runtime.mlx     std.mp4: plain and fragmented MP4 files,
#                                       their tracks, samples and codec data
#   tests/417_aac_decode_runtime.mlx    std.aac: AAC-LC from MP4 and ADTS against
#                                       the WAV it was encoded from
#   tests/418_h264_stream_runtime.mlx   std.h264: parameter sets, pictures, the
#                                       DPB's output order (WebKit's test.mp4)
#   tests/422_h264_cpu_decode_runtime.mlx
#                                       std.h264.decode: WebKit's test.mp4
#                                       decoded on the CPU, bit-exact
#   tests/423_h265_stream_runtime.mlx   std.h265: parameter sets, pictures, the
#                                       reference picture sets, output order
#   tests/424_h265_cpu_decode_runtime.mlx
#                                       std.h265.decode: a Main 10 stream
#                                       decoded on the CPU, bit-exact, and
#                                       on a pool of threads
#   tests/425_vp9_header_runtime.mlx    std.vp9: uncompressed headers, the
#                                       reference slots, superframes
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
#   tests/372_sql_runtime.mlx           std.sql: SQLite's number texts,
#                                       WebKit's SQL parsed, statements
#                                       run, a file written and read back
#   tests/373_colorprofile_runtime.mlx  std.colorprofile: transforms whose
#                                       pixels are Little CMS's (matrix-
#                                       shaper, grey, 16-bit grid,
#                                       perceptual with black point
#                                       compensation), the sRGB curve
#   tests/374_xml_sax_runtime.mlx       std.xml.sax: libxml2's SAX2 events
#                                       and errors (namespaces, DTD
#                                       entities and defaults, chunks,
#                                       UTF-16, Latin-1, content apart,
#                                       an entity loop)
#   tests/375_fontconfig_runtime.mlx    std.fontconfig: case folding, names,
#                                       languages; fonts of the tree read,
#                                       cached, matched, sorted, listed
#   tests/392_demangle_runtime.mlx     std.demangle: C++ names as c++filt
#                                       writes them, and mlx-cc's own
#   tests/413_git_runtime.mlx          std.git: names, deltas, commits, the
#                                       checkout's HEAD and count
#                                       (tools/check_git_reader.sh: against
#                                       git on repositories it makes)
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
run 411_xdg_open_runtime "a file opened with the desktop's application for it: its type, mimeapps.list's default, a desktop entry's MimeType, its Exec line, run"
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
run 372_sql_runtime "SQLite's reals and numbers, WebKit's SQL parsed, GROUP BY, collations, CHECK, a file of 3000 rows read back"
run 373_colorprofile_runtime "ICC transforms as Little CMS makes them: BGRA to sRGB, grey, 16-bit, perceptual; the sRGB curve"
run 374_xml_sax_runtime "XML parsed as libxml2's push parser parses it: SAX2 events, entities, errors, chunks, encodings"
run 375_fontconfig_runtime "Fontconfig's case folding, names and substitutions; fonts read, cached, matched, sorted and listed as Fontconfig 2.15 does"
run 376_http_auth_runtime "HTTP authentication: challenges told apart, Basic tokens, Digest responses (RFC 2617's example) and Authorization values"
run 377_http_cookies_runtime "cookies read with libsoup's rules, written back and matched; a jar's policies, SameSite, prefixes, expiry, third parties and cookies.txt"
run 378_http_hsts_runtime "HSTS: Strict-Transport-Security read strictly, policies and their expiry, an enforcer's hosts and subdomains, its changes and file"
run 379_http_multipart_websocket_runtime "multipart bodies cut and written as libsoup does; WebSocket keys, accepts, protocols, extensions and permessage-deflate messages"
run 380_uri_message_runtime "URIs split, resolved (RFC 3986's examples), normalized and coded; HTTP heads read as libsoup reads them, written, framed, redirected"
run 381_net_runtime "IPv4 and IPv6 addresses, socket addresses, a TCP connection on the loopback, DNS queries and answers, names answered without a server"
run 461_logserve_runtime "logserve without the network: a request's head read (waited for, GET and HEAD, other methods, no HTTP/1, over the limit), the fixed pages a path names, the kernel log's gpu: lines, a saved log that is the log's beginning, a link's record in words, the text that grows"
run 382_http_client_runtime "an HTTP/1.1 session against a server in a child process: kept connections, cookies, redirects, POST, chunked, gzip, HEAD, Digest auth, errors"
run 383_mime_xml_text_runtime "media types by a file's extension and first bytes as GIO guesses; XML text escaped four ways and names checked as Name, NCName, QName, Nmtoken"
run 384_crypto_mpi_rsa_runtime "integers of any size: hex, signed division both ways, powm, gcd, inverses, primes; RSA private keys: PKCS #1 and PSS signatures, OAEP and PKCS #1 encryption against Python's values, a key generated"
run 385_flate_gzip_runtime "gzip files written and read in pieces, two members as one, a plain file as it is, trailing data left, damaged and truncated members refused"
run 386_hash_runtime "wyhash against its reference's vectors, words combined, autoHash and autoEql by type (integers, bools, enums, floats, pointers, optionals, slices, arrays), the string context"
run 390_heap_runtime "the program's heap: one wherever it is asked for, in the shared region, blocks and their footprints, a GiB reserved, the debug allocator over it under the leak check, deinit"
run 392_demangle_runtime "C++ names as c++filt writes them (g++'s and clang++'s manglings) and mlx-cc's own spellings; names it cannot read; a cut text"
run 410_crypto_rsa_sizes_runtime "RSA keys of any size: a 64-bit modulus, 512 bits made and verified in its frame, 4160 bits (numbers and work on the heap) signed, verified and OAEP both ways, d alone, three primes; out of range refused"
run 431_tar_runtime "tar headers written and read back: short and split names, a directory's slash, a link's target, the checksum, a name that cannot be stored; an archive in memory"
run 432_local_module_alias_runtime "a module bound to a local constant in a body: calls, values and types through it"
run 433_narrow_integers_runtime "narrow integers canonical after left shifts, complements and narrowing casts"
run 434_packed_struct_runtime "packed structs: fields at bit offsets in one word of 1, 2, 4 or 8 bytes, read and written with shifts and masks, signed fields with their sign, literals and constants, @bitCast to and from the word's integer"
run 435_float_negative_zero_runtime "a float's negation flips its sign bit: -(0.0) is -0.0, in f32 and f64"
run 436_function_type_in_builtin_runtime "a function type where a builtin takes a type: fn, ?fn and [*]?fn in @ptrCast"
run 437_atomic_optional_pointer_runtime "atomics on an optional pointer: load, store and exchange of ?*T and ?[*]T"
run 438_global_address_initializers_runtime "module-level vars initialized with floats and addresses: functions, strings, another global's element; one address for a table wherever it is named"
run 439_function_before_declaration_runtime "a function named in a module-level initializer above its declaration: a table and a pointer before their functions"
run 440_endless_loop_ends_function_runtime "a function ending with a while true that no break ends; breaks of inner and outer loops, a continue that names it"
run 442_function_alignment_runtime "functions begin at multiples of 16: an address through an integer and back, its low bits free"
run 443_function_type_parameters_runtime "a function type with a function type or a tuple among its parameters keeps its own parameters"
run 444_statement_after_postfix_runtime "a line ending in .* or .? ends its statement: the next line's ( begins one"
run 445_constant_address_runtime "the address of a module-level constant, of this module or an imported one: a copy of its value in the function"
run 448_power_of_two_division_runtime "a division by a constant power of two and its remainder, shifted and masked: every integer type and divisor against the divisor read at run time, signed ones toward zero"
run 449_power_of_two_multiplication_runtime "a product by a constant power of two, shifted: every integer type and factor against the factor read at run time, a checked product that does not fit still trapping, elements reached by an index"
run 450_mem_block_moves_runtime "std's mem.copy and mem.set by blocks: the bytes a byte loop gives at the lengths where words, half-KiB blocks and pages hand over, sixteen alignments, overlapping copies both ways"
run 451_kernel_bytes_runtime "the kernel's Bytes.fill and Bytes.copy (block moves, words, bytes) built for the host: the bytes a byte loop gives at the hand-over lengths, sixteen alignments"
run 453_splash_squeeze_runtime "ignition's frame brought to a narrower display's width (a picture drawn for the screen's shape): means by hand and against whole numbers, parts as the whole, nothing outside their reach"
run 457_block_map_runtime "brixfs's map of the blocks in use (a bit a block by words, with the count) against a loop over single bits: runs marked and let go at chance, the first free block, what a second map keeps; the runs of a page"
run 458_commit_frees_replaced_runtime "how brixfs's store finds its free blocks, on a model with the store's map: commits that let go what they replaced and mark the rewritten files again, snapshots kept, the disk run full: after each step the map is what a walk over every root marks"
run 441_types_across_import_ring_runtime "records of three modules that import one another naming one another: pointers, records inside, a function type, sizes"
run 413_git_runtime "git object names, deltas applied, commits parsed, this checkout's HEAD read and its commits counted, a directory without a repository"
run 418_h264_stream_runtime "H.264 parameter sets into the Vulkan video structures, every access unit of WebKit's test.mp4 as a picture with its references and slices, the DPB's output order against the file's presentation times, Annex B framing, a cut SPS refused"
run 422_h264_cpu_decode_runtime "WebKit's test.mp4 (Main profile, B frames) decoded on the CPU through std.mp4 and std.h264: every picture, in presentation order, the planes' CRC-32 the reference decoding's"
run 452_gpu_display_readout_runtime "the kernel's read-out of a Radeon's display controller built for the host, on register tables: a picture the card scales (and what would be set for one of the monitor's size), one in borders, one sent as it is, controllers off, states that do not fit, the frames in a second, the raw dump, flags beside the fields that are read, every controller at its own numbers"
run 423_h265_stream_runtime "H.265 parameter sets into the Vulkan video structures, every picture of a 64x64 stream with B frames and a CRA picture, the reference picture sets' current lists as slots, the output order, a random access from the CRA picture, a cut SPS refused"
run 424_h265_cpu_decode_runtime "a 64x64 H.265 Main 10 stream (weighted B frames, AMP, transform skip, SAO, wavefronts) decoded on the CPU by std.h265.decode: every picture in output order with the planes' CRC-32 the reference decoding's, 8-bit planes rounded; the same on a pool of three threads"
run 425_vp9_header_runtime "a 48x32 VP9 stream's uncompressed headers (profile, depth, subsampling, size, frame types, references and their slots, refresh flags, the compressed header's place), a superframe's index split, a broken frame marker refused"
run 417_aac_decode_runtime "AAC-LC decoded from an MP4 and an ADTS stream, each within 15 dB of the WAV it was encoded from; damaged blocks refused; reset"
run 416_mp4_demux_runtime "MP4 files read: a plain file's sample tables (chunks, composition offsets, sync samples, an edit list, seeking), fragments (trex defaults, tfdt, trun flags, one without tfdt), avcC parameter sets and Annex B, AudioSpecificConfig, a fragment cut short, damage"

# A packed struct wider than a word is refused with the message that says
# so (tests/support/packed_too_wide).
too_wide=$("$compiler" --quiet tests/support/packed_too_wide/main.mlx -o "$work/too_wide" 2>&1 || true)
if grep -q 'MLX-E4001: a packed struct holds at most 64 bits' <<< "$too_wide"; then
    echo "ok   packed_too_wide: a packed struct of 80 bits refused"
else
    echo "FAIL packed_too_wide: expected the 64-bit message, got:"
    echo "$too_wide" | head -3
    failures=$((failures + 1))
fi

if [[ $failures -ne 0 ]]; then
    echo "$failures standard module checks failed"
    exit 1
fi
echo "all standard module checks passed"
