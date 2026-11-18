#!/usr/bin/env bash
# Checks the Vulkan conformance suite's port (projects/cts), in two parts
# that tests/521 and tests/522 read:
#
#   tools/check_cts.sh --framework
#       the framework's own self-tests (delibs: debase, depool, dethread,
#       deutil, decpp; a test log of qphelper written and read back),
#       compiled by mlx-cc from the fetched sources with
#       projects/cts/port/framework_selftest.cpp, linked by mlx-ld on
#       mlxlibc with mlxlibcpp, and run; what they say in
#       mlx-out/cts/results/framework.txt
#   tools/check_cts.sh [--no-build] [--deqp PROGRAM] [--library DIR]
#       deqp-vk built (./mlx build cts, unless --no-build) and the cases of
#       projects/cts/check-cases.txt run by tools/run_cts.sh against our
#       driver over the host card, as one group: mlx-out/cts/results/
#       check-cases.qpa and its summary (--deqp and --library as
#       run_cts.sh has them, for a suite built elsewhere); before the run,
#       that deqp-vk is not older than an object it is linked from
#   tools/check_cts.sh --runner
#       tools/run_cts.sh held against a stand-in for the suite
#       (tests/support/cts_standin_suite.sh, no sources needed): a case
#       that passes, fails, is not supported by the device or for want of
#       the port, crashes the suite, comes after the crash, and one the
#       suite does not have, each told as it ended (tests/522)
#
# The sources: CTS_SOURCES, else mlx-out/cts/sources (tools/fetch_cts.sh).
# The toolchain of --framework: MLX_CC (default mlx-out/bin/mlx-cc),
# MLX_LIBC (mlx-out/libc, mlxlibc's loader and library) and MLX_LIB
# (mlx-out/lib, mlxlibcpp). Exits 0 when every self-test or every case
# passed.
set -uo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
sources=${CTS_SOURCES:-$repo_root/mlx-out/cts/sources}
results=$repo_root/mlx-out/cts/results
mode=suite
build=true
run_options=()
while [[ $# -gt 0 ]]; do
    case $1 in
    --framework) mode=framework ;;
    --runner) mode=runner ;;
    --no-build) build=false ;;
    --deqp|--library) run_options+=("$1" "$2"); shift ;;
    -h|--help) sed -n '2,/^set -uo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;;
    *) echo "check_cts.sh: unknown argument $1 (see --help)" >&2; exit 2 ;;
    esac
    shift
done
mkdir -p "$results"

if [[ $mode == runner ]]; then
    # The paths relative, as one types them: run_cts.sh makes them whole.
    work=mlx-out/cts/runner
    rm -rf "$work"
    mkdir -p "$work"
    printf '%s\n' "# the stand-in's cases, and one it does not have" s.pass s.fail s.unsupported s.port s.crash s.after s.unknown > "$work/standin.txt"
    tools/run_cts.sh --deqp tests/support/cts_standin_suite.sh --results "$work/results" "@$work/standin.txt" > "$work/summary.txt" 2>&1
    status=$?
    cat "$work/summary.txt"
    says() { grep -qxF -- "$1" "$work/summary.txt" || { echo "FAIL tools/run_cts.sh does not say: $1"; exit 1; }; }
    [[ $status -eq 1 ]] || { echo "FAIL tools/run_cts.sh exits $status, not 1, with cases that failed"; exit 1; }
    says "$repo_root/$work/results/standin.qpa: 7 cases: 2 passed, 3 failed, 2 not supported (1 of them for want of the port, not of the driver), 1 of the failed not run"
    says "  failed: s.fail (Fail): the value is wrong"
    says "  failed: s.crash (Crash): the suite stopped in the case"
    says "  failed: s.unknown (NotRun): the suite never began the case"
    says "  not supported (1): the feature is not supported"
    says "  not supported (1): Amber is not built in this port: the case's script is not run"
    says "  port: mlxcts: the driver's vkCmdSetLineWidth is not given to the suite: it takes a float by value, which the library's entries do not carry"
    [[ $(grep -c '^  failed: ' "$work/summary.txt") -eq 3 ]] || { echo "FAIL tools/run_cts.sh tells other failures"; exit 1; }
    # Every case the stand-in began is in the group's result file once.
    for c in s.pass s.fail s.unsupported s.port s.crash s.after; do
        [[ $(grep -cxF "#beginTestCaseResult $c" "$work/results/standin.qpa") -eq 1 ]] || { echo "FAIL $c is not in the result file once"; exit 1; }
    done
    echo "check_cts.sh: tools/run_cts.sh tells every case of the stand-in suite as it ended"
    exit 0
fi

# The sources, for what is compiled here (the self-tests, the suite).
suite=$sources/vk-gl-cts
if [[ $mode == framework ]] || $build; then
    [[ -f "$suite/framework/delibs/debase/deDefs.h" ]] || { echo "FAIL no conformance suite sources in $sources (tools/fetch_cts.sh)" >&2; exit 2; }
fi

