#!/usr/bin/env python3
"""Checks std.truetype's rasterizer against FreeType and a ground truth.

Builds tools/truetype-dump, renders glyphs of several fonts and sizes, and
requires for every glyph:

  - the same bitmap size and placement as FreeType (unhinted), and
  - coverage close to a 16x16-supersampled rendering of the exact outline
    (non-zero winding, fontTools): mean error at most 2.5 levels, and no
    worse than FreeType's own mean error.

Needs freetype-py and fontTools (pip install freetype-py fonttools); skips
otherwise. Usage: python3 tools/check_truetype.py [compiler]
"""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CASES = [
    ("tests/support/fonts/DejaVuSans-subset.ttf", 24, "AVgé&8QOaS@Ä€"),
    ("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 13, "gé&8QOaSß€"),
    ("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 40, "a%Wé@"),
]


def main():
    try:
        import freetype
        from fontTools.pens.basePen import BasePen
        from fontTools.ttLib import TTFont
    except ImportError:
        print("skip check_truetype.py (needs freetype-py and fontTools)")
        return 0
    compiler = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "mlx-out/bin/compiler/mlx4")

    class Flat(BasePen):
        def __init__(self, glyphs):
            super().__init__(glyphs)
            self.edges = []

        def _moveTo(self, point):
            self.current = self.start = point

        def _lineTo(self, point):
            self.edges.append((self.current, point))
            self.current = point

        def _qCurveToOne(self, control, point):
            a = self.current
            for step in range(1, 65):
                t = step / 64
                u = 1 - t
                q = (u * u * a[0] + 2 * u * t * control[0] + t * t * point[0],
                     u * u * a[1] + 2 * u * t * control[1] + t * t * point[1])
                self.edges.append((self.current, q))
                self.current = q

        def _closePath(self):
            if self.current != self.start:
                self.edges.append((self.current, self.start))
            self.current = self.start

    def truth(font, glyph_name, scale, left, top, width, height):
        pen = Flat(font.getGlyphSet())
        font.getGlyphSet()[glyph_name].draw(pen)
        edges = [((a[0] * scale, -a[1] * scale), (b[0] * scale, -b[1] * scale)) for a, b in pen.edges]
        samples = 16
        result = {}
        for py in range(top, top + height):
            for sy in range(samples):
                y = py + (sy + 0.5) / samples
                crossings = []
                for (x0, y0), (x1, y1) in edges:
                    if y0 <= y < y1 or y1 <= y < y0:
                        crossings.append((x0 + (y - y0) * (x1 - x0) / (y1 - y0), 1 if y1 > y0 else -1))
                for px in range(left, left + width):
                    inside = 0
                    for sx in range(samples):
                        x = px + (sx + 0.5) / samples
                        if sum(d for cx, d in crossings if cx < x) != 0:
                            inside += 1
                    result[(px, py)] = result.get((px, py), 0) + inside
        return {key: value * 255 / (samples * samples) for key, value in result.items()}

    failures = 0
    with tempfile.TemporaryDirectory() as work:
        dump = os.path.join(work, "truetype-dump")
        subprocess.run([compiler, "--quiet", "tools/truetype-dump/main.mlx", "-o", dump], cwd=ROOT, check=True)
        for path, em, text in CASES:
            full = os.path.join(ROOT, path)
            if not os.path.exists(full):
                print(f"skip {path} (not installed)")
                continue
            output = subprocess.run([dump, full, str(em)] + [str(ord(c)) for c in text],
                                    capture_output=True, text=True, cwd=ROOT).stdout.split("\n")
            font = TTFont(full)
            cmap = font.getBestCmap()
            scale = em / font["head"].unitsPerEm
            face = freetype.Face(full)
            face.set_pixel_sizes(0, em)
            line = 1
            ours_error = freetype_error = 0.0
            count = 0
            worst = 0.0
            for character in text:
                width, height, left, top = map(int, output[line].split()[6:10])
                line += 1
                ours = {}
                for row in range(height):
                    for column, value in enumerate(map(int, output[line].split())):
                        ours[(left + column, top + row)] = value
                    line += 1
                face.load_char(character, freetype.FT_LOAD_RENDER | freetype.FT_LOAD_NO_HINTING)
                glyph = face.glyph
                bitmap = glyph.bitmap
                if (width - 2, height, left, top) != (bitmap.width, bitmap.rows, glyph.bitmap_left, -glyph.bitmap_top):
                    print(f"FAIL {path} {em}px {character!r}: bitmap {width - 2}x{height}@({left},{top}), "
                          f"FreeType {bitmap.width}x{bitmap.rows}@({glyph.bitmap_left},{-glyph.bitmap_top})")
                    failures += 1
                    continue
                reference = {(glyph.bitmap_left + x, -glyph.bitmap_top + y): bitmap.buffer[y * bitmap.pitch + x]
                             for y in range(bitmap.rows) for x in range(bitmap.width)}
                for key, value in truth(font, cmap[ord(character)], scale, left, top, width, height).items():
                    if value == 0 and ours.get(key, 0) == 0 and reference.get(key, 0) == 0:
                        continue
                    error = abs(ours.get(key, 0) - value)
                    ours_error += error
                    freetype_error += abs(reference.get(key, 0) - value)
                    worst = max(worst, error)
                    count += 1
            ours_mean = ours_error / max(1, count)
            freetype_mean = freetype_error / max(1, count)
            verdict = "ok  " if ours_mean <= 2.5 and ours_mean <= freetype_mean else "FAIL"
            if verdict == "FAIL":
                failures += 1
            print(f"{verdict} {os.path.basename(path)} {em}px {text}: mean error {ours_mean:.2f} "
                  f"(FreeType {freetype_mean:.2f}), worst {worst:.0f}, placement identical to FreeType")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
