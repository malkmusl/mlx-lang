# Makes the small fonts of mlx-freetype's tests (tests/fonts) out of
# fonts a Debian or Ubuntu system has (fonts-liberation, fonts-inter,
# fonts-dejavu-core), with fontTools (pip install fonttools); it is not
# part of the build, and the fonts it made are kept here:
#
#   var-tt.ttf    a TrueType variable font: fvar (two axes, five named
#                 instances, one with a PostScript name), avar, gvar (with
#                 intermediate regions and untouched points), HVAR, MVAR
#   var-cff2.otf  a CFF2 variable font (blended charstrings, HVAR)
#   colr.ttf      COLR version 0 layers, CPAL version 1 (two palettes with
#                 types and names, named entries)
#   colr1.ttf     COLR version 1: layers, solids, the three gradients,
#                 transforms of every kind, a composite, COLR glyphs, clip
#                 boxes
#   sbix.ttf      sbix strikes of PNGs of every color type and depth, all
#                 row filters, and a `dupe` glyph
#
#   python3 make_fonts.py OUTPUT-DIRECTORY
import sys, os, tempfile
from fontTools.ttLib import TTFont, newTable
from fontTools import subset
from fontTools.designspaceLib import DesignSpaceDocument, AxisDescriptor, SourceDescriptor, InstanceDescriptor
from fontTools import varLib
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.t2CharStringPen import T2CharStringPen

out = sys.argv[1]
work = tempfile.mkdtemp()
os.makedirs(out, exist_ok=True)

TEXT = "AOBagnex.,!H"

# The fonts the test fonts are made of, and their licenses (the SIL Open
# Font License's reserved names are why the test fonts are renamed).
LICENSES = {
    "Liberation Sans": "SIL Open Font License 1.1, Copyright 2012 Red Hat, Inc.",
    "Inter": "SIL Open Font License 1.1, Copyright 2016 The Inter Project Authors.",
    "DejaVu Sans": "Bitstream Vera Fonts License, Copyright 2003 Bitstream, Inc.; DejaVu changes are in the public domain.",
}


def rename(font, family, source):
    # The test fonts are named as fonts of their own: the family, full,
    # PostScript and unique names, and the CFF font name.
    name = font["name"]
    for rec in name.names:
        if rec.nameID in (1, 16, 21, 25):
            rec.string = family
        elif rec.nameID == 4:
            rec.string = family + " " + (name.getDebugName(2) or "Regular")
        elif rec.nameID == 6:
            rec.string = family + "-" + (name.getDebugName(2) or "Regular").replace(" ", "")
        elif rec.nameID == 3:
            rec.string = family + ";test"
        elif rec.nameID in (5, 7, 8, 9, 10, 11, 12, 14):
            rec.string = "Made for mlx-freetype's tests from " + source + " (see make_fonts.py)."
        elif rec.nameID in (0, 13):
            rec.string = "Derived from " + source + ", under its license: " + LICENSES[source]
    for table in ("CFF ",):
        if table in font:
            cff = font[table].cff
            cff.fontNames = [family + "-Regular"]


def subset_font(path, dest, unicodes_text, extra_glyphs=()):
    options = subset.Options()
    options.layout_features = []
    options.name_IDs = ["*"]
    options.name_languages = ["*"]
    options.notdef_outline = True
    options.glyph_names = True
    options.hinting = False
    options.drop_tables += ["DSIG", "GPOS", "GSUB", "GDEF", "kern", "fpgm", "prep", "cvt ", "gasp", "hdmx", "LTSH", "VDMX", "FFTM"]
    font = TTFont(path)
    s = subset.Subsetter(options)
    s.populate(unicodes=[ord(c) for c in unicodes_text] + [0xC1], glyphs=list(extra_glyphs))
    s.subset(font)
    font.save(dest)
    return dest


