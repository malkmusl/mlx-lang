#!/usr/bin/env bash
# mlx-xml2, libxml2's C API (compiler/capi/xml2, over std.xml.sax):
#
#   - `./mlx build xml2` builds the library (libmlxxml2.so) and the
#     headers: mlx-cheaders makes mlx-out/include/mlx-xml2/libxml/*.h of
#     the API's Mlx modules (compiler/capi/xml2/api);
#   - every function the headers declare is in the library;
#   - the headers in C and C++, and a program linked with the library;
#   - tests/layout.c: the structures' sizes and the offsets WebKit reads
#     are the library's (and, where there is the system's libxml2, its
#     header's);
#   - tests/sax.c, built by mlx-cc against the library, prints every SAX2
#     callback, its arguments and the parser's position, every error as
#     the handler gets it (or libxml2's report on stderr), the parser's
#     state and last error, for the documents of tests/corpus and
#     tests/fragments (namespaces, DTDs with entities, attribute lists and
#     parameter entities, CDATA, comments, PIs, UTF-8, UTF-16, Latin-1 and
#     other charsets, malformed documents of each kind, entity loops and
#     an amplification attack), pushed whole and a byte or a few at a
#     time, with the parser's options, as WebKit's XMLDocumentParser
#     drives it (its handlers, UTF-16 with xmlSwitchEncoding before each
#     chunk, fragments by xmlParseContent): the runs of tests/runs, which
#     must print what tests/sax.expected says (what sax.c prints built by
#     gcc against the system's libxml2, Ubuntu's 2.9.14);
#   - where there are gcc and libxml2's headers and library: sax.c built
#     by both prints the same for these runs and more (chunk sizes,
#     options); REGENERATE=1 writes tests/sax.expected of the system's;
#   - WebCore's XML document parser (the unified sources of
#     XMLDocumentParserLibxml2.cpp and XMLErrors.cpp) compiles with mlx-cc
#     on these headers, or stops at an error of mlx-cc's C++ that is not
#     in libxml2's API, where the WebKit source is (WEBKIT_SOURCE) and
#     WebCore's derived sources are made.
#
#   tools/check_xml2.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-xml2.XXXXXX)
trap '[ -n "${KEEP:-}" ] || rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }
headers=mlx-out/include/mlx-xml2
library=mlx-out/lib/libmlxxml2.so
tests=compiler/capi/xml2/tests

./mlx build -j 1 cc xml2 > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build xml2"; }
echo "ok   ./mlx build xml2"

