#!/usr/bin/env bash
# tools/lattice_why.sh DIR [N]
#
# What the functions held back in a translated program wait for, from the
# units' lists in DIR/index (tools/lattice_translate.sh --out DIR): each
# function held back on its own account once, ranked by the record or the
# construct it waits for (the N most frequent, 30 without N). A function
# held back because it calls one of these is not counted: it waits with
# it.
set -u
directory=${1:?lattice_why.sh: the directory of a translation}
count=${2:-30}
[[ -d $directory/index ]] || { echo "lattice_why.sh: $directory/index is missing"; exit 1; }
cat "$directory"/index/*.idx | awk -F'\t' '$1 == "W" && !seen[$2]++ { print $3 "\t" $5 "\t" $4 }' | sort | uniq -c | sort -rn | head -"$count"