def transform_tt(src, dest, matrix, advance_scale, metrics):
    font = TTFont(src)
    glyf = font["glyf"]
    hmtx = font["hmtx"]
    for name in font.getGlyphOrder():
        g = glyf[name]
        if g.isComposite():
            for c in g.components:
                c.x = int(round(c.x * matrix[0]))
            continue
        if g.numberOfContours == 0:
            continue
        coords = g.coordinates
        for i in range(len(coords)):
            x, y = coords[i]
            coords[i] = (int(round(x * matrix[0] + y * matrix[1])), int(round(x * matrix[2] + y * matrix[3])))
        g.recalcBounds(glyf)
    for name in font.getGlyphOrder():
        adv, lsb = hmtx[name]
        g = glyf[name]
        hmtx[name] = (int(round(adv * advance_scale)), getattr(g, "xMin", 0) if g.numberOfContours != 0 else 0)
    os2 = font["OS/2"]
    for k, v in metrics.items():
        setattr(os2, k, v)
    font.save(dest)


def build_tt_var():
    base = subset_font("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", os.path.join(work, "tt-base.ttf"), TEXT)
    light = os.path.join(work, "tt-light.ttf")
    bold = os.path.join(work, "tt-bold.ttf")
    semi = os.path.join(work, "tt-semi.ttf")
    narrow = os.path.join(work, "tt-narrow.ttf")
    transform_tt(base, light, (0.95, 0, 0, 1.0), 0.95, {"sxHeight": 1000, "sCapHeight": 1400})
    transform_tt(base, bold, (1.15, 0.05, 0, 1.02), 1.15, {"sxHeight": 1100, "sCapHeight": 1450, "yStrikeoutPosition": 600})
    transform_tt(base, semi, (1.05, 0.02, 0.01, 1.0), 1.07, {"sxHeight": 1070})
    transform_tt(base, narrow, (0.8, 0, 0, 1.0), 0.8, {"sxHeight": 1062})
    doc = DesignSpaceDocument()
    a = AxisDescriptor()
    a.tag, a.name, a.minimum, a.default, a.maximum = "wght", "Weight", 100, 400, 900
    a.map = [(100, 100), (400, 400), (600, 700), (900, 900)]
    doc.addAxis(a)
    a = AxisDescriptor()
    a.tag, a.name, a.minimum, a.default, a.maximum = "wdth", "Width", 75, 100, 100
    doc.addAxis(a)
    for path, loc, name in ((base, {"Weight": 400, "Width": 100}, "Regular"), (light, {"Weight": 100, "Width": 100}, "Light"), (bold, {"Weight": 900, "Width": 100}, "Bold"), (semi, {"Weight": 700, "Width": 100}, "Semi"), (narrow, {"Weight": 400, "Width": 75}, "Narrow")):
        s = SourceDescriptor()
        s.path = path
        s.location = loc
        s.styleName = name
        s.familyName = "MlxVarTest"
        doc.addSource(s)
    for loc, name in (({"Weight": 100, "Width": 100}, "Thin"), ({"Weight": 400, "Width": 100}, "Regular"), ({"Weight": 650, "Width": 100}, "SemiBold"), ({"Weight": 900, "Width": 100}, "Black"), ({"Weight": 900, "Width": 75}, "Black Condensed")):
        i = InstanceDescriptor()
        i.location = loc
        i.styleName = name
        i.familyName = "MlxVarTest"
        i.postScriptFontName = None if name != "Black Condensed" else "MlxVarTest-BlackCond"
        doc.addInstance(i)
    vf, _, _ = varLib.build(doc)
    rename(vf, "MlxVarTest", "Liberation Sans")
    vf.save(os.path.join(out, "var-tt.ttf"))


def redraw_cff(src, dest, matrix, advance_scale):
    font = TTFont(src)
    cff = font["CFF "].cff
    top = cff.topDictIndex[0]
    cs = top.CharStrings
    gs = font.getGlyphSet()
    hmtx = font["hmtx"]
    for name in font.getGlyphOrder():
        adv, lsb = hmtx[name]
        nadv = int(round(adv * advance_scale))
        pen = T2CharStringPen(nadv, gs)
        gs[name].draw(TransformPen(pen, (matrix[0], matrix[2], matrix[1], matrix[3], 0, 0)))
        c = pen.getCharString(private=cs[name].private, globalSubrs=cs[name].globalSubrs)
        cs[name] = c
        hmtx[name] = (nadv, lsb)
    font.save(dest)


