#!/usr/bin/env bash
# The Wi-Fi part of projects/network that runs without a Wi-Fi card:
#
#   - std.sha1 and std.aes against published vectors (FIPS 180-4 and 197,
#     RFC 2202, 6070 and 3394, WPA's PSK and PRF and the PTK of IEEE
#     802.11 Annex J) and against python's hashlib and OpenSSL's libcrypto
#     on random inputs;
#   - the WPA2 handshakes of projects/network/wpa.mlx
#     (tools/network-test/handshake.mlx) against an access point's side in
#     python (tools/network-test/authenticator.py) over a veth link, in a
#     network namespace of its own: the 4-way handshake and a group key
#     handshake agree on the keys, a replayed group message is not
#     answered, a forged message 3 is ignored, a message 3 with another
#     RSN element than the beacon's is refused (no message 4), and a wrong
#     passphrase shows as messages 1 without a message 3.
#
# nl80211 itself (scan, connect, key installation) needs a card or
# mac80211_hwsim and is not checked here.
#
#   tools/check_wifi.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
for tool in unshare ip python3; do
    command -v "$tool" > /dev/null || { echo "check_wifi.sh: $tool is not installed" >&2; exit 2; }
done

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
"$compiler" --quiet tools/network-test/vectors.mlx -o "$work/vectors"
"$compiler" --quiet tools/network-test/handshake.mlx -o "$work/handshake"

fail() { echo "FAIL $1" >&2; [[ -f "${2:-}" ]] && cat "$2" >&2; exit 1; }

expect() {
    local got
    got=$("$work/vectors" "${@:2}")
    [[ "$got" == "$1" ]] || fail "vectors ${*:2}: got $got, wanted $1"
}

hex_of() { python3 -c 'import sys; print(sys.argv[1].encode().hex())' "$1"; }

# Published vectors.
expect a9993e364706816aba3e25717850c26c9cd0d89d sha1 abc
expect 84983e441c3bd26ebaae4aa1f95129e5e54670f1 sha1 abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq
expect b617318655057264e28bc0b6fb378c8ef146be00 hmac 0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b "Hi There"
expect effcdf6ae5eb2fa2d27416d5f184df9c259a7c79 hmac "$(hex_of Jefe)" "what do ya want for nothing?"
expect 0c60c80f961f0e71f3a9b524af6012062fe037a6 pbkdf2 password salt 1 20
expect 4b007901b765489abead49d926f721d065a429c1 pbkdf2 password salt 4096 20
expect 3d2eec4fe41c849b80c8d83662c0e44a8b291a964cf2f07038 pbkdf2 passwordPASSWORDpassword saltSALTsaltSALTsaltSALTsaltSALTsalt 4096 25
expect 69c4e0d86a7b0430d8cdb78070b4c55a aes 000102030405060708090a0b0c0d0e0f 00112233445566778899aabbccddeeff
expect 00112233445566778899aabbccddeeff aesdecrypt 000102030405060708090a0b0c0d0e0f 69c4e0d86a7b0430d8cdb78070b4c55a
expect 1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5 wrap 000102030405060708090a0b0c0d0e0f 00112233445566778899aabbccddeeff
expect 00112233445566778899aabbccddeeff unwrap 000102030405060708090a0b0c0d0e0f 1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5
expect refused unwrap 000102030405060708090a0b0c0d0e0f 1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe6
expect f42c6fc52df0ebef9ebb4b90b38a5f902e83fe1b135a70e23aed762e9710a12e psk password IEEE
expect bcd4c650b30b9684951829e0d75f9d54b862175ed9f00606e17d8da35402ffee75df78c3d31e0f889f012120c0862beb67753e7439ae242edb8373698356cf5a prf 0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b prefix "$(hex_of 'Hi There')" 64
expect 379f9852d0199236b94e407ce4c00ec847c9edc01c2c6e5b4910caddfb3e51a7b2360c79e9710fdd58bea93deaf06599 ptk 0dc0d6eb90555ed6419756b9a15ec3e3209b63df707dd508d14581f8982721af a0a1a1a3a4a5 b0b1b2b3b4b5 e0e1e2e3e4e5e6e7e8e9f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff000102030405 c0c1c2c3c4c5c6c7c8c9d0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5
echo "ok   SHA-1, HMAC-SHA1, PBKDF2, AES-128, the key wrap, WPA's PSK, PRF and PTK: the published vectors"

# Random inputs against hashlib and libcrypto.
python3 - "$work/vectors" "$repo_root/tools/network-test" <<'PY' || fail "std.sha1 or std.aes differ from python's on random inputs"
import hashlib, hmac, os, random, subprocess, sys
vectors = sys.argv[1]
sys.path.insert(0, sys.argv[2])
import libcrypto
def run(*arguments):
    return subprocess.run([vectors, *arguments], capture_output=True, text=True, check=True).stdout.strip()
rng = random.Random(7)
def text(length):
    return ''.join(rng.choice('abcdefghijklmnopqrstuvwxyzABCDEFGHIJ0123456789 ') for _ in range(length))
for length in [0, 1, 55, 56, 63, 64, 65, 119, 120, 200]:
    message = text(length)
    assert run('sha1', message or '') == hashlib.sha1(message.encode()).hexdigest(), ('sha1', length)
