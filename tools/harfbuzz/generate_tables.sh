#!/bin/sh
# Makes the tables of the shaping engine (std/src/shaping) of HarfBuzz 8.3's
# source (the directory of harfbuzz-8.3.0.tar.xz unpacked, $1):
#   indictable.mlx    the Indic categories and positions (indic_table.py,
#                     of hb-ot-shaper-indic-table.cc preprocessed)
#   usetable.mlx      the Universal Shaping Engine's categories (use_table.py,
#                     of hb-ot-shaper-use-table.hh preprocessed)
#   machines.mlx      the syllable machines of the Indic, Khmer, Myanmar and
#                     Universal shapers (machines.py, of Ragel's output)
#   voweltable.mlx    the vowel constraints (vowel_constraints.py)
#   arabictable.mlx   Arabic joining, shaping and ligatures (arabic_table.py)
#   arabicpua.mlx     the Arabic private-use code points (arabic_pua.py)
#   tagtable.mlx      OpenType language tags (tag_table.py)
#   cfftable.mlx      CFF's standard strings, encoding, charsets
#                     (cff_strings.py)
#   nametable.mlx     the languages of the name table (name_languages.py)
# The preprocessing needs a C++ compiler (CXX, g++ by default).
#
#   tools/harfbuzz/generate_tables.sh HARFBUZZ_SOURCE
set -eu
repo_root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$repo_root"
HB_SRC=$(cd "$1/src" && pwd)
HB_WORK=$(mktemp -d /tmp/mlx-harfbuzz-tables.XXXXXX)
export HB_SRC HB_WORK
trap 'rm -rf -- "$HB_WORK"' EXIT
cxx=${CXX:-g++}
"$cxx" -E -P -x c++ -std=c++11 -I"$HB_SRC" "$HB_SRC/hb-ot-shaper-indic-table.cc" -o "$HB_WORK/indic-table.i"
printf '#include "hb-ot-shaper-use-table.hh"\n' > "$HB_WORK/use-table.cc"
"$cxx" -E -P -x c++ -I"$HB_SRC" "$HB_WORK/use-table.cc" -o "$HB_WORK/use-table.i"
for generator in indic_table use_table machines vowel_constraints arabic_table arabic_pua tag_table cff_strings name_languages; do
    python3 "tools/harfbuzz/$generator.py"
done