def build_cff2_var():
    base = subset_font("/usr/share/fonts/opentype/inter/Inter-Regular.otf", os.path.join(work, "cff-base0.otf"), TEXT)
    b = os.path.join(work, "cff-base.otf")
    bold = os.path.join(work, "cff-bold.otf")
    redraw_cff(base, b, (1, 0, 0, 1), 1)
    redraw_cff(base, bold, (1.2, 0.08, 0, 1.03), 1.2)
    doc = DesignSpaceDocument()
    a = AxisDescriptor()
    a.tag, a.name, a.minimum, a.default, a.maximum = "wght", "Weight", 400, 400, 900
    doc.addAxis(a)
    for path, loc, name in ((b, {"Weight": 400}, "Regular"), (bold, {"Weight": 900}, "Bold")):
        s = SourceDescriptor()
        s.path = path
        s.location = loc
        s.styleName = name
        s.familyName = "MlxCff2Test"
        doc.addSource(s)
    for loc, name in (({"Weight": 400}, "Regular"), ({"Weight": 700}, "Bold"), ({"Weight": 900}, "Black")):
        i = InstanceDescriptor()
        i.location = loc
        i.styleName = name
        i.familyName = "MlxCff2Test"
        doc.addInstance(i)
    vf, _, _ = varLib.build(doc)
    rename(vf, "MlxCff2Test", "Inter")
    vf.save(os.path.join(out, "var-cff2.otf"))


def build_colr():
    base = subset_font("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", os.path.join(work, "colr-base.ttf"), TEXT)
    font = TTFont(base)
    order = font.getGlyphOrder()
    cmap = font.getBestCmap()
    A, O, B, g, H = cmap[ord("A")], cmap[ord("O")], cmap[ord("B")], cmap[ord("g")], cmap[ord("H")]
    from fontTools.colorLib.builder import buildCOLR, buildCPAL
    font["COLR"] = buildCOLR({A: [(O, 0), (B, 1)], g: [(g, 2), (O, 0xFFFF)], H: [(A, 1), (H, 0)]}, version=0)
    font["CPAL"] = buildCPAL([[(1, 0, 0, 1), (0, 0.5, 0, 0.75), (0, 0, 1, 1)], [(0.2, 0.2, 0.2, 1), (1, 1, 0, 0.5), (0, 1, 1, 1)]], paletteTypes=[1, 2], paletteLabels=["Light", "Dark"], paletteEntryLabels=["Ink", "Leaf", "Sky"], nameTable=font["name"])
    rename(font, "MlxColrTest", "DejaVu Sans")
    font.save(os.path.join(out, "colr.ttf"))


