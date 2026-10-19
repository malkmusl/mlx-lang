#!/usr/bin/env bash
# Makes the certificates tools/check_tls.sh serves with openssl s_server,
# in DIRECTORY: an RSA root (ca.pem), a P-256 intermediate it signed
# (intermediate.pem), and servers the intermediate signed for
# "localhost", "127.0.0.1" and "*.mlx.test": rsa (RSA 2048), p256, p384
# and ed25519 (NAME.pem, NAME.key, NAME-chain.pem with the intermediate).
#
#   tools/tls-test/make_certs.sh DIRECTORY
set -euo pipefail

directory=$1
mkdir -p "$directory"
cd "$directory"

cat > extensions.cnf <<'EOF'
[ca]
basicConstraints = critical, CA:TRUE
keyUsage = critical, keyCertSign, cRLSign
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid

[server]
basicConstraints = critical, CA:FALSE
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = DNS:localhost, IP:127.0.0.1, DNS:*.mlx.test
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid
EOF

openssl req -x509 -newkey rsa:2048 -nodes -keyout ca.key -out ca.pem -days 3650 \
    -subj "/C=DE/O=Mlx TLS Test/CN=Mlx TLS Test Root" -addext "basicConstraints=critical,CA:TRUE" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" 2> /dev/null
openssl req -new -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -keyout intermediate.key -out intermediate.csr \
    -subj "/C=DE/O=Mlx TLS Test/CN=Mlx TLS Test Intermediate" 2> /dev/null
openssl x509 -req -in intermediate.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out intermediate.pem -days 3000 \
    -extfile extensions.cnf -extensions ca 2> /dev/null

server() {
    local name=$1
    shift
    openssl req -new "$@" -nodes -keyout "$name.key" -out "$name.csr" -subj "/C=DE/O=Mlx TLS Test/CN=localhost" 2> /dev/null
    openssl x509 -req -in "$name.csr" -CA intermediate.pem -CAkey intermediate.key -CAcreateserial -out "$name.pem" \
        -days 365 -extfile extensions.cnf -extensions server 2> /dev/null
    cat "$name.pem" intermediate.pem > "$name-chain.pem"
}
server rsa -newkey rsa:2048
server p256 -newkey ec -pkeyopt ec_paramgen_curve:P-256
server p384 -newkey ec -pkeyopt ec_paramgen_curve:P-384
server ed25519 -newkey ed25519

rm -f ./*.csr ./*.srl
