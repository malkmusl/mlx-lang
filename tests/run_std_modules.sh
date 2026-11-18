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
#   tests/495_gcn_encoding_runtime.mlx  the Vulkan driver's encoder and
#   tests/496_gcn_interpreter_runtime.mlx  decoder, interpreter and shader
#   tests/497_gcn_compiler_runtime.mlx  compiler for the RX 580 (projects/
#                                       brix/vulkan; tools/check_gcn_isa.py
#                                       and tools/check_gcn_encoding.sh hold
#                                       them against Mesa and llvm-mc)
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
run 381_net_runtime "IPv4 and IPv6 addresses, socket addresses, a TCP connection on the loopback, DNS queries and answers, names answered without a server, a name with the dot of the root at its end, what is no name"
run 461_logserve_runtime "logserve without the network: a request's head read (waited for, GET and HEAD, other methods, no HTTP/1, over the limit), the fixed pages a path names, the kernel log's gpu: lines, a saved log that is the log's beginning, a link's record in words, the text that grows; the limit's 2048, a query, a NUL and encoded dots in a path, a head a byte at a time"
run 488_kernel_random_runtime "the kernel's random number generator built for the host: its block function against RFC 8439's vector and std.crypto.chacha's at chance; what counts as a processor's random words (not zeros, ones or all the same); fit for secrets only when keyed from them; the same seed the same numbers, another seed, moment or request others; answers of every length, nothing written past them; the key anew at every request and in no answer; a thousand words all different, every byte value among sixteen thousand"
run 487_logpush_runtime "logpush built for the host against servers of the test's own, one without TLS and one with: the settings in every form (no file, empty, one line, CR LF, blanks and tabs, a line break after the token or none, GitHub's lengths of names, further lines) and what is wrong with them by a line's number alone; the names by the boot's date and the seconds since, the ROM by its bytes; the requests byte for byte (the path, each header line, the JSON with the base64 of a text and of every byte value, a branch), a whole kernel log of 256 KiB, the ROM and the log of the boot before asked for first and left alone when they are there; 201, 200, 401, 403, 404, 408, 409, 410 (an API version GitHub no longer serves), 422, 429, 500, a redirect not followed, a body that is no JSON, an answer cut off, a connection that says nothing, nobody listening; over TLS when the chain leads to the root given and the name is the certificate's, nothing sent when the root is not trusted, the name another's, the clock not set or no random numbers for a key; a test endpoint without TLS takes a test's token only; the token in no line it can say, also from a server that sends it back"
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
run 470_canvas_loops_runtime "std.ui's pixel loops against the plain loops they replace, bit for bit: a colour over a pixel at every coverage and alpha, runs of pixels stored, clear, fills, rounded fills, outlines, scaled images and rows of coverage at chance (sizes 0 and 1, boxes off each edge, both byte orders), text against its glyphs' coverage, text a character to a cell against each character cut to its cell"
run 471_terminal_cells_runtime "std.ui.terminal draws the cells that changed: after writes, scrolls, clears, the alternate screen, the history, selections, another cursor, resizes and colours at chance the picture is the one drawn whole, the cells drawn are exactly those that changed (after the rows it moved), a line more at the bottom moves the rows and draws that line, nothing changed draws nothing; dim text is the mean of its colour and its background, on white too"
run 457_block_map_runtime "brixfs's map of the blocks in use (a bit a block by words, with the count) against a loop over single bits: runs marked and let go at chance, the first free block, what a second map keeps; the runs of a page"
run 458_commit_frees_replaced_runtime "how brixfs's store finds its free blocks, on a model with the store's map: commits that let go what they replaced and mark the rewritten files again, snapshots kept, the disk run full: after each step the map is what a walk over every root marks"
run 441_types_across_import_ring_runtime "records of three modules that import one another naming one another: pointers, records inside, a function type, sizes"
run 413_git_runtime "git object names, deltas applied, commits parsed, this checkout's HEAD read and its commits counted, a directory without a repository"
run 418_h264_stream_runtime "H.264 parameter sets into the Vulkan video structures, every access unit of WebKit's test.mp4 as a picture with its references and slices, the DPB's output order against the file's presentation times, Annex B framing, a cut SPS refused"
run 422_h264_cpu_decode_runtime "WebKit's test.mp4 (Main profile, B frames) decoded on the CPU through std.mp4 and std.h264: every picture, in presentation order, the planes' CRC-32 the reference decoding's"
run 452_gpu_display_readout_runtime "the kernel's read-out of a Radeon's display controller built for the host, on register tables: a picture the card scales (and what would be set for one of the monitor's size), one in borders, one sent as it is, controllers off, states that do not fit, the frames in a second, the raw dump, flags beside the fields that are read, every controller at its own numbers"
run 459_edid_timing_runtime "the kernel's reading of a monitor's EDID built for the host, on blocks written from the standard's layout: the preferred timing, the least demanding of several of the monitor's size, the own mode only in a CTA-861 or DisplayID extension, the 1080p timing of the standards, wrong checksums, no EDID, interlaced, stereo and slow timings, a CTA block filled to its end"
run 460_gpu_mode_set_runtime "the kernel's plan for a monitor's own mode on a Radeon's display controller and its carrying out, built for the host, against a card that is a table and writes down every write: the sequences for a picture the card scales and for a timing set on DisplayPort, word for word as worked out from Linux's code; where the waits stand among the writes; no write when the boot keeps the firmware's picture, a link without room, HDMI, a picture of the kernel's on another adapter; the firmware's state back after a write that does not hold or a state that does not come at every step, frames that stand or run at another rate, the memory interface running dry"
run 472_atom_interpreter_runtime "the kernel's interpreter for a Radeon's firmware tables built for the host, on tables assembled by hand against a card that is a table: moves into every kind of destination, every part of a word as source and destination, the operations, products and quotients of 32 and 64 bits, compare, test and the seven jumps, switches, calls with the parameter space moved on, data blocks, the register block and the ROM's indirect register programs, the scratch, waits, the PCI and IO port modes as Linux has them; what stops a run and says where: an opcode that is none, reads outside the ROM and a table's own space, lists that are too short, a table that calls itself, loops without end by count and by time, hands that may not write or do not reach"
run 473_gpu_rom_runtime "the kernel's reading of a Radeon's video BIOS ROM built for the host, on ROMs the test makes: what holds a ROM and what names it, the tables a mode set runs, the firmware information's clocks, the connectors with their transmitters, pins and lines and which one a DIG block is, the log's lines; the ROM out of an ACPI table VFCT, through the PCI expansion ROM and through the card's ROM registers with every word written put back; a wrong checksum, marks and lengths that are wrong, lists that are too short, pointers outside, display paths and records that do not end"
run 474_gpu_tmds_sequence_runtime "the kernel's mode set on a Radeon's TMDS output through the card's own firmware tables built for the host, against a card that is a table and a ROM whose tables write down what they were told: the whole change for a monitor's own mode by HDMI, every write and every table's run with its block in order, as worked out from the sequence's document; no write and no table run when the boot keeps the firmware's picture; the way back write for write and run for run; a failure at every step (a write that does not hold, a state that does not come, a table stopped by the card, one that writes nothing, does not end or has an opcode that is none, timing words not as told, a PLL that keeps its clock, no frames with the transmitter off) and the firmware's state back after each; the registers alone before any table has written; a table that fails on the way back; the display engine's clock raised and put back; the firmware's own mode set again; an output that sends HDMI already; another controller, encoder, PLL and connector; a PLL a little short of its clock; a way back on which the card does the same wrong (not said to be back), a register that stays (said by its number); the longest dark time said beforehand"
run 475_gpu_tmds_blocks_runtime "what the kernel's mode set on a Radeon's TMDS output tells the card's firmware tables, and when it plans nothing, built for the host: the parameter blocks byte for byte against the bytes worked out from Linux's atombios.h and command_table.c (the pixel clock, the transmitter on and off, the stream, the controller, the timing, the display engine's clock), each back end's PHY, PLL and encoder, the AVI info frame, the timing's words; what a monitor's EDID offers (every timing of its size, the HDMI blocks, extension blocks not passed on); the timing and the signalling chosen and the link's limits; every reason for no plan with its line and no write: no ROM, a table missing or of another revision, an output that is not TMDS or has no PLL, no connector in the ROM or one of another kind, a hot plug pin that is not placed, a clock of another source, no frames, no way back, no front porch, no RGB, a display clock that cannot be trusted or raised, nothing to rehearse"
run 480_gpu_dp_sequence_runtime "the kernel's DisplayPort link trained by the driver itself, built for the host, against a card that is a table and a monitor behind its AUX channel that is one too: a link that is too slow taken down and trained at the rate and lanes the monitor's own mode needs, every write, table run and AUX transaction in order as worked out from the sequence's document, and the way back to the firmware's own link event for event; every rate and count of lanes; a link that is not trained; a link that has room kept and its lanes read afterwards, trained again when the monitor loses it; the firmware's own link taken down and trained again; the asking alone; no write, table or transaction when the boot keeps the firmware's picture; rounds that take what the monitor asks for, with the waits; a training that fails each way there is, four tries with the link down between; a failure at every write, table run and access to the monitor and the firmware's state back after each; a way back whose table stops, whose link does not train, a register that stays (none of them said to be back); another controller, front end, back end, hot plug line and AUX channel; the longest dark time said beforehand"
run 481_gpu_dp_link_runtime "what the kernel's DisplayPort link rests on, built for the host: an AUX transaction's register words one by one against the words worked out from Linux's dce_aux.c, pins taken and given back, a channel that is off or watches another line; the tries of an access (a monitor that puts off, answers short, late or not at all, refuses, a hot plug line that is low, a channel the card's microcontroller holds); the monitor's EDID through the channel's I2C bridge; what the asking reads and in which order, a monitor in D3, capabilities from 2200h, every reason a monitor is none to train; what a rate and lanes carry at the edges, Linux's order of choice, the waits, the pattern and drive bytes, the next drive, the lanes' judgement; the transmitter's blocks for a link byte for byte; a DisplayPort connector's AUX channel and rates as the ROM names them; every reason for no plan with its line and no write: no ROM, a table missing or of another revision, no connector or one of another kind, a hot plug pin or DDC line that is not placed, a monitor that is none to train or holds another link, no rate and lanes that carry the timing, a display clock that is too low or cannot be trusted, no DTO, sync pulses elsewhere, several streams, a picture on another adapter, a monitor that does not answer, nothing to rehearse"
run 482_display_choice_runtime "what the GPU's driver does with the display at a boot, chosen in the one image, on the host: the keys (f n r a, either case, of every character a keyboard gives) and the choice each stands for; what the machine remembers (one byte, F or N; anything else is none) and what the loader writes after a key for every size and first byte; the image's word; every combination of key, remembered variable, image word and what the firmware let the loader do, through the word the loader hands over: the key, then what is remembered, then the image, then the monitor's own mode; a word of 0 and one without the loader's mark; that only the monitor's own mode and the rehearsal let the driver write; the log's lines word for word, with what is said of a firmware without a keyboard or a variable store, a write refused, something remembered that is no choice; how long the loader looked; the loader's own lines; and the EFI loader's own part run here against a firmware that is a table: every key list with every variable and every answer to a write, a key that comes after 0 to 40 looks without one, a keyboard that never runs out or fails, a firmware that lacks its keyboard, its waiting, its variable store or its console (nothing that is not there is called), the word it hands over, what it writes, what it says, the registers and the stack as it found them"
run 483_gpu_plug_detect_runtime "the kernel's looking for the monitor on a Radeon's connectors built for the host, against a card that is a table and ROMs made here: each connector's hot plug line read as Linux reads it and said, each of the six lines at its own block and its own bit of the pins' word, a block that is off, a pin held as a plain pin, a line that changes and rests or never rests, connectors without a line; the detection in every case (the firmware's output with a monitor, without one, a monitor on an output the firmware did not start, two plugged, an adapter, a DisplayPort link, nothing started, two outputs, a ROM without records); that no mode is set on an output whose line says for certain that nothing is plugged in, and on every other; the lines looked at again and again against a clock that is the test's: an unplug at the second look that agrees, a plug after Linux's filter time, a short pulse, a bounce, a plug shorter than the filter, a block that goes off and comes back, each change said once; a line whose two sense bits differ for good said once as one that cannot be read; every kind of connector; what serves a connector read anew at a change; an hour of ticks on a still card; all lines toggled at random with no change lost or given twice; pulses of every length around Linux's times at every phase; no write in all of it, and no read at a tick without a look"
run 484_gpu_ddc_edid_runtime "a monitor's EDID read by the kernel over a DDC line with a Radeon's I2C engine built for the host, against a card that is a table with an EDID memory behind each line: a connector's line out of the ROM's table; the first look and a block behind its segment pointer write for write, look for look and pause for pause as worked out from Linux's dce_i2c_hw.c, with every engine and pin word as it was found afterwards; each line at its own words, a pad left in AUX mode; EDIDs of one, two, four and eight blocks; nothing on the line, a missing acknowledge once and every time, an engine that stays busy to Linux's bound and is reset, its time limit, a run given up, a block whose sum is wrong once, always and in an extension, a base block of nothing, a word that does not take its old value back, everything that refuses before a write; the same reading a piece a tick without a pause; the asking at boot (who, what is said, the EDID held against the loader's) and while brixOS runs (asked when plugged in, once more a second later, forgotten when unplugged, one at a time, nobody without hands, a reading dropped when its monitor was pulled and another plugged in meanwhile); every line with the engine's words holding anything beforehand, a missing acknowledge at every message of every transfer, an engine that never finishes under a clock that stands still, 255 blocks named, a failure that stays in the status word; an hour of ticks and a minute of moving the monitor with hands lent"
run 485_gpu_switch_runtime "the monitor followed from one output of a Radeon to another by the kernel, built for the host, against the user's whole card as a table (five connectors, the card's tables as programs of the test's, an I2C engine with an EDID memory behind each DDC line, the hot plug lines by a script): at boot the monitor on the firmware's output (nothing decided, the plain plan's writes one for one), on the other HDMI output (the picture brought there, the transmitter made ready first, the old PLL powered down), on none, on a DisplayPort output, under the choice that writes nothing, with the firmware's output's line not judged, on a card whose firmware started no output; while brixOS runs the cable moved from HDMI to HDMI and back and to DVI and back (every table run in order with its block, the picture's memory untouched, nothing cleared), the monitor switched off and on and to another input and back (nothing written), the output found not sending on its return, a second monitor, the monitor pulled with none elsewhere and plugged in again, a monitor of another size, two monitors elsewhere (HDMI first), a monitor that answers only when asked a second time, a move that waits while another EDID is read, something that gives no EDID, the choice that only asks; the transmitter's init disturbing the running output (taken off, its frames stopped, its frames at another rate); a failure at every write and every table run of the move with the state of before back after each and the move tried again; a move given up after three tries"
run 486_gpu_switch_plan_runtime "the kernel's plan that brings the picture to another output of a Radeon, built for the host, against the user's whole card as a table: HDMI to HDMI byte for byte against the study (the transmitter's init, the hot plug select, the old output down, the old PLL powered down, the new PLL, the new front end, the way back), without the init, to DVI, a timing a DVI link cannot carry, the front end when the back end's own is taken and when every one is, every refusal (DisplayPort, a back end on already, a connector of another kind, no hot plug line, no PLL, the ROM's two sides disagreeing, the picture kept and the monitor of another size), the same-size rule, the clock the way back asks for, the plan for the same connector step for step the plain one, another controller and the other pair of outputs"
run 493_gpu_engines_found_runtime "the kernel's first look at a Radeon's engines (step S0) and how far a boot goes, built for the host, against a card that is a table as the user's RX 580 answers: the digits 0 to 7 of every character a keyboard gives, the stage of every word the loader can hand over with every display choice (F holds every stage back), its line word for word, the display's part of the aperture; the EFI loader's own part with digits alone, beside a letter, twice, after looks without one, without a keyboard; S0's lines on the user's card, only the three index words written and each left as found, the GART's place by Linux's rule; the SMC running, no AVFS, another list version, every memory columns field, a soft key, a file missing, cut short or of another header, no GFX_Info, no ROM, a host path word, the VGA words; a card left running said at stage 0 and halted at stage 1 write for write; every refusal"
run 494_gpu_engines_memory_runtime "the kernel's stages 1 and 2 toward a Radeon that takes commands, built for the host, against the same card: the aperture divided and the driver's part proved (M1), the GART turned on (M2) and the interrupt ring brought up (I1) write for write in the study's and Linux's order; every refusal before any write; every self-test failing (a word that does not hold, the word beyond the aperture, a register that does not hold, a fault, a GART entry that reads back otherwise or that the card does not see, a storm, a write pointer not written back, a busy ring, entries whose interrupt does not come, the picture minding, the card gone) with every register as the firmware had it afterwards; a card without MSI; the way down; no register of the display's written anywhere"
run 495_gcn_encoding_runtime "our Vulkan driver's encoder and decoder for the RX 580 (GFX8) built for the host: an instruction of every format the card has (SOP2, SOPK, SOP1, SOPC, SOPP, SMEM, VOP2, VOP1, VOPC, VOP3 with its modifiers and the VOP3b carries, VINTRP, DS, MUBUF, MTBUF, MIMG, FLAT, EXP, DPP, SDWA) against the words LLVM's assembler gives for it, encoded, decoded and named; the instruction table made from Mesa's aco_opcodes.py: 946 instructions in order, the counts by format, none of GFX9's (nor FLAT's d16 forms), each one encoded and decoded back; what may not be written"
run 496_gcn_interpreter_runtime "our Vulkan driver's interpreter of the RX 580's shader code built for the host: each instruction's effect on a wave of 64 lanes (the scalar and vector arithmetic with their carries and flags, the comparisons at the edges, exec and the saveexec forms, conversions, the reciprocal, loads and stores, the branches, a work group's start); what it refuses with its reason: a load read or written before its wait, the ten wait-state rules one short and passed with their s_nop, two scalar values on a vector instruction, an access past a buffer's range or outside memory, a register no instruction wrote, a value the sources do not give, an instruction not modelled, a run without end"
run 497_gcn_compiler_runtime "our Vulkan driver's shader compiler built for the host: std.gpu's nine kernels compiled from their SPIR-V and run in the interpreter on real inputs, every pixel against the processor (std.truetype.drawRun, std.gpu.video.convertNv12, std.ui.canvas, the compositor's blendPixel and coverage; pattern, both blurs, warp and shrink as their builders write them); divisions at the edges against the processor's (by a variable, by powers of two, of two uniform values), the reciprocal one and two units below and above; what a loop's header makes read after the loop (a word kept by each lane, a boolean refused); each of the ten comparisons on a lane's values and on the wave's, and an if on a lane's value whose sides give the wave's values; the wait states the compiler's pass puts in (before a buffer instruction's scalar offset and its descriptor)"
run 455_dhcp_lease_runtime "the kernel's DHCP client built for the host, on a clock and a wire of the test's: DISCOVER and its pauses, the OFFER's REQUEST, an ACK with and without the lease's times, a router, a name server, a T1 of no seconds and a lease of one second, T1's REQUEST to the server, an ACK that extends, T2's to everyone, the lease's end, a NAK, a lease for ever, a clock that jumps, answers that are cut short or another's; the link coming up with an address held (the lease asked for anew at once, of everyone, quickly at first: an ACK, a NAK, silence); an ACK whose netmask is none"
run 464_net_reach_runtime "what the kernel's network stack works out from its configuration, built for the host: which four bytes are a netmask (ones, then zeros: no holes, not 0.0.0.0), which addresses are everyone's (255.255.255.255, a subnet's last; none on a subnet of one or two) or a group's (224.0.0.0 and above), and the way a packet takes or what stands in it: no device, the link down, no address, no gateway; the machine itself reached without any of them"
run 465_tcp_half_open_runtime "the kernel's table of TCP connections asked of a listener and not made yet, built for the host, on a clock of the test's: a SYN's record found again, the SYN+ACK after one and three seconds and the end after seven, the same SYN once more and another from the same port, the one that waited longest giving way to a newcomer when every record is taken, a hundred peers that never answer and one that does, a listener's records gone with it"
run 476_boot_displays_runtime "the kernel's reading of the displays in the boot information, built for the host, on blobs written as the EFI loader writes them: none, one from a loader that knows of one, two, eight, a count beyond the room, records that are no picture's or a picture again, two cards behind a console's output without pixels; each display's words and shape; the main display by its monitor's word, by its picture, the firmware's first of those alike, and named in settings/display by number or adapter"
run 477_displays_fit_runtime "std.displays on ordinary memory: where a picture lies on a display of another size and shape (bars above and below or beside it, the wide monitor's picture on a 4:3 display, pixels that are not square), a plain colour as itself and black bars at any sizes, the frame's own size, half and twice, red-first displays, frames at chance against the mean in whole numbers; after changes named the display is what the whole frame gives and no more was written than changed, also brought in slices; seven displays at most, none the frame is too large for"
run 489_display_modes_runtime "the modes a display's monitor offers and a mode changed while the system runs, built for the host: the user's monitor's first block (its own 3840x1080 timing as the machine's log gives it) and what a link's limit says of it; established timings by their bits, those of the third kind, standard timings by their codes with the standards' numbers, a code without a timing, reduced blanking for a monitor that takes it; an extension's video codes beside a timing written out, interlaced and pixel-doubled ones, a code beyond the table, a mode named twice kept once, the largest first; no EDID and one that does not add up; every request against a driver and a clock that are tables: asked for and pending, confirmed and kept, the clock running out (taken back by the kernel, once), taken back by hand, a second request while one is pending, nothing pending, a mode the display lacks, a size of a driver that makes any, a mode refused with nothing written, a change that fails with the mode before back or not, a way back that fails, a remembered mode set at boot and forgotten, the modes listed anew when the monitor says more of itself; the records byte for byte; the remembered file's text and what a boot does with it; a size asked for at any rate is the monitor's own mode of it, an unlisted size at a rate is none; a list of an EDID that names every kind of timing made with less than 64 KiB of the value arena"
run 490_gpu_mode_change_runtime "a Radeon's TMDS output given another mode while the system runs, built for the host, against a card that is a table and a ROM whose tables write down what they were told: the modes judged without a write (one that fits, the one that runs, another rate of its size, more than the output carries, less than it sends); from the monitor's own mode to 1920x1080 write for write and table run for table run what a boot's mode set does for that timing, every other processor held before the first write and let go after the last; back by the timing read from the controller's words, the picture at the aperture's start (two places in turn, not further up each time); a mode the output does not carry, a card without its ROM and an output that is not TMDS refused with nothing written and nobody held; a failure at every table's run in turn and at writes that do not hold with the mode before back, one not known to be back said as that; the user's machine as it is, the monitor's HDMI block not read: HDMI signalling for its own mode, DVI for 1920x1080, and back; held and let go alike on every path; the plan made from what the card runs at the change, not from what was read after the last one"
run 489_display_modes_runtime "the modes a display's monitor offers and a mode changed while the system runs, built for the host: the user's monitor's first block (its own 3840x1080 timing as the machine's log gives it) and what a link's limit says of it; established timings by their bits, those of the third kind, standard timings by their codes with the standards' numbers, a code without a timing, reduced blanking for a monitor that takes it; an extension's video codes beside a timing written out, interlaced and pixel-doubled ones, a code beyond the table, a mode named twice kept once, the largest first; no EDID and one that does not add up; every request against a driver and a clock that are tables: asked for and pending, confirmed and kept, the clock running out (taken back by the kernel, once), taken back by hand, a second request while one is pending, nothing pending, a mode the display lacks, a size of a driver that makes any, a mode refused with nothing written, a change that fails with the mode before back or not, a way back that fails, a remembered mode set at boot and forgotten, the modes listed anew when the monitor says more of itself; the records byte for byte; the remembered file's text and what a boot does with it"
run 490_gpu_mode_change_runtime "a Radeon's TMDS output given another mode while the system runs, built for the host, against a card that is a table and a ROM whose tables write down what they were told: the modes judged without a write (one that fits, the one that runs, another rate of its size, more than the output carries, less than it sends); from the monitor's own mode to 1920x1080 write for write and table run for table run what a boot's mode set does for that timing, every other processor held before the first write and let go after the last; back by the timing read from the controller's words, the picture at the aperture's start (two places in turn, not further up each time); a mode the output does not carry, a card without its ROM and an output that is not TMDS refused with nothing written and nobody held; a failure at every table's run in turn and at writes that do not hold with the mode before back, one not known to be back said as that; the user's machine as it is, the monitor's HDMI block not read: HDMI signalling for its own mode, DVI for 1920x1080, and back; held and let go alike on every path"
run 491_desktop_mode_change_runtime "the desktop laid out anew when the display's mode changes while the compositor runs, on the host: where a window goes in a work area that changed (the room beside it shared out as before, one off the new screen brought inside, one larger than it made its size, there and back where it was); the top bar and the dock, a maximized, a fullscreen, a snapped, two tiled, a floating, a too large and a minimized window from 1024x768 to 800x600, to 3840x1080 and back; the question whether to keep the mode: its card and buttons on small and large screens, its seconds, its words in English and German, Enter, Escape, other keys held, a click on each button and elsewhere, drawn dimming the screen with Keep in the accent"
run 492_display_page_runtime "the settings program's Display page and its way to the compositor, on the host: the compositor's file as brixOS writes it (who sets the modes, where to ask, the seconds left, each mode with its rate, own, running, kept, or why it cannot be set) and as another system writes it; the rows and the line the page says; org.mlx.Display.Ask on a channel of the compositor's listener in MLXIPC's native format: the request as the compositor reads it and every answer as the asker gets it (set, refused with the reason, busy, no such mode, not allowed, failed with the mode before back or not), a call that is no Ask, an asker that went, the listener gone with the compositor, the page asking with nobody listening"
run 498_gpu_together_runtime "a Radeon's display while brixOS runs as one driver, built for the host, against the user's whole card as a table: after a mode change the switch's record is the new mode's and the same monitor is followed to the other output with the timing that runs; after a move a change is planned for the new output, and the modes, what runs and what the EDID says of HDMI are the new monitor's; the same monitor back writes nothing; a mode set for a monitor that came in place of the first: its EDID is the switch's record, it is the same monitor when it comes back and is followed when its cable is moved; a change whose way back is lost ends the following; a mode pending while the monitor is moved taken back first, then the picture of the mode before brought over; the time to confirm over at the very tick that decides a move: taken back, the move not made with the picture of before; a second monitor, the monitor switched off and on or pulled with none elsewhere leave the pending mode; at every write of a mode change a tick looks at nothing and reads nothing and a move waits; at every write of a move, its way back's too, a request is busy; after every piece of an EDID's reading a mode change lets the reading end before its first write; a line that changes at every moment of a change taken after it; everything else held for the writing alone, no read-out before a move when nothing wrote the card; a late confirmation finds the mode before; a remembered mode that failed is forgotten; the remembered file keeps its newest lines; a new picture never where the one before lay"
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
