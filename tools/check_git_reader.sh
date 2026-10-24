#!/usr/bin/env bash
# std.git (std/src/git.mlx), the reader of git repositories in Mlx,
# against git itself (the reference, and the maker of the repositories it
# reads):
#
#   - tests/413_git_runtime.mlx's own checks (names, deltas, commits
#     parsed, this checkout read);
#   - a repository made here: commits packed with OFS_DELTA deltas (git
#     repack -adf), more packed with REF_DELTA ones in a second pack
#     (repack.useDeltaBaseOffset=false), a branch merged (--no-ff),
#     packed-refs (git pack-refs --all, HEAD's branch then in packed-refs
#     alone), loose commits and a loose branch after; read from its root
#     and a directory below: HEAD and `git rev-list --count HEAD` as git
#     says, and every object (`git cat-file --batch-all-objects`) of the
#     kind and size git says, its bytes hashing to its name;
#   - a worktree of it on another branch with commits of its own (its .git
#     file, commondir), one with a detached HEAD, a bare clone, a clone
#     sharing its objects (objects/info/alternates), a shallow clone
#     (--depth 7: the count stops at .git/shallow and says so), and a
#     directory in no repository;
#   - this checkout: HEAD, its count (a shallow clone's too) and the
#     objects of its packs (those under 64 KiB);
#   - build numbers (git.buildNumberOf) against git's count since the
#     commit that last changed VERSION (`git rev-list --count HEAD
#     ^COMMIT` + 1; every commit before the file ever changed): VERSION
#     made, changed on a branch and merged, changed by a commit, a
#     VERSION in a directory;
#   - a git submodule (its .git file into the superproject's
#     .git/modules/NAME), counted alone.
#
#   tools/check_git_reader.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
command -v git > /dev/null || { echo "check_git_reader.sh: git is not installed (it makes the repositories and says what they hold)" >&2; exit 2; }
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
work=$(mktemp -d /tmp/mlx-git.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }

"$compiler" --quiet tests/413_git_runtime.mlx -o "$work/413" > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "tests/413_git_runtime.mlx does not compile"; }
status=0
"$work/413" || status=$?
[[ $status -eq 13 ]] || fail "tests/413_git_runtime.mlx: check $status failed"
echo "ok   tests/413_git_runtime.mlx: names, deltas, commits, this checkout"

# The repositories, made the same on every machine: no configuration of
# the user's or the system's, fixed names and times.
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_NOSYSTEM=1
export GIT_AUTHOR_NAME="A U Thor" GIT_AUTHOR_EMAIL=author@example.com
export GIT_COMMITTER_NAME="C O Mitter" GIT_COMMITTER_EMAIL=committer@example.com
tick=1700000000
commit() {
    local directory=$1 message=$2
    tick=$((tick + 60))
    GIT_AUTHOR_DATE="@$tick +0200" GIT_COMMITTER_DATE="@$tick +0200" git -C "$directory" commit -q -a -m "$message"
}
# A file that grows a little each time (so that packs hold deltas).
change() {
    local directory=$1 n=$2
    seq 1 $((n * 40)) | sed "s/^/line of commit $n: /" > "$directory/text.txt"
    echo "file $n" > "$directory/file-$((n % 5)).txt"
    git -C "$directory" add -A
}