def build_colr1():
    from fontTools.colorLib.builder import buildCOLR, buildCPAL
    from fontTools.ttLib.tables.otTables import PaintFormat as PF
    base = subset_font("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", os.path.join(work, "colr1-base.ttf"), TEXT)
    font = TTFont(base)
    cmap = font.getBestCmap()
    A, O, B, g, H, a, n, e, x = (cmap[ord(c)] for c in "AOBgHanex")
    solid = lambda i, al=1.0: {"Format": PF.PaintSolid, "PaletteIndex": i, "Alpha": al}
    line = lambda ext=0: {"Extend": ext, "ColorStop": [{"StopOffset": 0.0, "PaletteIndex": 0, "Alpha": 1.0}, {"StopOffset": 0.5, "PaletteIndex": 1, "Alpha": 0.5}, {"StopOffset": 1.0, "PaletteIndex": 2, "Alpha": 1.0}]}
    glyph = lambda name, paint: {"Format": PF.PaintGlyph, "Paint": paint, "Glyph": name}
    paints = {
        A: {"Format": PF.PaintColrLayers, "Layers": [glyph(O, solid(0)), glyph(B, solid(1, 0.5)), glyph(A, solid(0xFFFF))]},
        B: glyph(B, {"Format": PF.PaintLinearGradient, "ColorLine": line(1), "x0": 0, "y0": 0, "x1": 500, "y1": 700, "x2": -300, "y2": 200}),
        O: glyph(O, {"Format": PF.PaintRadialGradient, "ColorLine": line(2), "x0": 600, "y0": 600, "r0": 50, "x1": 700, "y1": 500, "r1": 600}),
        g: glyph(g, {"Format": PF.PaintSweepGradient, "ColorLine": line(0), "centerX": 500, "centerY": 400, "startAngle": 0.25, "endAngle": 1.5}),
        H: {"Format": PF.PaintTransform, "Paint": glyph(H, solid(2)), "Transform": {"xx": 0.9, "yx": 0.1, "xy": -0.2, "yy": 1.1, "dx": 30, "dy": -40}},
        a: {"Format": PF.PaintComposite, "SourcePaint": {"Format": PF.PaintTranslate, "Paint": glyph(a, solid(0)), "dx": 100, "dy": -50},
            "CompositeMode": 3, "BackdropPaint": {"Format": PF.PaintRotateAroundCenter, "Paint": glyph(a, solid(1)), "angle": 0.125, "centerX": 300, "centerY": 300}},
        n: {"Format": PF.PaintColrLayers, "Layers": [
            {"Format": PF.PaintScale, "Paint": glyph(n, solid(0)), "scaleX": 1.25, "scaleY": 0.75},
            {"Format": PF.PaintScaleAroundCenter, "Paint": glyph(n, solid(1)), "scaleX": 0.5, "scaleY": 1.5, "centerX": 200, "centerY": 100},
            {"Format": PF.PaintScaleUniform, "Paint": glyph(n, solid(2)), "scale": 1.1},
            {"Format": PF.PaintScaleUniformAroundCenter, "Paint": glyph(n, solid(0)), "scale": 0.9, "centerX": -10, "centerY": 20},
            {"Format": PF.PaintRotate, "Paint": glyph(n, solid(1)), "angle": -0.5},
            {"Format": PF.PaintSkew, "Paint": glyph(n, solid(2)), "xSkewAngle": 0.1, "ySkewAngle": -0.2},
            {"Format": PF.PaintSkewAroundCenter, "Paint": glyph(n, solid(0)), "xSkewAngle": -0.1, "ySkewAngle": 0.2, "centerX": 50, "centerY": 60}]},
        e: {"Format": PF.PaintColrGlyph, "Glyph": A},
        x: {"Format": PF.PaintColrLayers, "Layers": [{"Format": PF.PaintColrGlyph, "Glyph": B}, glyph(x, solid(2, 0.25))]},
    }
    clips = {A: (0, -200, 1500, 1600), O: (10, 20, 1300, 1500), g: (5, -400, 1200, 1100)}
    font["COLR"] = buildCOLR(paints, version=1, clipBoxes=clips)
    font["CPAL"] = buildCPAL([[(1, 0, 0, 1), (0, 0.5, 0, 0.75), (0, 0, 1, 1)], [(0.2, 0.2, 0.2, 1), (1, 1, 0, 0.5), (0, 1, 1, 1)]])
    rename(font, "MlxColrOneTest", "DejaVu Sans")
    font.save(os.path.join(out, "colr1.ttf"))


