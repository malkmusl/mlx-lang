#!/usr/bin/env bash
# mlx-glib, GLib, GObject and GIO with GLib's C API in Mlx (compiler/capi/glib):
#
#   - `./mlx build glib` builds the library (libmlxglib.so) and the
#     headers: mlx-cheaders makes mlx-out/include/mlx-glib/**/*.h of the
#     API's Mlx modules (compiler/capi/glib/api);
#   - every function the headers declare is in the library;
#   - the headers in C and C++: glib.h, glib-object.h, glib-unix.h,
#     glib/gstdio.h, gio/gio.h and gobject/gvaluecollector.h together;
#   - C programs built by mlx-cc against the library print what
#     NAME.expected says (what GLib 2.80 prints for the same program,
#     with the exit status last):
#       tests/strings.c    strings, UTF-8, conversions, lists, hashes...
#       tests/mainloop.c   contexts, sources, timeouts, threads, once
#       tests/variant.c    GVariant: types, serialization, printing
#       tests/uri.c        GUri: parsing, building, escaping
#       tests/gobject.c    types, values, properties, signals, objects
#       tests/system.c     markup, spawning, Unix signals, dates
#       tests/gio.c        cancellables, tasks, files and monitors,
#                          sockets, resources, D-Bus without a bus
#       tests/collect.c    G_DECLARE_FINAL_TYPE, G_VALUE_COLLECT_INIT,
#                          G_VALUE_LCOPY, g_alloca
#       tests/options.c    GOptionContext and groups, gettext
#       tests/network.c    addresses, a socket service, a client
#       tests/tls.c        TLS certificates (PEM, DER, chains, keys,
#                          X.509 fields, verification), GInitable,
#                          passwords, the backend and its databases
#       tests/tls_handshake.c
#                          TLS connections to openssl s_server (TLS 1.3
#                          and 1.2, synchronous and asynchronous,
#                          pollable reads, accept-certificate), when
#                          openssl is installed
#       tests/streams.c    input and output streams: memory, filter and
#                          socket streams, synchronous and asynchronous,
#                          splicing, pollable sources, GSimpleIOStream
#       tests/extras.c     keyed data lists, random numbers, language
#                          names, IDNA host names, GIO's enum types,
#                          proxy resolvers, network addresses,
#                          GResolver and a subclass of it, files
#                          and memory streams sought, the network
#                          and power monitors, content types (what
#                          libsoup and WebKit stand on)
#       tests/fileinfo.c   GFileInfo's attributes, masks and matchers;
#                          g_file_query_info (and async) and
#                          g_file_input_stream_query_info on files of
#                          this system, checked against stat
#       tests/dbus.c       GDBus on a dbus-daemon of the test's own (when
#                          dbus-daemon is installed): connections,
#                          names owned, exported objects called
#                          (methods, errors, properties, Introspect),
#                          signals and subscriptions, proxies,
#                          descriptors passed, timeouts, cancelling,
#                          closing; names' syntax, D-Bus errors
#   - WTF's sources for WPE that stand on GLib and the system
#     (tests/wtf/files.txt: wtf/glib, generic, unix, linux and posix)
#     compile with mlx-cc on these headers, with WPE's configuration
#     (tests/wtf/wpe-config.txt, NAME VALUE each), where the WebKit
#     source is (WEBKIT_SOURCE, by default where projects/webkit looks)
#     and mlx-unicode's headers are made (./mlx build unicode).
#
#   tools/check_glib.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-glib.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }
headers=mlx-out/include/mlx-glib
library=mlx-out/lib/libmlxglib.so

./mlx build cc glib > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build glib"; }
echo "ok   ./mlx build glib"

