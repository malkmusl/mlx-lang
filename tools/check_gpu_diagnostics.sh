#!/usr/bin/env bash
# The GPU's crashes and profile (std.gpu.diagnostics through
# std.diagnostics) on lavapipe, with tests/support/gpu_diagnostics_program.mlx:
#
#   - with MLX_GPU_PROFILE the GPU time goes on the functions that recorded
#     the work (heavy far above light), as PROGRAM-gpu-SECONDS.profile;
#   - mlx-profile profiles the processor and has the GPU profiled beside it;
#   - a dispatch that never ends is a GPU crash in the pass that hung, said
#     with the functions that recorded it and kept as a gpu-crash line;
#   - mlx-codemap reads both (gpu, crashes).
#
#   tools/check_gpu_diagnostics.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
lavapipe=""
for manifest in /usr/share/vulkan/icd.d/lvp_icd.json /usr/share/vulkan/icd.d/lvp_icd.x86_64.json; do
    if [[ -f "$manifest" ]]; then lavapipe=$manifest; break; fi
done
if [[ -z "$lavapipe" ]]; then
    echo "skip tools/check_gpu_diagnostics.sh (no lavapipe manifest)"
    exit 0
fi
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
fail() {
    echo "FAIL $1" >&2
    [[ -n "${2:-}" && -f "$2" ]] && sed 's/^/    /' "$2" >&2
    exit 1
}

"$compiler" --quiet tests/support/gpu_diagnostics_program.mlx -o "$work/program"
"$compiler" --quiet tools/profile/main.mlx -o "$work/mlx-profile"
"$compiler" --quiet tools/codemap/main.mlx -o "$work/mlx-codemap"
export VK_DRIVER_FILES=$lavapipe
export XDG_STATE_HOME=$work/state

# The profile.
MLX_GPU_PROFILE=1 "$work/program" profile || fail "the profile run ended with $?"
profile=$(ls "$work"/state/mlx/profiles/program-gpu-*.profile 2>/dev/null | head -1)
[[ -n "$profile" ]] || fail "no GPU profile written"
head -1 "$profile" | grep -q "^profile	program	[0-9]*	[0-9]*	1	gpu$" || fail "the GPU profile's header is wrong" "$profile"
heavy=$(awk -F'\t' '$3 ~ /:heavy$/ { print $2 }' "$profile")
light=$(awk -F'\t' '$3 ~ /:light$/ { print $2 }' "$profile")
[[ -n "$heavy" && -n "$light" && "$heavy" -gt $((light * 5)) ]] || fail "heavy ($heavy µs) is not far above light ($light µs)" "$profile"
grep -q "^[0-9]*	[0-9]*	tests/support/gpu_diagnostics_program.mlx:[0-9]*:run$" "$profile" || fail "the work is not on run, which recorded it" "$profile"
echo "ok   MLX_GPU_PROFILE: the GPU time is on the functions that recorded the work (heavy $heavy µs, light $light µs)"

# mlx-profile: the processor, and the GPU beside it.
"$work/mlx-profile" -o "$work/both" "$work/program" profile > "$work/mlx-profile.txt" 2>&1 || fail "mlx-profile ended with $?" "$work/mlx-profile.txt"
ls "$work"/both/program-[0-9]*.profile > /dev/null 2>&1 || fail "mlx-profile wrote no processor profile" "$work/mlx-profile.txt"
ls "$work"/both/program-gpu-*.profile > /dev/null 2>&1 || fail "no GPU profile beside mlx-profile's" "$work/mlx-profile.txt"
head -1 "$work"/both/program-[0-9]*.profile | grep -q "	processor$" || fail "the processor profile does not say so"
echo "ok   mlx-profile: the processor's profile, and the GPU's beside it (MLX_GPU_PROFILE)"

# A hang.
status=0
"$work/program" hang 2> "$work/hang.err" || status=$?
[[ $status -eq 3 ]] || fail "the hang run ended with $status, not 3" "$work/hang.err"
grep -q "^program: crashed: GPU hang (a frame did not finish in 5 s), in pass 2 of 2 (a dispatch)$" "$work/hang.err" || fail "the hang is not said" "$work/hang.err"
grep -q "^  from tests/support/gpu_diagnostics_program.mlx:[0-9]*:stuck" "$work/hang.err" || fail "the hang does not name stuck, which recorded the pass" "$work/hang.err"
grep -q "^gpu-crash	[0-9]*	program	GPU hang .*	.*:stuck+" "$work/state/mlx/crashes.log" || fail "no gpu-crash line kept" "$work/state/mlx/crashes.log"
echo "ok   a GPU hang: said in the pass that hung, with the functions that recorded it, and kept as a gpu-crash"

# The codemap reads both.
"$work/mlx-codemap" gpu tests/support > "$work/gpu.txt"
grep -q "^tests/support/gpu_diagnostics_program.mlx:[0-9]*:[0-9]*: function heavy (GPU work recorded here 0 µs, with what it calls" "$work/gpu.txt" || fail "mlx-codemap gpu does not list heavy" "$work/gpu.txt"
"$work/mlx-codemap" crashes tests/support > "$work/crashes.txt"
grep -q "^== program: GPU hang" "$work/crashes.txt" || fail "mlx-codemap crashes does not list the GPU crash" "$work/crashes.txt"
grep -q "function stuck (on the way 1x)" "$work/crashes.txt" || fail "the GPU crash is not on stuck" "$work/crashes.txt"
echo "ok   mlx-codemap reads them: gpu, crashes"
echo "all GPU diagnostics checks passed"