def png_bytes(width, height, color_type, depth, pixel, palette=None, trns=None):
    # A PNG of the given color type and depth; pixel(x, y) gives the
    # samples of a pixel (a tuple), or the palette index.
    import zlib, struct
    def chunk(kind, data):
        c = struct.pack(">I", len(data)) + kind + data
        return c + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    rows = b""
    for y in range(height):
        bits = []
        for x in range(width):
            v = pixel(x, y)
            if not isinstance(v, tuple): v = (v,)
            bits.extend(v)
        if depth >= 8:
            per = depth // 8
            row = b"".join(s.to_bytes(per, "big") for s in bits)
        else:
            acc, n, out = 0, 0, bytearray()
            for s in bits:
                acc = (acc << depth) | s
                n += depth
                if n == 8:
                    out.append(acc); acc = 0; n = 0
            if n: out.append(acc << (8 - n))
            row = bytes(out)
        rows += bytes([y % 5]) + filter_row(row, rows, y, max(1, (len(bits) // width) * depth // 8))
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, depth, color_type, 0, 0, 0))
    if palette: data += chunk(b"PLTE", bytes(c for rgb in palette for c in rgb))
    if trns is not None: data += chunk(b"tRNS", trns)
    return data + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")


_prev_rows = {}
def filter_row(row, rows, y, bpp):
    # Applies filter type y % 5 to the row (the previous unfiltered row kept).
    prev = _prev_rows.get(y - 1, bytes(len(row))) if y else bytes(len(row))
    _prev_rows[y] = row
    kind = y % 5
    out = bytearray()
    for i, b in enumerate(row):
        a = row[i - bpp] if i >= bpp else 0
        up = prev[i] if i < len(prev) else 0
        c = prev[i - bpp] if i >= bpp and i - bpp < len(prev) else 0
        if kind == 0: f = b
        elif kind == 1: f = b - a
        elif kind == 2: f = b - up
        elif kind == 3: f = b - ((a + up) >> 1)
        else:
            p = a + up - c
            pa, pb, pc = abs(p - a), abs(p - up), abs(p - c)
            pr = a if pa <= pb and pa <= pc else (up if pb <= pc else c)
            f = b - pr
        out.append(f & 0xFF)
    return bytes(out)


def build_sbix():
    from fontTools.ttLib.tables.sbixStrike import Strike
    from fontTools.ttLib.tables.sbixGlyph import Glyph
    base = subset_font("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", os.path.join(work, "sbix-base.ttf"), TEXT)
    font = TTFont(base)
    order = font.getGlyphOrder()
    images = [
        lambda: png_bytes(12, 10, 6, 8, lambda x, y: (x * 20, y * 25, (x * y) % 256, (x * 30 + y * 10) % 256)),
        lambda: png_bytes(9, 11, 2, 8, lambda x, y: (x * 25, 200 - y * 15, 77)),
        lambda: png_bytes(7, 8, 0, 8, lambda x, y: (x * 31 + y * 7) % 256),
        lambda: png_bytes(10, 6, 4, 8, lambda x, y: ((x * 21) % 256, (y * 50) % 256)),
        lambda: png_bytes(8, 8, 3, 8, lambda x, y: (x + y) % 4, palette=[(255, 0, 0), (0, 255, 0), (0, 0, 255), (10, 20, 30)], trns=bytes([255, 128, 0])),
        lambda: png_bytes(6, 9, 6, 16, lambda x, y: (x * 9000, y * 7000, 30000, (x * 11000 + y * 3000) % 65536)),
        lambda: png_bytes(13, 5, 3, 2, lambda x, y: (x + 2 * y) % 4, palette=[(1, 2, 3), (200, 100, 50), (0, 0, 0), (255, 255, 255)]),
        lambda: png_bytes(11, 7, 0, 4, lambda x, y: (x + y) % 16),
        lambda: png_bytes(16, 3, 0, 1, lambda x, y: (x + y) % 2),
        lambda: png_bytes(5, 5, 2, 16, lambda x, y: (x * 13000, y * 13000, 65535 - x * 5000)),
    ]
    sbix = newTable("sbix")
    sbix.version = 1
    sbix.flags = 1
    for ppem in (20, 40):
        strike = Strike(ppem=ppem, resolution=72)
        for gid, name in enumerate(order):
            if gid == 0:
                continue
            if gid == 3:
                strike.glyphs[name] = Glyph(glyphName=name, graphicType="dupe", referenceGlyphName=order[1], gid=gid)
                continue
            if gid == 4:
                continue
            img = images[gid % len(images)]()
            strike.glyphs[name] = Glyph(glyphName=name, graphicType="png ", imageData=img, originOffsetX=gid - 5, originOffsetY=-(gid % 3), gid=gid)
        sbix.strikes[ppem] = strike
    font["sbix"] = sbix
    rename(font, "MlxSbixTest", "DejaVu Sans")
    font.save(os.path.join(out, "sbix.ttf"))


def struct_pack_gid(gid):
    import struct
    return struct.pack(">H", gid)


build_tt_var()
build_cff2_var()
build_colr()
build_colr1()
build_sbix()
for name in sorted(os.listdir(out)):
    print(name, os.path.getsize(os.path.join(out, name)))