for key_length in [1, 20, 64, 65, 100]:
    key = os.urandom(key_length)
    message = text(rng.randrange(1, 150))
    assert run('hmac', key.hex(), message) == hmac.new(key, message.encode(), hashlib.sha1).hexdigest(), ('hmac', key_length)
for rounds, length in [(2, 20), (7, 41), (4096, 32)]:
    password, salt = text(rng.randrange(1, 70)), text(rng.randrange(1, 40))
    assert run('pbkdf2', password, salt, str(rounds), str(length)) == hashlib.pbkdf2_hmac('sha1', password.encode(), salt.encode(), rounds, length).hex(), ('pbkdf2', rounds)
for _ in range(8):
    key, block = os.urandom(16), os.urandom(16)
    assert run('aes', key.hex(), block.hex()) == libcrypto.encrypt(key, block).hex(), 'aes'
    assert run('aesdecrypt', key.hex(), block.hex()) == libcrypto.decrypt(key, block).hex(), 'aes decrypt'
for blocks in [2, 3, 5, 12]:
    kek, plain = os.urandom(16), os.urandom(8 * blocks)
    wrapped = libcrypto.wrap(kek, plain)
    assert run('wrap', kek.hex(), plain.hex()) == wrapped.hex(), ('wrap', blocks)
    assert run('unwrap', kek.hex(), wrapped.hex()) == plain.hex(), ('unwrap', blocks)
PY
echo "ok   the same against hashlib and libcrypto on random inputs"

# The handshakes, in a namespace with a veth link: wlan0 (the station),
# ap0 (the access point).
cat > "$work/inner.sh" <<'SCRIPT'
W=$1
R=$2
ip link add wlan0 type veth peer name ap0
ip link set wlan0 up
ip link set ap0 up
INDEX=$(ip -o link show wlan0 | cut -d: -f1)
OWN=$(ip -o link show wlan0 | sed 's/.*link\/ether \([0-9a-f:]*\).*/\1/')
AP=$(ip -o link show ap0 | sed 's/.*link\/ether \([0-9a-f:]*\).*/\1/')
for run in normal:correct forged:correct downgrade:correct normal:wrong; do
    MODE=${run%%:*}
    PASS="${run##*:} horse battery"
    python3 "$R/tools/network-test/authenticator.py" ap0 "$OWN" "$PASS" "Test Net" --mode "$MODE" > "$W/ap-$run.txt" 2>&1 &
    SERVER=$!
    "$W/handshake" "$INDEX" "$OWN" "$AP" 30140100000fac040100000fac040100000fac020000 "correct horse battery" "Test Net" 11 > "$W/sta-$run.txt" 2>&1
    wait "$SERVER"
done
SCRIPT
timeout 120 unshare -rn bash "$work/inner.sh" "$work" "$repo_root" > "$work/inner.log" 2>&1 || fail "the namespace's script failed" "$work/inner.log"

ap="$work/ap-normal:correct.txt"
sta="$work/sta-normal:correct.txt"
grep -q "^msg2 ok$" "$ap" && grep -q "^msg4 ok$" "$ap" || fail "the 4-way handshake did not complete" "$ap"
[[ "$(sed -n 's/^ptk //p' "$ap")" == "$(sed -n 's/^pairwise //p' "$sta")" ]] || fail "the station and the access point derived other TKs" "$sta"
[[ "$(sed -n 's/^gtk 1 //p' "$ap")" == "$(sed -n 's/^group 1 //p' "$sta")" ]] || fail "the station did not take message 3's GTK" "$sta"
echo "ok   the 4-way handshake: the same TK on both sides, the GTK from message 3"
grep -q "^group ok$" "$ap" && [[ "$(sed -n 's/^gtk 2 //p' "$ap")" == "$(sed -n 's/^group 2 //p' "$sta")" ]] || fail "the group key handshake did not complete" "$ap"
grep -q "^replay ignored$" "$ap" || fail "a replayed group message was answered" "$ap"
echo "ok   the group key handshake takes a new GTK; its replay is not answered"

grep -q "^forged msg3 ignored$" "$work/ap-forged:correct.txt" && grep -q "^msg4 ok$" "$work/ap-forged:correct.txt" || fail "a forged message 3 was not ignored" "$work/ap-forged:correct.txt"
echo "ok   a message 3 with a broken MIC is ignored, the right one is answered"

grep -q "^failure element$" "$work/sta-downgrade:correct.txt" && ! grep -q "^pairwise" "$work/sta-downgrade:correct.txt" && grep -q "^no msg4$" "$work/ap-downgrade:correct.txt" || fail "message 3 with another RSN element was not refused" "$work/sta-downgrade:correct.txt"
echo "ok   a message 3 whose RSN element is not the beacon's is refused (no keys, no message 4)"

grep -q "^gave up$" "$work/ap-normal:wrong.txt" && grep -q "^unanswered 3$" "$work/sta-normal:wrong.txt" && ! grep -q "^pairwise" "$work/sta-normal:wrong.txt" || fail "a wrong passphrase did not show as unanswered messages 1" "$work/sta-normal:wrong.txt"
echo "ok   a wrong passphrase: the access point gives up, the station counts three messages 1 without a message 3"