grep -ho 'pub extern("c") fn [A-Za-z0-9_]*' compiler/capi/xml2/api/*.mlx | awk '{print $4}' | sort -u > "$work/declared"
nm -D --defined-only "$library" | awk '{print $NF}' | sort -u > "$work/defined"
comm -23 "$work/declared" "$work/defined" > "$work/missing"
[ ! -s "$work/missing" ] || { cat "$work/missing"; fail "functions the headers declare are not in the library"; }
echo "ok   the library defines the $(wc -l < "$work/declared") functions the headers declare"

cc() { mlx-out/bin/mlx-cc -I"$headers" "$@"; }
run() { LD_LIBRARY_PATH="$repo_root/mlx-out/lib" "$@"; }

cat > "$work/all.c" <<'EOF'
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/SAX2.h>
#include <libxml/xmlerror.h>
#include <libxml/entities.h>
#include <libxml/xmlIO.h>
#include <libxml/dict.h>
#include <libxml/encoding.h>
#include <libxml/globals.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlstring.h>
#include <libxml/tree.h>
#include <libxml/valid.h>
#include <libxml/hash.h>
#include <libxml/xmlversion.h>
static int elements;
static void start(void *ctx, const xmlChar *localname, const xmlChar *prefix, const xmlChar *URI, int nb_namespaces, const xmlChar **namespaces, int nb_attributes, int nb_defaulted, const xmlChar **attributes) { elements++; }
int main(void) {
    xmlSAXHandler sax = { 0 };
    xmlParserCtxtPtr ctxt;
    xmlChar *copy = xmlStrdup(BAD_CAST "copy");
    int ok;
    xmlInitParser();
    sax.startElementNs = start;
    sax.initialized = XML_SAX2_MAGIC;
    ctxt = xmlCreatePushParserCtxt(&sax, NULL, NULL, 0, NULL);
    xmlCtxtUseOptions(ctxt, XML_PARSE_NOENT | XML_PARSE_HUGE);
    xmlParseChunk(ctxt, "<a><b/>", 7, 0);
    xmlParseChunk(ctxt, "<c/></a>", 8, 1);
    ok = elements == 3 && ctxt->wellFormed && xmlStrEqual(copy, BAD_CAST "copy") && xmlGetPredefinedEntity(BAD_CAST "lt")->content[0] == '<' && LIBXML_VERSION == 20914;
    xmlFree(copy);
    xmlFreeParserCtxt(ctxt);
    return ok ? 0 : 1;
}
EOF
cc -c "$work/all.c" -o "$work/all.o" || fail "the headers do not compile as C"
cc -x c++ -std=c++20 -c "$work/all.c" -o "$work/all-cpp.o" || fail "the headers do not compile as C++"
cc "$work/all.c" -o "$work/all" "$library" || fail "a program does not link with the library"
run "$work/all" || fail "a program pushing a document to the parser"
echo "ok   the headers in C and C++"

cc -DMLX_XML2 "$tests/layout.c" -o "$work/layout" "$library" || fail "tests/layout.c does not build"
run "$work/layout" > "$work/layout.out" || { cat "$work/layout.out"; fail "the header's structures are not the library's"; }
echo "ok   tests/layout.c: the structures' sizes and offsets are the library's"

# The runs of tests/runs by the program SAX: its output, run by run.
runs() {
    local sax=$1 line options files
    while IFS= read -r line; do
        case "$line" in ''|'#'*) continue;; esac
        options=${line%%:*}
        files=${line#*:}
        echo "### $options"
        # (Globs and options split by the shell on purpose.)
        # shellcheck disable=SC2086
        (cd "$tests" && LD_LIBRARY_PATH="$repo_root/mlx-out/lib" "$sax" $options $files 2>&1)
    done < "$tests/runs"
}

cc "$tests/sax.c" -o "$work/sax" "$library" || fail "tests/sax.c does not build"
runs "$work/sax" > "$work/sax.out"
have_system=
if command -v gcc > /dev/null && printf '#include <libxml/parser.h>\nint main(void) { xmlInitParser(); return 0; }\n' | gcc -x c - -I/usr/include/libxml2 -o "$work/probe" -lxml2 > /dev/null 2>&1; then
    have_system=1
    gcc -O1 -w -I/usr/include/libxml2 "$tests/sax.c" -o "$work/sax-ref" -lxml2 || fail "tests/sax.c does not build against the system's libxml2"
    if [ -n "${REGENERATE:-}" ]; then
        runs "$work/sax-ref" > "$tests/sax.expected"
        echo "ok   tests/sax.expected written of the system's libxml2"
    fi
fi
diff -u "$tests/sax.expected" "$work/sax.out" > "$work/sax.diff" || { head -40 "$work/sax.diff"; fail "tests/sax.c prints otherwise"; }
echo "ok   tests/sax.c, $(grep -c '^### ' "$work/sax.out") runs on tests/corpus and tests/fragments: what the system's libxml2 prints ($(grep -c '^== ' "$work/sax.out") documents)"

if [ -z "$have_system" ]; then
    echo "skip the comparisons with the system's libxml2: no gcc with libxml2's headers and library"
else
    gcc -O1 -w -I/usr/include/libxml2 "$tests/layout.c" -o "$work/layout-ref" -lxml2
    "$work/layout-ref" > "$work/layout-ref.out"
    diff -u "$work/layout-ref.out" "$work/layout.out" || fail "the structures differ from the system's libxml2 header's"
    echo "ok   the structures' sizes and offsets are those of the system's libxml2 header"
    count=0
    for options in "-c 2" "-c 7" "-c 2 -noent" "-c 5 -noent -recover" "-c 13 -serror" "-c 4 -generic -noent"; do
        # shellcheck disable=SC2086
        (cd "$tests" && "$work/sax-ref" $options corpus/[ac-df-z]*.xml corpus/b[a-hj-z]*.xml corpus/e[a-mo-z]*.xml corpus/end*.xml corpus/entities.xml corpus/entity-markup.xml > "$work/theirs" 2>&1)
        # shellcheck disable=SC2086
        (cd "$tests" && run "$work/sax" $options corpus/[ac-df-z]*.xml corpus/b[a-hj-z]*.xml corpus/e[a-mo-z]*.xml corpus/end*.xml corpus/entities.xml corpus/entity-markup.xml > "$work/ours" 2>&1)
        diff -u "$work/theirs" "$work/ours" > "$work/diff" || { head -40 "$work/diff"; fail "sax.c $options differs from the system's libxml2"; }
        count=$((count + 1))
    done
    for options in "-webkit -c 10" "-webkit -c 3 -serror" "-fragment -webkit -serror"; do
        # shellcheck disable=SC2086
        (cd "$tests" && "$work/sax-ref" $options corpus/[ac-df-z]*.xml corpus/b[a-hj-z]*.xml corpus/e[a-mo-z]*.xml corpus/end*.xml corpus/entities.xml corpus/entity-markup.xml fragments/*.frag > "$work/theirs" 2>&1)
        # shellcheck disable=SC2086
        (cd "$tests" && run "$work/sax" $options corpus/[ac-df-z]*.xml corpus/b[a-hj-z]*.xml corpus/e[a-mo-z]*.xml corpus/end*.xml corpus/entities.xml corpus/entity-markup.xml fragments/*.frag > "$work/ours" 2>&1)
        diff -u "$work/theirs" "$work/ours" > "$work/diff" || { head -40 "$work/diff"; fail "sax.c $options differs from the system's libxml2"; }
        count=$((count + 1))
    done
    echo "ok   sax.c built by gcc against the system's libxml2 prints the same for tests/runs and $count more runs (chunks of 2 to 13 bytes, options, WebKit's)"
fi

webkit=${WEBKIT_SOURCE:-/home/user/csrc/wpe/wpewebkit-2.54.1}
unified=mlx-out/webkit/generated/WebCore/unified-sources
if [ ! -d "$webkit/Source/WebCore" ]; then
    echo "skip WebCore's XML document parser: no WebKit source at $webkit (WEBKIT_SOURCE)"
elif [ ! -d "$unified" ]; then
    echo "skip WebCore's XML document parser: WebCore's derived sources are not made (./mlx build webkit-webcore-derived)"
else
    # The unified sources of WebCore's uses of libxml2: the XML document
    # parser and its scope, XMLErrors.cpp. A unit compiles, or mlx-cc
    # stops at an error of its own C++ outside libxml2's API (the libxml2
    # declarations the whole unit uses resolved before it instantiates
    # templates).
    for source in xml/parser/XMLDocumentParserLibxml2.cpp xml/XMLErrors.cpp; do
        bundle=$(grep -l "\"$source\"" "$unified"/*.cpp | head -1)
        [ -n "$bundle" ] || fail "no unified source has $source"
        unit="wpe-webcore/$(basename "$bundle")"
        log=mlx-out/build/logs/$(echo "$unit" | tr / _).log
        if ./mlx build -j 1 "$unit" > "$work/units.log" 2>&1; then
            echo "ok   $unit ($source) compiles"
        elif grep -q "libxml\|'xml[A-Z]\|xmlChar\|xmlParser\|xmlSAX\|xmlEntity" "$log"; then
            head -5 "$log"
            fail "$unit ($source): an error in libxml2's API"
        else
            echo "ok   $unit ($source): libxml2's API resolved; mlx-cc stops at $(grep -m1 'error:' "$log" | sed 's|.*/Source/||')"
        fi
    done
fi

echo "check_xml2.sh: all checks passed"