if [[ $mode == framework ]]; then
    # The repository's own mlx-cc is built first, so that the self-tests
    # are compiled by the compiler of this tree (not by one an earlier
    # build left in mlx-out); another one given by MLX_CC is taken as it is.
    if [[ -z "${MLX_CC:-}" ]]; then
        ./mlx build cc > "$repo_root/mlx-out/cts/cc-build.log" 2>&1 || { echo "FAIL ./mlx build cc: $(tail -5 "$repo_root/mlx-out/cts/cc-build.log")" >&2; exit 2; }
    fi
    cc=${MLX_CC:-$repo_root/mlx-out/bin/mlx-cc}
    libc=${MLX_LIBC:-$repo_root/mlx-out/libc}
    lib=${MLX_LIB:-$repo_root/mlx-out/lib}
    [[ -x "$cc" ]] || { echo "FAIL no mlx-cc at $cc (./mlx build cc)" >&2; exit 2; }
    work=$repo_root/mlx-out/cts/framework
    rm -rf "$work"
    mkdir -p "$work"
    f=$suite/framework
    include=""
    for d in delibs/debase delibs/depool delibs/dethread delibs/deutil delibs/decpp delibs/destream qphelper; do
        include="$include -I$f/$d"
    done
    defines="-DDE_ASSERT_FAILURE_CALLBACK -D_XOPEN_SOURCE=600 -D_GNU_SOURCE -DDEQP_TARGET_NAME=\"Mlx\""
    c_sources=(
        debase/deDefs.c debase/deFloat16.c debase/deFloat16Test.c debase/deInt32.c debase/deInt32Test.c
        debase/deMath.c debase/deMathTest.c debase/deMemory.c debase/deRandom.c debase/deString.c debase/deSha1.c
        depool/deMemPool.c depool/dePoolArray.c depool/dePoolHeap.c depool/dePoolHash.c depool/dePoolHashArray.c
        depool/dePoolHashSet.c depool/dePoolMultiSet.c depool/dePoolSet.c depool/dePoolStringBuilder.c depool/dePoolTest.c
        dethread/deAtomic.c dethread/deSingleton.c dethread/deThreadTest.c dethread/unix/deMutexUnix.c
        dethread/unix/deSemaphoreUnix.c dethread/unix/deThreadUnix.c dethread/unix/deThreadLocalUnix.c
        deutil/deClock.c deutil/deCommandLine.c deutil/deFile.c deutil/deProcess.c deutil/deDynamicLibrary.c
    )
    cpp_sources=(
        deAppendList deArrayBuffer deArrayUtil deBlockBuffer deCommandLine deDefs deDirectoryIterator
        deDynamicLibrary deFilePath deMemPool deMeta deMutex dePoolArray dePoolString deProcess deRandom
        deRingBuffer deSemaphore deSTLUtil deSharedPtr deStringUtil deThread deThreadLocal
        deThreadSafeRingBuffer deUniquePtr deSpinBarrier deSha1
    )
    objects=()
    for s in "${c_sources[@]}"; do
        o="$work/$(echo "$s" | tr '/' '_' | sed 's/\.c$/-c.o/')"
        "$cc" -std=c11 $include $defines -c "$f/delibs/$s" -o "$o" 2> "$o.log" || { echo "FAIL mlx-cc: $s: $(head -2 "$o.log")"; exit 1; }
        objects+=("$o")
    done
    for s in qpDebugOut qpInfo qpTestLog qpXmlWriter; do
        o="$work/qphelper_$s-c.o"
        "$cc" -std=c11 $include $defines -c "$f/qphelper/$s.c" -o "$o" 2> "$o.log" || { echo "FAIL mlx-cc: qphelper/$s.c: $(head -2 "$o.log")"; exit 1; }
        objects+=("$o")
    done
    for s in "${cpp_sources[@]}" ; do
        o="$work/decpp_$s.o"
        "$cc" -std=c++17 -fexceptions $include $defines -c "$f/delibs/decpp/$s.cpp" -o "$o" 2> "$o.log" || { echo "FAIL mlx-cc: decpp/$s.cpp: $(head -2 "$o.log")"; exit 1; }
        objects+=("$o")
    done
    o="$work/framework_selftest.o"
    "$cc" -std=c++17 -fexceptions $include $defines -c projects/cts/port/framework_selftest.cpp -o "$o" 2> "$o.log" || { echo "FAIL mlx-cc: framework_selftest.cpp: $(head -2 "$o.log")"; exit 1; }
    objects+=("$o")
    "$cc" -o "$work/framework-selftest" "${objects[@]}" --dynamic-linker="$libc/libmlxc.so.1" \
        -L"$libc" -L"$lib" -lmlxcpp -lm 2> "$work/link.log" || { echo "FAIL mlx-ld: $(head -3 "$work/link.log")"; exit 1; }
    (cd "$work" && timeout 600 ./framework-selftest) > "$results/framework.txt" 2>&1
    status=$?
    cat "$results/framework.txt"
    [[ $status -eq 0 ]] || { echo "FAIL the framework's self-tests (exit $status)"; exit 1; }
    echo "check_cts.sh: the framework's self-tests passed, built by our toolchain"
    exit 0
fi

if $build; then
    CTS_SOURCES=$sources ./mlx build cts > "$results/build.log" 2>&1 || { echo "FAIL ./mlx build cts: $(tail -5 "$results/build.log")"; exit 1; }
fi
rm -f "$results/check-cases.qpa"
tools/run_cts.sh "${run_options[@]}" --results "$results" @projects/cts/check-cases.txt
