# mlx-tasn1

libtasn1 for the C and C++ programs built here, in Mlx, with libtasn1's
C API (4.19), the part WebKit calls: PAL's TASN1 utilities
(Source/WebCore/PAL/pal/crypto/tasn1) read and write the DER of
WebCrypto's keys with it (SubjectPublicKeyInfo, PKCS #8 PrivateKeyInfo,
ECPrivateKey, RSAPublicKey, RSAPrivateKey), from the definitions
asn1Parser wrote as a C array. WebCore includes `<libtasn1.h>` as it
would libtasn1's header and links `libmlxtasn1.so` in place of
`libtasn1.so.6`.

The interface is written once, in Mlx: `api/libtasn1.mlx` declares the
types, constants and functions C sees (`asn1_node`, `asn1_static_node`,
`ASN1_SUCCESS`, `ASN1_ETYPE_*`...). `mlx-cheaders` (compiler/cc) makes
the header of it, `mlx-out/include/mlx-tasn1/libtasn1.h`; there is no
header written by hand.

    ./mlx build tasn1           the library and its header
    tools/check_tasn1.sh        builds it and runs the test (and it against the system's libtasn1)

## What it does

`tasn1.mlx` keeps definitions and structures as one kind of tree, as
libtasn1 does, and follows libtasn1's rules for what is read, written
and refused:

- `asn1_array2tree` (the static array's sibling and child flags, the
  identifiers checked: `ASN1_ARRAY_ERROR`, `ASN1_ELEMENT_NOT_EMPTY`,
  `ASN1_IDENTIFIER_NOT_FOUND` with libtasn1's description),
  `asn1_create_element` (a definition copied with the types it names put
  in place), `asn1_delete_structure`.
- `asn1_der_decoding` and `asn1_der_decoding2`: long-form lengths,
  explicit and implicit tags, OPTIONAL and DEFAULT elements, CHOICE,
  SET (any order), SEQUENCE OF and SET OF (`?1`, `?2`...), ANY; trailing
  bytes and bad lengths are `ASN1_DER_ERROR`, and a failed decoding
  deletes the structure.
- `asn1_read_value` and `asn1_read_value_type` by libtasn1's names
  (`a.b`, `?1`, `?LAST`): INTEGERs as their bytes, OBJECT IDENTIFIERs as
  dotted text, BIT STRINGs with their length in bits, a CHOICE as the
  alternative's name, ANY as its encoding, `ASN1_MEM_ERROR` and the size
  needed for a buffer too short.
- `asn1_write_value`: INTEGERs from decimal text or bytes (made
  minimal), OBJECT IDENTIFIERs, BIT and OCTET STRINGs, BOOLEAN, NULL,
  CHOICE selection, ANY, `"NEW"` elements of SEQUENCE OF and SET OF,
  OPTIONAL elements removed by a NULL value.
- `asn1_der_coding`: DER with the definitions' tags, SET OF members in
  DER's order, `ASN1_VALUE_NOT_FOUND` and the element's name when a
  value is missing, `ASN1_MEM_ERROR` and the length needed.
- `asn1_strerror`, `asn1_perror`, `asn1_check_version`.

## Tests

`tools/check_tasn1.sh` builds the library and the header (also as C++),
checks that every function the header declares is defined, and runs
`tests/tasn1.c` built by mlx-cc: it prints exactly what `tasn1.expected`
says, which is what the system's libtasn1 4.19.0 prints for the same
program (the script checks that again when gcc and libtasn1 are
installed). The test uses WebCore's own definitions (WebCrypto's
asn1Parser output) and OpenSSL's keys (`tests/tasn1_vectors.h`: P-256,
RSA-1024, Ed25519, X25519, each as SubjectPublicKeyInfo and PKCS #8,
and the EC and RSA keys' own forms), decodes and reads them the way
WebCore does, encodes the structures WebCore exports, and tries
malformed DER and broken definitions.

## What is missing

Only what WebKit uses is here: no reading of ASN.1 text
(`asn1_parser2tree`, `asn1_parser2array`), no `asn1_print_structure`,
`asn1_find_node`, `asn1_number_of_elements`, `asn1_copy_node`, partial
decoding (`asn1_der_decoding_startEnd`, `asn1_der_decoding_element`),
`asn1_expand_any_defined_by` or the low-level `asn1_get_*`/`asn1_encode_*`
helpers. Times (UTCTime, GeneralizedTime) are kept as their text and not
checked; `ASN1_DECODE_FLAG_STRICT_DER` is accepted but lengths that are
not minimal are accepted as libtasn1 4.19 accepts them without it.
