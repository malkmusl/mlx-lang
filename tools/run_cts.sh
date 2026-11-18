#!/usr/bin/env bash
# Runs groups of the Vulkan conformance suite's port (projects/cts) against
# our driver over the host card and says how they went: the suite's own
# result file (its .qpa log) for each group, then a summary of it: how
# many cases passed, failed (each one with the first line of its message)
# and were not supported (each reason with its count).
#
#   tools/run_cts.sh [--deqp PROGRAM] [--results DIR] [--library DIR]
#                    [--summary-only FILE.qpa] GROUP... [-- SUITE-ARGUMENTS]
#
#     GROUP            a case or a pattern of the suite (`dEQP-VK.info.*`,
#                      `dEQP-VK.api.smoke.create_sampler`), or @FILE: the
#                      cases FILE names, one a line (`#` starts a comment),
#                      run as one group named after FILE
#                      (`@projects/cts/check-cases.txt`: check-cases.qpa)
#     --deqp PROGRAM   the suite (default mlx-out/cts/bin/deqp-vk, made by
#                      `./mlx build cts`)
#     --library DIR    where its driver library is (default mlx-out/lib, where
#                      `./mlx build cts` puts libmlxcts-driver.so)
#     --results DIR    where the result files go (default mlx-out/cts/results);
#                      a group's is NAME.qpa, NAME the group with its dots
#                      and stars made underscores
#     --summary-only FILE.qpa
#                      run nothing: the summary of a result file written before
#     after `--`       given to the suite as they are (`--deqp-vk-device-id=1`)
#
# The suite runs with --deqp-compute-only=enable (our device has a compute
# queue only), with its watchdog (a case that hangs ends as a timeout),
# without the images in the log and without its SPIR-V
# validation and disassembly (SPIRV-Tools is not built: projects/cts/README.md);
# a case that ends the suite (a crash) is told as such and the suite runs
# again on the cases after it. A case of the group or the list that the
# suite never began (a name it does not have, a run that ended before it)
# is a failure too, "not run". Not supported for want of the port (Amber,
# SPIRV-Tools: "... in this port") is counted apart from what the driver
# does not support. It exits 0 when no case
# failed (a case not supported is no failure), 1 when one did, 2 when the
# suite could not run.
set -uo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
deqp=$repo_root/mlx-out/cts/bin/deqp-vk
library=$repo_root/mlx-out/lib
results=$repo_root/mlx-out/cts/results
summary_only=""
groups=()
extra=()
while [[ $# -gt 0 ]]; do
    case $1 in
    --deqp) deqp=$2; shift ;;
    --library) library=$2; shift ;;
    --results) results=$2; shift ;;
    --summary-only) summary_only=$2; shift ;;
    --) shift; extra=("$@"); break ;;
    -h|--help) sed -n '2,/^set -uo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;;
    -*) echo "run_cts.sh: unknown option $1 (see --help)" >&2; exit 2 ;;
    *) groups+=("$1") ;;
    esac
    shift
done
# (The suite runs in its own directory: the paths given are made whole
# here, where they were meant.)
whole() { if [[ $1 == /* ]]; then echo "$1"; else echo "$PWD/$1"; fi; }
deqp=$(whole "$deqp")
library=$(whole "$library")
results=$(whole "$results")

# The summary of the result file $1: counts, the failures with their first
# message line, the reasons for not supported with their counts. $2 (when
# given) names the cases that were to run and are not in the file: each
# is a failure, not run.
summarize() {
    awk -v file="$1" -v notrun="${2:-}" '
    BEGIN {
        if (notrun != "") while ((getline name < notrun) > 0) if (name != "") missing[++missed] = name
    }
    function finish(status, message) {
        if (current == "") return
        cases++
        if (status == "Pass") passed++
        else if (status == "NotSupported") {
            unsupported++
            reasons[message]++
            if (message ~ /in this port/) unbuilt++
        }
        else if (status == "QualityWarning" || status == "CompatibilityWarning") { warned++; warnings[current] = status ": " message }
        else { failed++; failures[failed] = current " (" status "): " message }
        current = ""
    }
    /^#beginTestCaseResult / { if (current != "") finish("Crash", "the suite stopped in the case"); current = $2; next }
    /^#terminateTestCaseResult / { finish($2, "the case ended the suite (" $2 ")"); next }
    /<Result StatusCode="/ {
        line = $0
        sub(/.*<Result StatusCode="/, "", line)
        status = line
        sub(/".*/, "", status)
        message = line
        sub(/^[^>]*>/, "", message)
        sub(/<\/Result>.*/, "", message)
        gsub(/&lt;/, "<", message); gsub(/&gt;/, ">", message); gsub(/&quot;/, "\"", message); gsub(/&apos;/, "'\''", message); gsub(/&amp;/, "\\&", message)
        pending_status = status
        pending_message = message
        next
    }
    /^#endTestCaseResult/ { finish(pending_status, pending_message); pending_status = ""; pending_message = ""; next }
    END {
        if (current != "") finish("Crash", "the suite stopped in the case")
        for (i = 1; i <= missed; i++) { current = missing[i]; finish("NotRun", "the suite never began the case") }
        printf "%s: %d cases: %d passed, %d failed, %d not supported", file, cases, passed, failed, unsupported
        if (unbuilt > 0) printf " (%d of them for want of the port, not of the driver)", unbuilt
        if (missed > 0) printf ", %d of the failed not run", missed
        if (warned > 0) printf ", %d with a warning", warned
        printf "\n"
        for (i = 1; i <= failed; i++) print "  failed: " failures[i]
        for (c in warnings) print "  warning: " c ": " warnings[c]
        for (r in reasons) printf "  not supported (%d): %s\n", reasons[r], r
        exit failed > 0 ? 1 : 0
    }' "$1"
}