repo=$work/repo
git init -q -b main "$repo"
mkdir -p "$repo/sub/dir"
echo "below" > "$repo/sub/dir/below.txt"
for n in $(seq 1 30); do change "$repo" "$n"; commit "$repo" "commit $n"; done
git -C "$repo" repack -q -adf --depth=50 --window=50
for n in $(seq 31 45); do change "$repo" "$n"; commit "$repo" "commit $n"; done
git -C "$repo" -c repack.useDeltaBaseOffset=false repack -q -d
packs=$(ls "$repo"/.git/objects/pack/*.pack | wc -l)
[[ $packs -eq 2 ]] || fail "the repository has $packs packs, not 2"
for idx in "$repo"/.git/objects/pack/*.idx; do
    deltas=$(git verify-pack -v "$idx" | awk 'NF == 7' | wc -l)
    [[ $deltas -gt 0 ]] || fail "$(basename "$idx") has no deltas"
done
git -C "$repo" checkout -q -b side HEAD~5
for n in $(seq 46 50); do change "$repo" "$n"; commit "$repo" "side $n"; done
git -C "$repo" checkout -q main
tick=$((tick + 60))
GIT_AUTHOR_DATE="@$tick +0200" GIT_COMMITTER_DATE="@$tick +0200" git -C "$repo" merge -q --no-ff -X theirs -m "merge side" side > /dev/null
git -C "$repo" pack-refs --all
[[ ! -e "$repo/.git/refs/heads/main" ]] || fail "refs/heads/main is still a file after pack-refs"
# Runs the test with the checks of $work/checks (and keeps them).
checks=0
run_checks() {
    local status=0
    "$work/413" "$work/checks" || status=$?
    [[ $status -eq 13 ]] || { cat "$work/checks" >&2; fail "tests/413_git_runtime.mlx $work/checks: check $status failed"; }
    checks=$((checks + $(grep -c '^[a-z]' "$work/checks")))
}
head_line() {
    local directory=$1 git_directory=${2:-$1}
    local shallow=0
    [[ $(git -C "$git_directory" rev-parse --is-shallow-repository) == true ]] && shallow=1
    echo "head $directory $(git -C "$git_directory" rev-parse HEAD) $(git -C "$git_directory" rev-list --count HEAD) $shallow"
}
objects_line() {
    local directory=$1 name=$2
    git -C "$directory" cat-file --batch-all-objects --batch-check > "$work/$name.objects"
    echo "objects $directory $work/$name.objects"
}
{
    echo "# HEAD's branch in packed-refs alone, a merge, two packs"
    head_line "$repo"
    head_line "$repo/sub/dir" "$repo"
} > "$work/checks"
run_checks
for n in 51 52; do change "$repo" "$n"; commit "$repo" "loose $n"; done
loose=$(find "$repo/.git/objects" -path '*/pack' -prune -o -type f -name '[0-9a-f]*' -print | wc -l)
[[ $loose -gt 0 ]] || fail "the repository has no loose objects"
git -C "$repo" worktree add -q -b feature "$work/worktree" side
for n in 53 54 55; do change "$work/worktree" "$n"; commit "$work/worktree" "feature $n"; done
git -C "$repo" worktree add -q --detach "$work/detached" HEAD~7
GIT_AUTHOR_DATE="@$tick +0200" GIT_COMMITTER_DATE="@$tick +0200" git -C "$repo" tag -a -m "a tag" v1 HEAD~2
git clone -q --bare "$repo" "$work/bare.git"
git clone -q --shared "$repo" "$work/shared"
[[ -s "$work/shared/.git/objects/info/alternates" ]] || fail "the shared clone has no alternates"
git clone -q --depth 7 "file://$repo" "$work/shallow"
mkdir -p "$work/plain/below"
git -C "$work/plain/below" rev-parse --git-dir > /dev/null 2>&1 && fail "$work/plain is in a repository"
{
    echo "# loose commits, its own branch file again"
    head_line "$repo"
    objects_line "$repo" repo
    echo "# worktrees: their .git files, commondir"
    head_line "$work/worktree"
    head_line "$work/detached"
    objects_line "$work/worktree" worktree
    echo "# a bare clone, a clone with alternates, a shallow one"
    head_line "$work/bare.git"
    head_line "$work/shared"
    objects_line "$work/shared" shared
    head_line "$work/shallow"
    objects_line "$work/shallow" shallow
    echo "none $work/plain/below"
    echo "# this checkout, and the objects of its packs"
    head_line "$repo_root"
} > "$work/checks"
common=$(cd "$repo_root" && cd "$(git rev-parse --git-common-dir)" && pwd)
for idx in "$common"/objects/pack/*.idx; do
    git show-index < "$idx" | awk '{ print $2 }'
done | git -C "$repo_root" cat-file --batch-check | awk '$3 < 65536' > "$work/checkout.objects"
echo "objects $repo_root $work/checkout.objects" >> "$work/checks"
run_checks
echo "ok   HEADs and counts as git says, $(cat "$work"/*.objects | wc -l) objects read (two packs: OFS_DELTA and REF_DELTA, loose, alternates, shallow), a directory without a repository"

# Build numbers: VERSION's first line, and the commits since the commit
# that last changed it (that one counted: build 1), all of them before
# it ever changed (its first commit is not a change). The commit is found
# with git as the last one on HEAD's first-parent line whose VERSION
# differs from its first parent's, where that one has the file.
build_line() {
    local directory=$1 path=$2
    local changed build
    changed=$(git -C "$directory" log --first-parent -1 --format=%H -- "$path")
    if [[ -n "$changed" ]] && git -C "$directory" cat-file -e "$changed^:$path" 2> /dev/null; then
        build=$(( $(git -C "$directory" rev-list --count HEAD "^$changed") + 1 ))
    else
        build=$(git -C "$directory" rev-list --count HEAD)
    fi
    echo "build $directory $path $(git -C "$directory" show "HEAD:$path" | head -1) $build $(git -C "$directory" rev-parse HEAD)"
}
versioned=$work/versioned
git init -q -b main "$versioned"
for n in 1 2 3; do change "$versioned" "$n"; commit "$versioned" "before VERSION $n"; done
echo 0.1 > "$versioned/VERSION"
mkdir -p "$versioned/project"
echo 1.4 > "$versioned/project/VERSION"
change "$versioned" 4; commit "$versioned" "VERSION 0.1"
for n in 5 6; do change "$versioned" "$n"; commit "$versioned" "0.1 $n"; done
{
    echo "# VERSION made, never changed: every commit"
    build_line "$versioned" VERSION
    build_line "$versioned" project/VERSION
} > "$work/checks"
run_checks
[[ $(tail -2 "$work/checks" | head -1 | cut -d' ' -f4,5) == "0.1 6" ]] || fail "the first build line is not 0.1, build 6"
git -C "$versioned" checkout -q -b side
echo 0.2 > "$versioned/VERSION"
change "$versioned" 7; commit "$versioned" "side: VERSION 0.2"
change "$versioned" 8; commit "$versioned" "side 8"
git -C "$versioned" checkout -q main
for n in 9 10; do change "$versioned" "$n"; commit "$versioned" "main $n"; done
tick=$((tick + 60))
GIT_AUTHOR_DATE="@$tick +0200" GIT_COMMITTER_DATE="@$tick +0200" git -C "$versioned" merge -q --no-ff -X theirs -m "merge 0.2" side > /dev/null
for n in 11 12; do change "$versioned" "$n"; commit "$versioned" "0.2 $n"; done
echo 1.5 > "$versioned/project/VERSION"
change "$versioned" 13; commit "$versioned" "project 1.5"
{
    echo "# VERSION changed by a merge, project/VERSION by a commit"
    build_line "$versioned" VERSION
    build_line "$versioned" project/VERSION
} > "$work/checks"
run_checks
[[ $(head -2 "$work/checks" | tail -1 | cut -d' ' -f4,5) == "0.2 4" ]] || fail "after the merge the build line is not 0.2, build 4"
[[ $(tail -1 "$work/checks" | cut -d' ' -f4,5) == "1.5 1" ]] || fail "the commit changing project/VERSION is not build 1"
echo 0.3 > "$versioned/VERSION"
change "$versioned" 14; commit "$versioned" "VERSION 0.3"
for n in 15 16; do change "$versioned" "$n"; commit "$versioned" "0.3 $n"; done
git -C "$versioned" repack -q -adf
build_line "$versioned" VERSION > "$work/checks"
run_checks
[[ $(cut -d' ' -f4,5 "$work/checks") == "0.3 3" ]] || fail "after 0.3 the build line is not 0.3, build 3"
echo "ok   build numbers as git counts them: VERSION made (no reset), changed on a branch and merged, changed by a commit, a VERSION in a directory, packed"

# A submodule: its .git file names the superproject's .git/modules/NAME;
# it is counted alone, and the superproject apart.
super=$work/super
git init -q -b main "$super"
change "$super" 1; commit "$super" "super 1"
git -C "$super" -c protocol.file.allow=always submodule add -q "file://$versioned" module > /dev/null 2>&1 || fail "git submodule add"
commit "$super" "the submodule"
[[ -f "$super/module/.git" ]] || fail "the submodule has no .git file"
{
    head_line "$super/module"
    build_line "$super/module" VERSION
    head_line "$super"
    objects_line "$super/module" module
} > "$work/checks"
run_checks
echo "ok   a submodule (its .git file into the superproject's .git/modules) counted on its own, and the superproject"
echo "check_git_reader.sh: all checks passed"
