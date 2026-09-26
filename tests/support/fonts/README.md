# Test fonts

`DejaVuSans-subset.ttf` is DejaVu Sans 2.37 (Debian `fonts-dejavu-core`),
subset with fontTools to printable ASCII plus `éüßÄ€` and U+10300, without
hinting or OpenType layout tables, keeping the legacy `kern` table:

```sh
pyftsubset DejaVuSans.ttf --text-file=chars.txt --legacy-kern --no-hinting \
    --layout-features='' --drop-tables+=GSUB,GPOS,GDEF,MATH,FFTM \
    --output-file=DejaVuSans-subset.ttf
```

It has short `loca` offsets, cmap formats 4 and 12, compound glyphs (`é`,
`ü`, `Ä`) and 285 kerning pairs, which `tests/262_truetype_runtime.mlx`
relies on. License: `LICENSE` (Bitstream Vera; DejaVu changes are in the
public domain).
