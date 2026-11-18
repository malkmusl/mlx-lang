#!/usr/bin/env bash
# Holds the encoder and the decoder of our shader compiler for the RX 580
# (projects/brix/vulkan/encode.mlx, decode.mlx) against LLVM's assembler
# and disassembler for the card (llvm-mc -triple=amdgcn -mcpu=tonga, the
# same generation as Polaris), on every instruction the compiler emits for
# std.gpu's nine kernels and every one its selection can emit in a few
# operand patterns (tools/gcn-encoding/main.mlx lists them: our words and
# our text):
#   1. our text, assembled by llvm-mc, gives our words;
#   2. our words, disassembled by llvm-mc, give our text (LLVM's names
#      for the few instructions ACO's table names otherwise, and its _e32
#      suffix, taken off first).
# A development check: llvm-mc is the host's, not part of what is built;
# where it is not installed the check says so and passes.
#
#   tools/check_gcn_encoding.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
mc=$(command -v llvm-mc || true)
if [[ -z "$mc" ]]; then
    echo "check_gcn_encoding: no llvm-mc on this host: skipped"
    exit 0
fi
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
"$compiler" --quiet tools/gcn-encoding/main.mlx -o "$work/list"
"$work/list" > "$work/ours.txt"
if grep -q '^#' "$work/ours.txt"; then
    echo "FAIL the list itself: $(grep '^#' "$work/ours.txt" | sort -u | tr '\n' ' ')"
    exit 1
fi
cut -f1 "$work/ours.txt" > "$work/bytes.txt"
# our text with LLVM's names for the few instructions ACO's table names
# otherwise (the same map as below)
cut -f2 "$work/ours.txt" | sed -E 's/^v_(add|sub|subrev|addc|subb|subbrev)_co_u32/v_\1_u32/; s/^v_mul_lo_i32/v_mul_lo_u32/; s/^v_readlane_b32_e64/v_readlane_b32/; s/^v_cmp(x?)_lg_(i32|u32)/v_cmp\1_ne_\2/; s/^v_cmp(x?)_tru_(i32|u32)/v_cmp\1_t_\2/' > "$work/text.txt"

# 1. our text through LLVM's assembler
"$mc" -triple=amdgcn -mcpu=tonga -show-encoding < "$work/text.txt" > "$work/assembled.txt" 2> "$work/assembled.err" || true
grep 'encoding:' "$work/assembled.txt" | sed 's/.*encoding: \[\(.*\)\]/\1/' > "$work/assembled.bytes"
# 2. our words through LLVM's disassembler
"$mc" --disassemble -triple=amdgcn -mcpu=tonga < "$work/bytes.txt" 2> "$work/disassembled.err" | grep -v '^\s*\.text' | sed 's/^\s*//' > "$work/disassembled.txt" || true

python3 -I - "$work" <<'EOF'
import re, sys
work = sys.argv[1]
ours = [l.rstrip("\n").split("\t") for l in open(work + "/ours.txt")]
assembled = [l.strip() for l in open(work + "/assembled.bytes")]
disassembled = [l.strip() for l in open(work + "/disassembled.txt")]
errors = open(work + "/assembled.err").read().strip()
# LLVM's names for this generation where ACO's table has its own
alias = {
    "v_add_co_u32": "v_add_u32", "v_sub_co_u32": "v_sub_u32",
    "v_subrev_co_u32": "v_subrev_u32", "v_addc_co_u32": "v_addc_u32",
    "v_subb_co_u32": "v_subb_u32", "v_subbrev_co_u32": "v_subbrev_u32",
    "v_mul_lo_i32": "v_mul_lo_u32", "v_readlane_b32_e64": "v_readlane_b32",
}
def norm(text):
    text = re.sub(r"\s+", " ", text.strip())
    mnemonic, _, rest = text.partition(" ")
    # the integer comparisons' lg and tru are LLVM's ne and t
    mnemonic = re.sub(r"^v_cmp(x?)_lg_(i32|u32)", r"v_cmp\1_ne_\2", mnemonic)
    mnemonic = re.sub(r"^v_cmp(x?)_tru_(i32|u32)", r"v_cmp\1_t_\2", mnemonic)
    # a branch's distance: LLVM's disassembler writes it as 16 bits unsigned
    if (mnemonic.startswith("s_branch") or mnemonic.startswith("s_cbranch")) and rest.startswith("-"):
        rest = str(int(rest) + 65536)
    mnemonic = re.sub(r"_e32$", "", mnemonic)
    base = re.sub(r"_e64$", "", mnemonic)
    suffix = "_e64" if mnemonic.endswith("_e64") else ""
    base = alias.get(base, base)
    return (base + suffix + " " + rest).strip()
failures = 0
if errors:
    print("FAIL llvm-mc refused some of our text:")
    print("\n".join(errors.splitlines()[:12]))
    failures += 1
if len(assembled) != len(ours) or len(disassembled) != len(ours):
    print("FAIL counts differ: ours %d, assembled %d, disassembled %d" % (len(ours), len(assembled), len(disassembled)))
    failures += 1
for (bytes_, text), back, words in zip(ours, assembled, disassembled):
    if back.replace(" ", "") != bytes_.replace(" ", ""):
        print("FAIL assembling %r gives [%s], ours [%s]" % (text, back, bytes_))
        failures += 1
    if norm(words) != norm(text):
        print("FAIL disassembling [%s] gives %r, ours %r" % (bytes_, words, text))
        failures += 1
    if failures > 20:
        break
if failures:
    sys.exit(1)
print("ok   %d instructions: llvm-mc assembles our text into our words and disassembles our words into our text" % len(ours))
EOF
