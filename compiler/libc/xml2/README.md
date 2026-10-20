# mlx-xml2

libxml2 for the C and C++ programs built here, in Mlx, with the API of
libxml2 2.9.14 as Ubuntu 24.04 has it (`<libxml/parser.h>` and the other
headers of `libxml/`, LIBXML_VERSION 20914). WebKit's XML document parser
(WebCore's XMLDocumentParserLibxml2.cpp, XHTML and SVG documents and
their fragments) includes it as it would libxml2's headers and links
`libmlxxml2.so` in place of `libxml2.so.2`: a push parser with WebKit's
SAX2 handler, fed the document as UTF-16 a chunk at a time
(`xmlSwitchEncoding` before each), entities substituted
(`XML_PARSE_NOENT | XML_PARSE_HUGE`), the document's entities kept by
SAX2's `xmlSAX2EntityDecl` and found again by `xmlGetDocEntity`;
fragments parsed by `xmlCreateMemoryParserCtxt` and `xmlParseContent`.

The parser is the standard library's `std.xml.sax` (std/src/xml/), which
Mlx programs use directly: libxml2's parser.c and parserInternals.c step
for step (with Ubuntu's patches: the entity amplification checks, the
limits of names and text), so that a program's handler gets the
callbacks libxml2's gives it, with the same arguments, at the same
positions, split where libxml2 splits text, and the same errors (codes,
messages, lines and columns, the parser's state after them):

- `sax.mlx`: the push parser (xmlParseTryOrFinish, xmlParseChunk's
  first-line and line-end handling), the productions of XML 1.0 and
  Namespaces (names, attribute values and their normalization, character
  data, references, comments, PIs, CDATA, the XML and text declarations,
  the internal subset: entity, attribute list, element and notation
  declarations, parameter entities, conditional sections), SAX2's start
  tags with namespaces and defaulted attributes, entities' content parsed
  by a parser of its own (xmlParseBalancedChunkMemoryInternal), the
  errors.
- `encoding.mlx`: the encodings the first bytes show and those declared:
  UTF-8, UTF-16 (LE, BE, with or without a byte order mark), UCS-4,
  ISO-8859-1, ASCII, and the single-byte charsets of std.unicode.charmaps
  (as libxml2 asks iconv for them), with libxml2's errors on bytes an
  encoding does not have.
- `uri.mlx`: URI references as uri.c parses them (namespace names and
  system identifiers are checked).
- `text.mlx`: the dictionary of names, buffers, XML's character classes,
  the printf subset of the messages.

This directory is the C layer: the headers (made by mlx-cheaders from
`api/*.mlx`) and the exported functions over std.xml.sax. A parser
context (`xmlParserCtxt`, laid out as libxml2's) holds the std parser
after libxml2's structure; the context's fields are the parser's state,
shown again before each callback and when a function returns, and what
a program sets in them (the handler, `userData`, `instate` and `depth`
before `xmlParseContent`) is taken at the next call. The parser's
callbacks go through `parser.mlx`, which calls the program's SAX handler
with its user data and does what libxml2 does around them: SAX2's own
entity lookup when the handler finds none, the "SAX compatibility mode"
document of entities for handlers that keep none, the routing of errors
(`errors.mlx`: the handler's `serror`, the global structured handler,
`warning` and `error` with the message, or libxml2's report on stderr
with the document's line and a caret), a context of its own for an
entity's content.

    ./mlx build xml2           the library and its headers
    tools/check_xml2.sh        builds it, parses tests/corpus and
                               tests/fragments with it and compares with
                               the system's libxml2, compiles WebCore's
                               XML document parser

## What it does

- Parsing: `xmlCreatePushParserCtxt`, `xmlParseChunk`,
  `xmlCreateMemoryParserCtxt`, `xmlCreateDocParserCtxt`, `xmlParseContent`,
  `xmlNewParserCtxt`, `xmlCtxtReset`, `xmlCtxtResetPush`,
  `xmlCtxtUseOptions` (and the handler changes its options make),
  `xmlStopParser`, `xmlFreeParserCtxt`, `xmlByteConsumed`,
  `xmlSwitchEncoding`, `xmlSwitchToEncoding`, `xmlCurrentChar`,
  `xmlNextChar`, `xmlStringDecodeEntities`, the defaults
  (`xmlKeepBlanksDefault`, `xmlSubstituteEntitiesDefault`...).
- SAX2: the default handler (`xmlSAXVersion`,
  `xmlSAX2InitDefaultSAXHandler`), the document, internal subset and
  entity callbacks (`xmlSAX2StartDocument`, `xmlSAX2EndDocument`,
  `xmlSAX2InternalSubset`, `xmlSAX2EntityDecl`, `xmlSAX2GetEntity`,
  `xmlSAX2GetParameterEntity`, `xmlSAX2UnparsedEntityDecl`), the locator.
- Documents and entities: `xmlNewDoc`, `xmlFreeDoc`, `xmlNewDtd`,
  `xmlCreateIntSubset`, `xmlGetIntSubset`, `xmlFreeDtd`, the entity
  tables (`xmlAddDocEntity`, `xmlAddDtdEntity`, `xmlNewEntity`,
  `xmlGetDocEntity`, `xmlGetDtdEntity`, `xmlGetParameterEntity`,
  `xmlGetPredefinedEntity`, `xmlEncodeEntitiesReentrant`,
  `xmlEncodeSpecialChars`), qualified names (`xmlSplitQName2`,
  `xmlBuildQName`, `xmlValidateNCName`...).
- Errors: `xmlSetGenericErrorFunc`, `xmlSetStructuredErrorFunc`,
  `xmlGetLastError`, `xmlCtxtGetLastError`, `xmlResetError`,
  `xmlCopyError`, `xmlParserError`, `xmlParserWarning`,
  `xmlParserValidityError` and the default generic handler (variadic, as
  printf's).
- Strings, memory, dictionaries, encodings: all of `xmlstring.h`,
  `xmlMemSetup` and the `xmlMalloc`/`xmlFree` variables, `dict.h`,
  `encoding.h` (handlers by name and by `xmlCharEncoding`, aliases,
  `isolat1ToUTF8`); the I/O callbacks are kept (`xmlRegisterInputCallbacks`).

222 functions in all, every one WebCore calls with XSLT off
(`ENABLE_XSLT=0` in projects/webkit/config.mlx): those it names besides
(`xmlReadMemory`, the XPath and tree functions) are XSLT's and the
encrypted media's.

## How it compares

tools/check_xml2.sh runs tests/sax.c, which prints every callback with
its arguments and the parser's position, every error as the handler gets
it, the parser's state and last error and the document SAX2 kept, built
by mlx-cc against this library, on 57 documents (namespaces and their
errors, DTDs with entities, attribute lists and parameter entities,
CDATA, comments and PIs, UTF-8, UTF-16 with and without a byte order
mark, Latin-1, windows-1252 and KOI8-R, unknown encodings, malformed
documents of each kind, an entity loop, a billion laughs, deep nesting,
long names and text) and 7 fragments, in 17 runs (whole and a byte, 3
and 64 bytes at a time, the parser's options, the handler's and the
global structured error handlers, libxml2's report on stderr, a stop,
WebKit's handler, encoding and chunks, fragments): it prints what the
same program built by gcc against the system's libxml2 prints
(tests/sax.expected), and so for nine more runs where gcc and libxml2
are there.

What differs:

- No tree: the SAX2 callbacks that build libxml2's tree build none, so a
  parser with SAX2's default handler makes a document without nodes (its
  DTD and entities are made), and what the tree changes in libxml2 (the
  depth of 256 nodes without `XML_PARSE_HUGE`, entities' node lists
  reused instead of parsed again) is not there. `xmlReadMemory` and the
  tree's functions are not declared.
- External entities and the external subset are never loaded, as when
  libxml2's loader finds nothing (`xmlNoNetExternalEntityLoader`'s
  warning); validation, SAX1's callbacks and `XML_PARSE_OLD10` are not
  here. The attribute and element declaration callbacks are not called.
- Entities nested in entities recurse as in libxml2, but Mlx's frames
  are larger than C's: past 3 MB of stack (some 300 entities deep in
  content) the parser reports the loop libxml2 reports at its limit of
  1024 (512 entities in content).
- The encodings are those of std.xml.encoding and std.unicode.charmaps
  (iconv has more: the CJK multi-byte ones, EBCDIC); a handler a program
  makes with `xmlNewCharEncodingHandler` is kept but not decoded with.
- `xmlIsLetter` and the name validators use XML 1.0's fifth edition's
  classes (libxml2's are the fourth's).
