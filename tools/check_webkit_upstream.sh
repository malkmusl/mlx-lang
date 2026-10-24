#!/usr/bin/env bash
# projects/webkit/upstream is WPE WebKit as released, never modified
# (projects/webkit/UPSTREAM.md, PORT.md "The principle: not a fork"):
#
# - the tree in the commit checked out is the one UPSTREAM.md records
#   (`git rev-parse HEAD:projects/webkit/upstream`): no commit after the
#   import changed it but an upgrade, which records its new tree;
# - the last commit that touched the tree also changed UPSTREAM.md (an
#   upgrade is one commit: the tree and its record together);
# - the working tree has no change in it: nothing modified, deleted,
#   staged, untracked or ignored (a generator's __pycache__, an editor's
#   backup, a build's output written into it);
# - when the tarball is at hand (WEBKIT_TARBALL, by default UPSTREAM.md's
#   `tarball:` in /home/user/csrc/wpe), it is the one recorded (size,
#   SHA-256), and the committed tree matches it file by file: the same
#   paths, contents (as git's blob hashes), executable bits and symbolic
#   links.
#
#   tools/check_webkit_upstream.sh [--no-tarball]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
fail() { echo "FAIL $1" >&2; exit 1; }

tree_dir=projects/webkit/upstream
record=projects/webkit/UPSTREAM.md
use_tarball=1
[ "${1:-}" = --no-tarball ] && use_tarball=0

[ -d "$tree_dir/Source" ] || fail "$tree_dir: no WebKit tree"
[ -f "$record" ] || fail "$record: missing"
field() { sed -n "s/^    $1: //p" "$record" | head -1; }
recorded_tree=$(field tree)
version=$(field version)
tarball_name=$(field tarball)
tarball_size=$(field tarball-size)
tarball_sha256=$(field tarball-sha256)
tarball_top=$(field tarball-top)
[ -n "$recorded_tree" ] && [ -n "$tarball_name" ] && [ -n "$tarball_sha256" ] && [ -n "$tarball_top" ] || fail "$record: the import's block (tree, tarball, tarball-sha256, tarball-top) is incomplete"

# The committed tree.
tree=$(git rev-parse "HEAD:$tree_dir" 2>/dev/null) || fail "$tree_dir is not in the commit checked out"
[ "$tree" = "$recorded_tree" ] || fail "$tree_dir in HEAD is tree $tree, not the $version imported ($recorded_tree): a commit changed it ($(git log -1 --format='%h %s' -- "$tree_dir")); WebKit is upgraded as UPSTREAM.md says, never modified"
echo "ok   $tree_dir in HEAD is the tree of $version as imported ($tree)"

# (The import itself came before its record; an upgrade cannot name its
# own commit, so it changes the record in it instead.)
imported=$(field imported | sed -n 's/.*commit \([0-9a-f]*\).*/\1/p')
last=$(git log -1 --format=%H -- "$tree_dir")
if [ -n "$imported" ] && [ "${last#"$imported"}" != "$last" ]; then
    echo "ok   the last commit that touched it is the import ($(git log -1 --format='%h %s' "$last"))"
elif git rev-parse -q --verify "$last^" > /dev/null && ! git diff --quiet "$last^" "$last" -- "$record"; then
    echo "ok   the last commit that touched it ($(git log -1 --format=%h "$last")) recorded it in $record"
else
    fail "the last commit that touched $tree_dir ($(git log -1 --format='%h %s' "$last")) is not the import and did not change $record: an upgrade records its tree in the same commit"
fi

# The working tree.
changes=$(git status --porcelain --ignored --untracked-files=all -- "$tree_dir" | head -20)
[ -z "$changes" ] || fail "$tree_dir has changes that are not WebKit's (git status):
$changes"
echo "ok   the working tree has no change, untracked or ignored file in it"

# The tarball.
tarball=${WEBKIT_TARBALL:-/home/user/csrc/wpe/$tarball_name}
if [ "$use_tarball" -eq 0 ] || [ ! -f "$tarball" ]; then
    echo "skip the comparison with the release's tarball: no $tarball (WEBKIT_TARBALL)"
    echo "check_webkit_upstream.sh: all checks passed"
    exit 0
fi
size=$(stat -c %s "$tarball")
[ "$size" = "$tarball_size" ] || fail "$tarball is $size bytes, not the $tarball_size recorded"
sum=$(sha256sum "$tarball" | cut -d' ' -f1)
[ "$sum" = "$tarball_sha256" ] || fail "$tarball's SHA-256 is $sum, not the $tarball_sha256 recorded"
echo "ok   $tarball is the release recorded ($size bytes, SHA-256 $sum)"

# Each file of the tarball as git would hold it (mode, blob hash, path),
# beside each of the committed tree, in one order; a member the tarball
# holds twice must be the same both times. (Python's tarfile reads the
# archive as a stream, hashing each member as git does.)
work=$(mktemp -d "${TMPDIR:-/tmp}/mlx-webkit-upstream.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
python3 -I - "$tarball" "$tarball_top" > "$work/tarball" 2> "$work/tarball.err" <<'PY' || fail "reading $tarball: $(head -5 "$work/tarball.err")"
import hashlib, sys, tarfile
path, top = sys.argv[1], sys.argv[2] + "/"
entries = {}
with tarfile.open(path, "r:*") as archive:
    for member in archive:
        if member.isdir():
            continue
        if not member.name.startswith(top):
            sys.exit("a member outside " + top + ": " + member.name)
        name = member.name[len(top):]
        if member.issym():
            mode, data = "120000", member.linkname.encode()
        elif member.isfile():
            mode = "100755" if member.mode & 0o111 else "100644"
            data = archive.extractfile(member).read()
        else:
            sys.exit("a member that is neither a file nor a symbolic link: " + member.name)
        blob = hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()
        if name in entries and entries[name] != (mode, blob):
            sys.exit("a member twice, otherwise the second time: " + name)
        entries[name] = (mode, blob)
for name in sorted(entries):
    mode, blob = entries[name]
    sys.stdout.write("%s %s\t%s\n" % (mode, blob, name))
PY
git ls-tree -r "$tree" | sed 's/ blob / /' | LC_ALL=C sort -t "$(printf '\t')" -k2 > "$work/tree"
LC_ALL=C sort -t "$(printf '\t')" -k2 "$work/tarball" > "$work/tarball.sorted"
if ! cmp -s "$work/tarball.sorted" "$work/tree"; then
    diff "$work/tarball.sorted" "$work/tree" | head -20 >&2
    fail "the committed tree differs from $tarball (< the tarball, > the tree)"
fi
echo "ok   the tree matches $tarball file by file ($(wc -l < "$work/tree") files)"
echo "check_webkit_upstream.sh: all checks passed"
