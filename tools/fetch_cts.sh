#!/usr/bin/env bash
# Fetches the sources of the Vulkan conformance suite's port
# (projects/cts/UPSTREAM.md) into a cache outside the repository, at the
# commits that file names, and holds each against its record:
#
#   VK-GL-CTS      vulkan-cts-1.3.10.0 (the suite)
#   glslang        the commit the suite's external/fetch_sources.py pins
#   SPIRV-Tools    likewise (its API headers only)
#
# SPIRV-Headers, which the suite pins too, is not fetched: nothing the
# port builds includes it (UPSTREAM.md says why).
#
# Each is a blobless clone of its GitHub repository with only the
# directories the port builds checked out (git's sparse checkout), never
# edited: the port is projects/cts. After the fetch the commit of each
# and the tree of every directory checked out must be the ones recorded,
# and the suite's fetch_sources.py must pin the two others to the
# commits recorded (so the record is the suite's own choice).
#
#   tools/fetch_cts.sh [--sources DIR] [--check]
#     --sources DIR  the cache (default: $CTS_SOURCES, else mlx-out/cts/sources)
#     --check        fetch nothing: hold what is there against the record
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
sources=${CTS_SOURCES:-$repo_root/mlx-out/cts/sources}
check_only=false
while [[ $# -gt 0 ]]; do
    case $1 in
    --sources) sources=$2; shift ;;
    --check) check_only=true ;;
    -h|--help) sed -n '2,/^set -euo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;;
    *) echo "fetch_cts.sh: unknown argument $1 (see --help)" >&2; exit 2 ;;
    esac
    shift
done
record=projects/cts/UPSTREAM.md
fail() { echo "FAIL $1" >&2; exit 1; }

# The record's lines `    NAME.FIELD: VALUE`.
field() {
    local value
    value=$(sed -n "s/^    $1: //p" "$record" | head -1)
    [[ -n "$value" ]] || fail "$record has no '$1'"
    echo "$value"
}

# NAME (its record's prefix): cloned into the cache, the directories of
# its `paths` checked out at its commit, then held against the record.
fetch() {
    local name=$1 url commit dir
    url=$(field "$name.url")
    commit=$(field "$name.commit")
    dir="$sources/$name"
    local -a paths
    read -r -a paths <<< "$(field "$name.paths")"
    if ! $check_only; then
        if [[ ! -d "$dir/.git" ]]; then
            mkdir -p "$sources"
            GIT_LFS_SKIP_SMUDGE=1 git clone -q --filter=blob:none --no-checkout "$url" "$dir" || fail "cannot clone $url"
        fi
        if ! git -C "$dir" cat-file -e "$commit^{commit}" 2> /dev/null; then
            git -C "$dir" fetch -q --filter=blob:none origin "$commit" || fail "cannot fetch $commit of $url"
        fi
        local -a cone=()
        local p
        for p in "${paths[@]}"; do cone+=("/$p"); done
        git -C "$dir" sparse-checkout set --no-cone "${cone[@]}"
        GIT_LFS_SKIP_SMUDGE=1 git -C "$dir" checkout -q --detach "$commit" || fail "cannot check out $commit of $name"
    fi
    [[ -d "$dir/.git" ]] || fail "$name is not fetched into $dir (run tools/fetch_cts.sh)"
    [[ "$(git -C "$dir" rev-parse HEAD)" == "$commit" ]] || fail "$name at $dir is not at $commit"
    # A tag in the record (the suite's: its release name in every result
    # file, projects/cts/build.mlx) names that commit, by the tag object
    # recorded.
    local tag
    tag=$(sed -n "s/^    $name\.tag: //p" "$record" | head -1)
    if [[ -n "$tag" ]]; then
        if ! $check_only && ! git -C "$dir" rev-parse -q --verify "refs/tags/$tag" > /dev/null; then
            git -C "$dir" fetch -q --filter=blob:none origin "refs/tags/$tag:refs/tags/$tag" || fail "cannot fetch the tag $tag of $url"
        fi
        [[ "$(git -C "$dir" rev-parse -q --verify "refs/tags/$tag")" == "$(field "$name.tag-object")" ]] || fail "$name's tag $tag is not the tag object recorded"
        [[ "$(git -C "$dir" rev-parse -q --verify "refs/tags/$tag^{commit}")" == "$commit" ]] || fail "$name's tag $tag does not name $commit"
    fi
    [[ "$(git -C "$dir" rev-parse 'HEAD^{tree}')" == "$(field "$name.tree")" ]] || fail "$name's tree is not the one recorded"
    [[ -z "$(git -C "$dir" status --porcelain)" ]] || fail "$name at $dir is changed (the sources are never edited)"
    echo "ok   $name $commit ($(field "$name.what"))"
}

fetch vk-gl-cts
cts="$sources/vk-gl-cts"
# The suite's own pins of what its build fetches.
pinned() { # REPOSITORY-URL-FRAGMENT
    awk -v want="$1" '$0 ~ want { getline; getline; gsub(/[ ",]/, ""); print; exit }' "$cts/external/fetch_sources.py"
}
for name in glslang spirv-tools; do
    fragment=$(field "$name.fetch-sources")
    [[ "$(pinned "$fragment")" == "$(field "$name.commit")" ]] || fail "$name: the suite pins $(pinned "$fragment"), the record says $(field "$name.commit")"
    fetch "$name"
done
echo "all CTS sources in $sources as recorded"
