#!/bin/sh
# Makes the test fonts of tools/check_harfbuzz.sh in directory $1, from the
# fonts of a Debian or Ubuntu system (fonts-dejavu-core, fonts-freefont-ttf;
# a font whose source is missing is not made):
#   Syllables.ttf      a small font for the syllable-based shapers
#                      (make_syllables.py)
#   MathTest.ttf       DejaVu Sans with a MATH table (make_math.py)
#   VarTest.ttf        DejaVu Sans as a variable font (make_variable.py)
#   FreeSerifOld.ttf   FreeSerif and FreeSans with the old Indic script tags
#   FreeSansOld.ttf    (dev2 as dev1 and so on: the shapers' fallbacks)
#   FreeSerifUse.ttf   FreeSerif and FreeSans with the Indic scripts as the
#   FreeSansUse.ttf    Universal Shaping Engine's tags (dev3...)
#   FreeSerifMyKh.ttf  FreeSerif with Musical Symbols as Myanmar and
#                      Kannada as Khmer (mym2, khmr)
set -eu
out=$1
here=$(dirname "$0")
dejavu=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
serif=/usr/share/fonts/truetype/freefont/FreeSerif.ttf
sans=/usr/share/fonts/truetype/freefont/FreeSans.ttf
mkdir -p "$out"
python3 "$here/make_syllables.py" "$out/Syllables.ttf"
if [ -f "$dejavu" ]; then
    python3 "$here/make_math.py" "$dejavu" "$out/MathTest.ttf"
    python3 "$here/make_variable.py" "$dejavu" "$out/VarTest.ttf" 600
fi
old="dev2=dev1 bng2=bng1 gjr2=gjr1 gur2=gur1 knd2=knd1 mlm2=mlm1 ory2=ory1 tml2=tml1"
use="dev2=dev3 bng2=bng3 gjr2=gjr3 gur2=gur3 knd2=knd3 mlm2=mlm3 ory2=ory3 tml2=tml3"
if [ -f "$serif" ]; then
    # shellcheck disable=SC2086
    python3 "$here/retag.py" "$serif" "$out/FreeSerifOld.ttf" $old
    # shellcheck disable=SC2086
    python3 "$here/retag.py" "$serif" "$out/FreeSerifUse.ttf" $use
    python3 "$here/retag.py" "$serif" "$out/FreeSerifMyKh.ttf" musc=mym2 knd2=khmr
fi
if [ -f "$sans" ]; then
    # shellcheck disable=SC2086
    python3 "$here/retag.py" "$sans" "$out/FreeSansOld.ttf" $old
    # shellcheck disable=SC2086
    python3 "$here/retag.py" "$sans" "$out/FreeSansUse.ttf" $use
fi
