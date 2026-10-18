#!/usr/bin/env python3
"""Checks std.truetype's glyphs against the system's FreeType.

Builds tools/truetype-dump (std.truetype's Font: glyph indices, advances
and rasterized glyphs) and a C program printing the same of the system's
FreeType 2 (FT_Get_Char_Index, the advance in font units, the glyph
rendered unhinted at the same size), runs both on glyphs of several
fonts (TrueType and CFF outlines, simple and compound glyphs) and sizes,
and requires every line to be the same: the glyphs, their advances, the
bitmaps' sizes and placement and every pixel. std.truetype renders with
FreeType's anti-aliasing rasterizer, so its glyphs are FreeType's.

Needs gcc and FreeType's development files (pkg-config freetype2); skips
otherwise. Usage: python3 tools/check_truetype.py [compiler]
"""
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CASES = [
    ("tests/support/fonts/DejaVuSans-subset.ttf", 24, "AVgé&8QOaS@Ä€"),
    ("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 13, "gé&8QOaSß€"),
    ("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 40, "a%Wé@"),
    ("/usr/share/fonts/truetype/liberation/LiberationSerif-Italic.ttf", 31, "Qfgjé&"),
    ("/usr/share/fonts/opentype/inter/Inter-Regular.otf", 17, "AVgé&8QOaS@€"),
]

REFERENCE = r"""
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    FT_Library library;
    FT_Face face;
    if (argc < 3 || FT_Init_FreeType(&library) || FT_New_Face(library, argv[1], 0, &face)) return 1;
    FT_Set_Pixel_Sizes(face, 0, (FT_UInt)atoi(argv[2]));
    for (int i = 3; i < argc; i++) {
        unsigned long code = strtoul(argv[i], NULL, 10);
        FT_UInt glyph = FT_Get_Char_Index(face, code);
        FT_Load_Glyph(face, glyph, FT_LOAD_NO_SCALE);
        long advance = face->glyph->metrics.horiAdvance;
        int width = 0, rows = 0, left = 0, top = 0;
        FT_Bitmap *b = NULL;
        if (!FT_Load_Glyph(face, glyph, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_RENDER)) {
            b = &face->glyph->bitmap;
            if (b->width > 0 && b->rows > 0) {
                width = (int)b->width;
                rows = (int)b->rows;
                left = face->glyph->bitmap_left;
                top = -face->glyph->bitmap_top;
            }
        }
        printf("glyph %lu %u advance %ld bitmap %d %d %d %d\n", code, glyph, advance, width, rows, left, top);
        for (int y = 0; y < rows; y++)
            for (int x = 0; x < width; x++)
                printf("%d%c", b->buffer[y * b->pitch + x], x + 1 == width ? '\n' : ' ');
    }
    return 0;
}
"""


def main():
    compiler = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "mlx-out/bin/compiler/mlx4")
    if shutil.which("gcc") is None or shutil.which("pkg-config") is None:
        print("skip check_truetype.py (needs gcc and pkg-config)")
        return 0
    flags = subprocess.run(["pkg-config", "--cflags", "--libs", "freetype2"], capture_output=True, text=True)
    if flags.returncode != 0:
        print("skip check_truetype.py (needs FreeType's development files)")
        return 0
    failures = 0
    with tempfile.TemporaryDirectory() as work:
        dump = os.path.join(work, "truetype-dump")
        subprocess.run([compiler, "--quiet", "tools/truetype-dump/main.mlx", "-o", dump], cwd=ROOT, check=True)
        source = os.path.join(work, "reference.c")
        with open(source, "w") as out:
            out.write(REFERENCE)
        reference = os.path.join(work, "reference")
        subprocess.run(["gcc", "-O1", source, "-o", reference] + flags.stdout.split(), check=True)
        for path, em, text in CASES:
            full = os.path.join(ROOT, path)
            if not os.path.exists(full):
                print(f"skip {path} (not installed)")
                continue
            codes = [str(ord(c)) for c in text]
            ours = subprocess.run([dump, full, str(em)] + codes, capture_output=True, text=True, cwd=ROOT).stdout.split("\n")
            theirs = subprocess.run([reference, full, str(em)] + codes, capture_output=True, text=True).stdout.split("\n")
            # (truetype-dump's first line tells the font's tables.)
            if ours[1:] != theirs:
                failures += 1
                for number, (a, b) in enumerate(zip(ours[1:], theirs)):
                    if a != b:
                        print(f"FAIL {os.path.basename(path)} {em}px, line {number + 1}: std.truetype {a!r}, FreeType {b!r}")
                        break
                else:
                    print(f"FAIL {os.path.basename(path)} {em}px: {len(ours) - 1} lines, FreeType {len(theirs)}")
                continue
            glyphs = sum(1 for line in theirs if line.startswith("glyph "))
            print(f"ok   {os.path.basename(path)} {em}px {text}: {glyphs} glyphs as FreeType renders them")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
