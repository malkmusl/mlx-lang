#!/usr/bin/env bash
# Checks std.crypt (std/src/crypt.mlx: the password hashes of
# /etc/shadow) against the system's crypt(3), the one glibc and libxcrypt
# provide, on hashes made for the check: for each password and setting,
# both have to spell the same hash, std.crypt's verify has to take it,
# and it has to refuse a password that is one byte different.
#
# Nothing here reads /etc/shadow or any other real password: python3
# makes the hashes from the passwords below.
#
#   tools/check_crypt.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-$(tools/ensure_compiler.sh)}
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

command -v python3 > /dev/null || { echo "check_crypt.sh: needs python3 (for the system's crypt(3))" >&2; exit 2; }
python3 -c "import crypt" 2> /dev/null || { echo "check_crypt.sh: python3 has no crypt module (3.13 dropped it); install one with crypt(3) bindings" >&2; exit 2; }

"$compiler" --quiet tests/support/crypt_hashes.mlx -o "$work/crypt_hashes"

# The cases: passwords around every block boundary of SHA-256 and
# SHA-512 and some with bytes a line cannot hold as it stands, salts of
# every length a hash takes, and the rounds a setting may ask for.
python3 -W ignore - "$work" <<'PYTHON'
import crypt, random, subprocess, sys

work = sys.argv[1]
random.seed(20261009)
alphabet = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"

passwords = ["", "a", "test", "p@ssw0rd!", "correct horse battery staple",
             "tab\tand space", "umlaut äöü and €", "\x01\x02\x7f",
             "x" * 15, "x" * 16, "x" * 17, "x" * 31, "x" * 32, "x" * 33,
             "x" * 55, "x" * 63, "x" * 64, "x" * 65, "x" * 111, "x" * 127,
             "x" * 128, "x" * 129, "x" * 200]

def salt(length):
    return "".join(random.choice(alphabet) for _ in range(length))

settings = []
for scheme in ("$5$", "$6$"):
    for length in (0, 1, 2, 8, 15, 16):
        settings.append(scheme + salt(length))
    # A salt longer than 16 characters is used up to 16 of them.
    settings.append(scheme + salt(24))
    for rounds in (1000, 1001, 5000, 12345, 40000):
        settings.append(f"{scheme}rounds={rounds}${salt(16)}")

lines, expected = [], []
for password in passwords:
    for setting in settings:
        made = crypt.crypt(password, setting)
        if made is None:
            continue
        lines.append(f"{setting} {made} {password.encode().hex()}")
        expected.append(made)

result = subprocess.run([work + "/crypt_hashes"], input="\n".join(lines) + "\n",
                        capture_output=True, text=True)
if result.returncode != 0:
    print("FAIL crypt_hashes ended with", result.returncode)
    print(result.stdout[-2000:], result.stderr[-2000:])
    raise SystemExit(1)
ours = result.stdout.splitlines()
if len(ours) != len(lines):
    print(f"FAIL {len(lines)} cases went in, {len(ours)} answers came back")
    raise SystemExit(1)

wrong = 0
for line, want, answer in zip(lines, expected, ours):
    made, _, mark = answer.rpartition(" ")
    if made != want or mark != "ok":
        wrong += 1
        if wrong <= 5:
            setting = line.split(" ")[0]
            print(f"FAIL {setting}")
            print(f"  crypt(3): {want}")
            print(f"  std.crypt: {made} {mark}")
if wrong:
    print(f"FAIL {wrong} of {len(lines)} hashes differ from crypt(3)")
    raise SystemExit(1)
schemes = sorted({line[:3] for line in lines})
print(f"ok   {len(lines)} hashes the same as crypt(3) ({', '.join(schemes)}), each verified and a changed password refused")
PYTHON

# What std.crypt refuses rather than guesses at. Two kinds: hashes it
# will not read at all (MD5, which nothing should still be using), and
# settings crypt(3) itself refuses (a locked account, rounds outside the
# range), where both answer no.
cat > "$work/refused" <<'CASES'
$1$abcdefgh $1$abcdefgh$ignored 74657374
$md5$abcdefgh $md5$abcdefgh$ignored 74657374
*unusable* *unusable* 74657374
!$6$abcdefgh $6$abcdefgh$ignored 74657374
$6$rounds=999$abcdefghijklmnop *0 74657374
$6$rounds=2000000000$abcdefghijklmnop *0 74657374
$6$rounds=$abcdefghijklmnop *0 74657374
CASES
refused=$("$work/crypt_hashes" < "$work/refused")
[[ "$(grep -c '^- refused$' <<< "$refused")" == 7 ]] || { echo "FAIL a setting std.crypt cannot read was not refused:" >&2; echo "$refused" >&2; exit 1; }
for setting in '*unusable*' '!$6$abcdefgh' '$6$rounds=999$abcdefghijklmnop' '$6$rounds=2000000000$abcdefghijklmnop' '$6$rounds=$abcdefghijklmnop'; do
    theirs=$(python3 -W ignore -c 'import crypt, sys; print(crypt.crypt("test", sys.argv[1]))' "$setting")
    [[ "$theirs" == "*"* || "$theirs" == "None" ]] || { echo "FAIL crypt(3) answers $theirs for $setting, where std.crypt refuses it" >&2; exit 1; }
done
echo "ok   refused rather than guessed at: a locked account and rounds out of range (crypt(3) refuses those too), and MD5 on purpose"
echo "all std.crypt checks passed"