# Every `pub extern("c") fn` of the API modules is defined by the library.
grep -ho 'pub extern("c") fn [A-Za-z0-9_]*' compiler/capi/glib/api/*.mlx | awk '{print $4}' | sort -u > "$work/declared"
nm -D --defined-only "$library" | awk '{print $NF}' | sort -u > "$work/defined"
comm -23 "$work/declared" "$work/defined" > "$work/missing"
[ ! -s "$work/missing" ] || { cat "$work/missing"; fail "functions the headers declare are not in the library"; }
echo "ok   the library defines the $(wc -l < "$work/declared") functions the headers declare"

cc() { mlx-out/bin/mlx-cc -I"$headers" "$@"; }
run() { LD_LIBRARY_PATH="$repo_root/mlx-out/lib" "$@"; }

{
    for header in glib.h glib-object.h glib-unix.h glib/gstdio.h gio/gio.h gobject/gvaluecollector.h; do echo "#include <$header>"; done
    echo 'int main(void) { return 0; }'
} > "$work/all.c"
cc -c "$work/all.c" -o "$work/all.o" || fail "the headers together do not compile as C"
cc -x c++ -std=c++20 -c "$work/all.c" -o "$work/all-cpp.o" || fail "the headers together do not compile as C++"
echo "ok   the headers together in C and C++"

mkdir -p "$work/run"
for test in strings mainloop variant uri gobject system gio collect options network tls streams extras fileinfo; do
    cc "compiler/capi/glib/tests/$test.c" -o "$work/$test" "$library" || fail "tests/$test.c does not build"
    status=0
    (cd "$work/run" && run "$work/$test") > "$work/$test.out" 2> "$work/$test.err" || status=$?
    echo "exit $status" >> "$work/$test.out"
    diff -u "compiler/capi/glib/tests/$test.expected" "$work/$test.out" || { cat "$work/$test.err"; fail "tests/$test.c prints otherwise"; }
    echo "ok   tests/$test.c"
done

# TLS connections against OpenSSL's s_server on 127.0.0.1 (the
# certificates of tools/tls-test/make_certs.sh, made anew): one server
# for TLS 1.3, one for TLS 1.2 only, both choosing the suite themselves
# and keeping no sessions, so that what GIO's GnuTLS backend did on the
# same servers (tls_handshake.expected) is what is expected here.
if ! command -v openssl > /dev/null; then
    echo "skip tests/tls_handshake.c: no openssl"
else
    tools/tls-test/make_certs.sh "$work/certs"
    port=$((20000 + RANDOM % 30000))
    (cd "$work/certs" && exec openssl s_server -accept "$port" -cert p256.pem -key p256.key -cert_chain intermediate.pem -www -serverpref -num_tickets 0) > "$work/s13.log" 2>&1 &
    tls13_server=$!
    (cd "$work/certs" && exec openssl s_server -accept "$((port + 1))" -tls1_2 -cert rsa.pem -key rsa.key -cert_chain intermediate.pem -www -serverpref -no_ticket) > "$work/s12.log" 2>&1 &
    tls12_server=$!
    tries=0
    while ! grep -q ACCEPT "$work/s13.log" 2> /dev/null || ! grep -q ACCEPT "$work/s12.log" 2> /dev/null; do
        tries=$((tries + 1))
        [ "$tries" -lt 100 ] || { kill "$tls13_server" "$tls12_server"; fail "s_server did not start"; }
        sleep 0.05
    done
    status=0
    cc compiler/capi/glib/tests/tls_handshake.c -o "$work/tls_handshake" "$library" || { kill "$tls13_server" "$tls12_server"; fail "tests/tls_handshake.c does not build"; }
    (cd "$work/run" && run timeout 120 "$work/tls_handshake" "$port" "$((port + 1))" "$work/certs") > "$work/tls_handshake.out" 2> "$work/tls_handshake.err" || status=$?
    echo "exit $status" >> "$work/tls_handshake.out"
    kill "$tls13_server" "$tls12_server" 2> /dev/null || true
    wait "$tls13_server" "$tls12_server" 2> /dev/null || true
    diff -u compiler/capi/glib/tests/tls_handshake.expected "$work/tls_handshake.out" || { cat "$work/tls_handshake.err"; fail "tests/tls_handshake.c prints otherwise"; }
    echo "ok   tests/tls_handshake.c"
fi

# GDBus against a session bus of its own: a dbus-daemon started for the
# test (its first client is :1.0, as when GLib ran the same program on a
# daemon of its own for dbus.expected).
if ! command -v dbus-daemon > /dev/null; then
    echo "skip tests/dbus.c: no dbus-daemon"
else
    cc compiler/capi/glib/tests/dbus.c -o "$work/dbus" "$library" || fail "tests/dbus.c does not build"
    mkdir -p "$work/bus"
    dbus-daemon --session --address="unix:path=$work/bus/socket" --nofork --print-address=1 > "$work/bus/address" 2> "$work/bus/log" &
    bus=$!
    tries=0
    while [ ! -s "$work/bus/address" ]; do
        tries=$((tries + 1))
        [ "$tries" -lt 100 ] || { kill "$bus"; cat "$work/bus/log"; fail "dbus-daemon did not start"; }
        sleep 0.05
    done
    status=0
    (cd "$work/run" && DBUS_SESSION_BUS_ADDRESS=$(head -n 1 "$work/bus/address") run timeout 150 "$work/dbus") > "$work/dbus.out" 2> "$work/dbus.err" || status=$?
    echo "exit $status" >> "$work/dbus.out"
    kill "$bus" 2> /dev/null || true
    wait "$bus" 2> /dev/null || true
    diff -u compiler/capi/glib/tests/dbus.expected "$work/dbus.out" || { cat "$work/dbus.err"; fail "tests/dbus.c prints otherwise"; }
    echo "ok   tests/dbus.c"
fi

webkit=${WEBKIT_SOURCE:-/home/user/csrc/wpe/wpewebkit-2.54.1}
generated=mlx-out/webkit/generated
if [ ! -d "$webkit/Source/WTF/wtf" ]; then
    echo "skip WTF's GLib sources: no WebKit source at $webkit (WEBKIT_SOURCE)"
elif [ ! -d mlx-out/include/mlx-unicode/unicode ]; then
    echo "skip WTF's GLib sources: no mlx-unicode headers (made by ./mlx build unicode)"
elif [ ! -d "$generated/bmalloc" ]; then
    echo "skip WTF's GLib sources: no $generated/bmalloc (made by ./mlx build webkit-wtf)"
else
    mkdir -p "$work/config"
    {
        echo '#ifndef CMAKECONFIG_H'
        echo '#define CMAKECONFIG_H'
        sed 's/^/#define /' compiler/capi/glib/tests/wtf/wpe-config.txt
        echo '#endif'
    } > "$work/config/cmakeconfig.h"
    wtf=$webkit/Source/WTF
    flags=(-DBUILDING_WPE__=1 -DBUILDING_WEBKIT=1 -DBUILDING_WITH_CMAKE=1 -DBUILDING_WTF '-DGETTEXT_PACKAGE="WPE"' -DHAVE_CONFIG_H=1
        -DJSC_GLIB_API_ENABLED -DPAS_BMALLOC=1 '-DPKGDATADIR="/usr/local/share/wpe-webkit-2.0"' -DSTATICALLY_LINKED_WITH_bmalloc
        -D_GLIBCXX_ASSERTIONS=1 -DNDEBUG -DRELEASE_WITHOUT_OPTIMIZATIONS -std=c++23
        -DSIMDE_NO_VECTOR -DSIMDUTF_IMPLEMENTATION_ICELAKE=0 -DSIMDUTF_IMPLEMENTATION_HASWELL=0 -DSIMDUTF_IMPLEMENTATION_WESTMERE=0
        -I"$work/config" -I"$generated" -I"$generated/bmalloc" -I"$wtf")
    for sub in wtf wtf/dtoa wtf/fast_float wtf/persistence wtf/simdutf wtf/text wtf/text/icu wtf/threads wtf/unicode; do flags+=(-I"$wtf/$sub"); done
    flags+=(-Imlx-out/include/mlx-unicode -I"$headers")
    failed=0
    while read -r file; do
        object=$work/$(echo "$file" | tr / _).o
        if mlx-out/bin/mlx-cc "${flags[@]}" -c "$wtf/wtf/$file" -o "$object" > "$object.log" 2>&1; then
            echo "ok   wtf/$file"
        else
            echo "FAIL wtf/$file" >&2
            head -20 "$object.log" >&2
            failed=1
        fi
    done < compiler/capi/glib/tests/wtf/files.txt
    [ "$failed" = 0 ] || fail "WTF's GLib sources do not all compile"
fi

echo "check_glib.sh: all checks passed"
