#!/usr/bin/env bash
# The C and C++ part of the code map (tools/codemap/cpp.mlx): mlx-cc's
# index of what it compiles (mlx-cc --index, compiler/cc/index.mlx) into
# mlx-out/codemap/cpp, where mlx-codemap reads it. Indexing makes no
# object (each unit is compiled to /dev/null) and adds to what is there:
# a file indexed before, unchanged, is not again.
#
#   tools/codemap_index.sh               the repository's own C and C++:
#                                        compiler/cc's and mlxlibcpp's
#                                        tests and sources, the C tests of
#                                        compiler/libc and tests/support
#                                        (`./mlx build codemap-cpp-index`)
#   tools/codemap_index.sh --build STEP...
#                                        the C and C++ units of build
#                                        steps as their commands compile
#                                        them (`--build webkit-mlx-webcore`:
#                                        WebCore, the port included, as
#                                        compiled for the port), two at a
#                                        time while 5 GB are free
#
# A build indexes as it goes with MLX_CC_INDEX in its environment (the
# commands and so the result cache's keys stay as they are):
#     MLX_CC_INDEX=$PWD/mlx-out/codemap/cpp ./mlx build webkit
# Units the cache gave or that were current are not compiled then;
# --build indexes them all.
#
# MLX_CODEMAP_INDEX_DIR=DIR writes elsewhere (mlx-codemap reads the
# directories of MLX_CODEMAP_CPP too).
set -uo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
cc="$repo_root/mlx-out/bin/mlx-cc"
[[ -x "$cc" ]] || { echo "codemap_index.sh: ./mlx build cc first" >&2; exit 2; }
export MLX_CC_INDEX=${MLX_CODEMAP_INDEX_DIR:-$repo_root/mlx-out/codemap/cpp}
mkdir -p "$MLX_CC_INDEX"
indexed=0
# The units that did not compile, by name (to see why: compile one).
failures="${MLX_CC_INDEX%/}.failed"
: > "$failures"
failed=0

# Waits until 5 GB of memory are free (a WebCore unit takes 2.5-4 GB).
wait_for_memory() {
  while (( $(awk '/MemAvailable/ { print int($2 / 1048576) }' /proc/meminfo) < 5 )); do sleep 2; done
}

own() {
  local includes=(-I compiler/cc/tests)
  local dir
  for dir in mlx-out/include/*/; do includes+=(-I "${dir%/}"); done
  local files=()
  local file
  for file in compiler/cc/tests/*.c compiler/cc/tests/*.cpp compiler/cc/tests/*/*.cpp compiler/libcpp/*.cpp \
              compiler/libc/*/tests/*.c compiler/libc/tests/*.c tests/support/*.c tests/support/*.cpp tools/mlxlibc_check.c; do
    [[ -f "$file" ]] && files+=("$file")
  done
  for file in "${files[@]}"; do
    local name=${file##*/}
    local flags=()
    local stem=${file%.*}
    [[ -f "$stem.flags" ]] && read -r -a flags < "$stem.flags"
    if "$cc" "${flags[@]}" "${includes[@]}" -I "$(dirname "$file")" -c "$file" -o /dev/null > /dev/null 2>&1; then
      indexed=$((indexed + 1))
    else
      failed=$((failed + 1))
      echo "$file" >> "$failures"
    fi
  done
}

# The compile commands of STEPs (mlx build --dry-run --verbose prints
# them), each run to /dev/null with the index on.
build() {
  local lines
   lines=$(./mlx build --dry-run --verbose --force "$@" 2>&1 | grep '^    ' | grep -E '(^|/)mlx-cc ' | grep -e ' -c ')
  [[ -n "$lines" ]] || { echo "codemap_index.sh: no compile commands in $*" >&2; return; }
  local running=0
  local line
  while IFS= read -r line; do
    local words
    read -r -a words <<< "$line"
    local args=()
    local i
    for ((i = 1; i < ${#words[@]}; i++)); do
      case "${words[$i]}" in
        -MD|-MMD) ;;
        -MF|-MT|-o) i=$((i + 1)) ;;
        *) args+=("${words[$i]}") ;;
      esac
    done
    wait_for_memory
    ( "$cc" "${args[@]}" -o /dev/null > /dev/null 2>&1 ) &
    running=$((running + 1))
    if (( running >= 2 )); then
      if wait -n; then indexed=$((indexed + 1)); else failed=$((failed + 1)); fi
      running=$((running - 1))
    fi
  done <<< "$lines"
  while (( running > 0 )); do
    if wait -n; then indexed=$((indexed + 1)); else failed=$((failed + 1)); fi
    running=$((running - 1))
  done
}

if [[ "${1:-}" == "--build" ]]; then
  shift
  build "$@"
else
  own
fi
echo "codemap_index.sh: $indexed units indexed, $failed did not compile; $(ls "$MLX_CC_INDEX" | wc -l) shards in $MLX_CC_INDEX"