if [[ -n "$summary_only" ]]; then
    [[ -f "$summary_only" ]] || { echo "run_cts.sh: no result file $summary_only" >&2; exit 2; }
    summarize "$summary_only"
    exit $?
fi
[[ ${#groups[@]} -gt 0 ]] || { echo "run_cts.sh: name a group (see --help)" >&2; exit 2; }
[[ -x "$deqp" ]] || { echo "run_cts.sh: no suite at $deqp (./mlx build cts)" >&2; exit 2; }
mkdir -p "$results"
# The watchdog, unless the arguments after `--` say how it is to be (the
# suite refuses an option given twice).
watchdog=(--deqp-watchdog=enable)
for argument in "${extra[@]}"; do [[ $argument != --deqp-watchdog* ]] || watchdog=(); done

# The suite with ARGUMENTS, in the directory it reads its data from, its
# output added to OUTPUT.
suite() { # OUTPUT ARGUMENTS...
    local output=$1
    shift
    (cd "$(dirname "$deqp")" && LD_LIBRARY_PATH="$library${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$deqp" \
        --deqp-compute-only=enable "${watchdog[@]}" --deqp-log-images=disable --deqp-surface-type=window \
        --deqp-spirv-validation=disable --deqp-log-decompiled-spirv=disable "$@" "${extra[@]}") >> "$output" 2>&1
}

status=0
for group in "${groups[@]}"; do
    if [[ $group == @* ]]; then
        list=${group#@}
        name=$(basename "$list" .txt)
    else
        list=""
        name=$(echo "$group" | tr '.*' '__' | sed 's/_*$//')
    fi
    log="$results/$name.qpa"
    out="$results/$name.out"
    rm -f "$log" "$out" "$results/$name".part* "$results/$name".cases* "$results/$name.notrun"
    # The group's cases first (the suite's own list of them, or the
    # file's); then the suite on those not run yet, again after a case
    # that ended it (a crash, its watchdog): what a case does to the
    # driver is told case by case, a crash among them.
    if [[ -n "$list" ]]; then
        [[ -f "$list" ]] || { echo "$group: no file $list"; status=2; continue; }
        sed 's/#.*//; s/[[:space:]]*$//; /^$/d' "$list" > "$results/$name.cases"
    else
        suite "$out" --deqp-case="$group" --deqp-runmode=txt-caselist --deqp-caselist-export-file="$results/$name.cases.txt"
        sed -n 's/^TEST: //p' "$results/$name.cases.txt" > "$results/$name.cases" 2> /dev/null
    fi
    if [[ ! -s "$results/$name.cases" ]]; then
        echo "$group: no case of the suite is named so: $(tail -3 "$out" | tr '\n' ' ')"
        status=2
        continue
    fi
    cp "$results/$name.cases" "$results/$name.cases.left"
    : > "$log"
    attempt=0
    while [[ -s "$results/$name.cases.left" ]]; do
        attempt=$((attempt + 1))
        part="$results/$name.part$attempt.qpa"
        suite "$out" --deqp-caselist-file="$results/$name.cases.left" --deqp-log-filename="$part"
        [[ -f "$part" ]] && cat "$part" >> "$log"
        sed -n 's/^#beginTestCaseResult //p' "$part" 2> /dev/null | sort > "$results/$name.cases.begun"
        if [[ ! -s "$results/$name.cases.begun" ]]; then break; fi
        grep -vxF -f "$results/$name.cases.begun" "$results/$name.cases.left" > "$results/$name.cases.next"
        mv "$results/$name.cases.next" "$results/$name.cases.left"
        rm -f "$part"
    done
    rm -f "$results/$name".cases.begun "$results/$name".cases.left
    if [[ ! -s "$log" ]]; then
        echo "$group: the suite wrote no result: $(tail -3 "$out" | tr '\n' ' ')"
        status=2
        continue
    fi
    # The cases that were to run and never began.
    sed -n 's/^#beginTestCaseResult //p' "$log" | sort -u > "$results/$name.cases.begun"
    grep -vxF -f "$results/$name.cases.begun" "$results/$name.cases" > "$results/$name.notrun"
    rm -f "$results/$name.cases.begun"
    summarize "$log" "$results/$name.notrun" || [[ $status -ne 0 ]] || status=1
    # Commands of the driver the port could not give the suite
    # (projects/cts/port/driver.mlx): the run is not the driver's whole.
    if grep -q '^mlxcts: .* is not given to the suite' "$out"; then
        grep '^mlxcts: .* is not given to the suite' "$out" | sort -u | sed 's/^/  port: /'
        [[ $status -ne 0 ]] || status=1
    fi
done
exit $status
