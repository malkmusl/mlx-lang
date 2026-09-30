#!/usr/bin/env bash
# mlx-profile (tools/profile) end to end, on tests/support/profile_busy.mlx
# (main calls work, work calls spin, where the time goes):
#   - the samples name the functions path:line:name, spin the hottest by
#     itself and work and main through it;
#   - the profile file has the header and the functions;
#   - mlx-codemap hot reads it.
#
#   tools/check_profile.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; [[ -f "${2:-}" ]] && cat "$2" >&2; exit 1; }

"$compiler" --quiet tools/profile/main.mlx -o "$work/mlx-profile"
"$compiler" --quiet tests/support/profile_busy.mlx -o "$work/busy"
"$compiler" --quiet tools/codemap/main.mlx -o "$work/mlx-codemap"

"$work/mlx-profile" -o "$work/state/mlx/profiles" --every 2 "$work/busy" > "$work/profile.txt" 2>&1 || fail "mlx-profile failed" "$work/profile.txt"
grep -q "^mlx-profile: [0-9]* samples" "$work/profile.txt" || fail "no samples" "$work/profile.txt"
first=$(grep -m1 "% itself: " "$work/profile.txt" || true)
grep -q "% itself: tests/support/profile_busy.mlx:4:spin$" <<< "$(grep -v ' 0% itself' "$work/profile.txt")" || fail "spin is not where the time went" "$work/profile.txt"
grep -q "% with calls, 0% itself: tests/support/profile_busy.mlx:14:work$" "$work/profile.txt" || fail "work is not seen through spin" "$work/profile.txt"
echo "ok   mlx-profile names spin, and work and main through it"

profile=$(ls "$work"/state/mlx/profiles/busy-*.profile)
head -1 "$profile" | grep -q "^profile	busy	[0-9]*	[0-9]*	2000$" || fail "the profile's header is wrong" "$profile"
grep -q "^[0-9]*	[0-9]*	tests/support/profile_busy.mlx:4:spin$" "$profile" || fail "the profile does not list spin" "$profile"
echo "ok   the profile is written"

XDG_STATE_HOME="$work/state" "$work/mlx-codemap" hot tests/support > "$work/hot.txt"
head -3 "$work/hot.txt" | grep -q "^tests/support/profile_busy.mlx:[0-9]*:[0-9]*: function \(spin\|work\|main\)" || fail "mlx-codemap hot does not read the profile" "$work/hot.txt"
echo "ok   mlx-codemap hot reads it"
