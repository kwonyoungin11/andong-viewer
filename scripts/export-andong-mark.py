"""Crop official Andong VI logo A from the AI guideline page."""
from __future__ import annotations

import sys
from pathlib import Path

import fitz
from PIL import Image

SRC = Path(r"D:\hgis\data\andong\_src_logo_a.ai")
OUT = Path(r"D:\hgis\data\andong\andong_mark.png")


def main() -> int:
    if not SRC.exists():
        print("missing", SRC, file=sys.stderr)
        return 2
    doc = fitz.open(SRC)
    page = doc[0]
    pix = page.get_pixmap(matrix=fitz.Matrix(3, 3), alpha=False)
    img = Image.frombytes("RGB", (pix.width, pix.height), pix.samples)
    w, h = img.size
    # Guideline page: the 2x2 mark sits in the upper half, not the 10mm sample.
    region = img.crop((int(w * 0.22), int(h * 0.12), int(w * 0.78), int(h * 0.58)))
    px = region.load()
    rw, rh = region.size
    xs: list[int] = []
    ys: list[int] = []
    for y in range(rh):
        for x in range(rw):
            r, g, b = px[x, y]
            if r + g + b < 90:
                continue
            # AD Blue / AD Red on the dark board
            if (b > 80 and b > r + 20 and b > g) or (r > 120 and r > g + 40 and r > b):
                xs.append(x)
                ys.append(y)
    if not xs:
        print("no logo pixels", file=sys.stderr)
        return 3
    pad = 18
    box = (
        max(0, min(xs) - pad),
        max(0, min(ys) - pad),
        min(rw, max(xs) + pad + 1),
        min(rh, max(ys) + pad + 1),
    )
    crop = region.crop(box).convert("RGBA")
    out_px = crop.load()
    cw, ch = crop.size
    for y in range(ch):
        for x in range(cw):
            r, g, b, _a = out_px[x, y]
            if r + g + b < 110:
                out_px[x, y] = (0, 0, 0, 0)
    side = max(cw, ch)
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    canvas.paste(crop, ((side - cw) // 2, (side - ch) // 2), crop)
    canvas = canvas.resize((256, 256), Image.Resampling.LANCZOS)
    canvas.save(OUT)
    print("wrote", OUT, canvas.size, "from", box)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
