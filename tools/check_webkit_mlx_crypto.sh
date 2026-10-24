#!/usr/bin/env bash
# The Mlx port of WebKit, M7 (projects/webkit/PORT.md, "Crypto"): WebCrypto
# (the port's crypto/mlx over std.crypto, std.aes, std.asn1, std.random)
# and crypto.getRandomValues. webcore-dump runs each of WebKit's layout
# tests that tests/layout/crypto.txt lists (crypto/subtle and crypto/,
# single-process ones: no workers, no internals) as the storage check runs
# its own (tools/check_webkit_mlx_storage.sh, which this calls with
# --list crypto.txt), and compares what it prints with the expected text.
# Their known-answer vectors (AES, HMAC, HKDF, PBKDF2, ECDSA, ECDH,
# Ed25519, X25519, RSA's) are imported keys and fixed inputs whose
# results the tests compare.
#
#   tools/check_webkit_mlx_crypto.sh [--no-build] [-j N] [--keep DIR] [TEST...]
exec "$(dirname "$0")/check_webkit_mlx_storage.sh" --list crypto.txt "$@"
