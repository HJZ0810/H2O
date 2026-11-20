"""Compare firmware-rasterized samples with an independent FreeType rendering."""

import argparse
import hashlib
import io
import math
from pathlib import Path
from zipfile import ZipFile

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

from generate_han_atlas import FONT_SHA256

CODEPOINTS = (0x867E, 0x9F98, 0x20BB7, 0x30EDE, 0x323B0, 0x33479)


def compare(archive_path, samples_path):
    archive_bytes = archive_path.read_bytes()
    if hashlib.sha256(archive_bytes).hexdigest() != FONT_SHA256:
        raise ValueError("Use the original Jigmo 2025-09-12 archive")
    bitmaps = samples_path.read_bytes()
    if len(bitmaps) != len(CODEPOINTS) * 72:
        raise ValueError("Sample file does not contain six 12x12 gray bitmaps")
    fonts = []
    with ZipFile(io.BytesIO(archive_bytes)) as archive:
        for name in ("Jigmo.ttf", "Jigmo2.ttf", "Jigmo3.ttf"):
            member = next(path for path in archive.namelist() if Path(path).name == name)
            raw = archive.read(member)
            with TTFont(io.BytesIO(raw)) as font:
                cmap = font.getBestCmap()
            face = ImageFont.truetype(io.BytesIO(raw), 48, layout_engine=ImageFont.Layout.BASIC)
            fonts.append((cmap, face))

    for index, codepoint in enumerate(CODEPOINTS):
        encoded = bitmaps[index * 72:(index + 1) * 72]
        actual = [pixel / 15 for byte in encoded for pixel in (byte >> 4, byte & 15)]
        face = next(font for cmap, font in fonts if codepoint in cmap)
        canvas = Image.new("L", (48, 48))
        ImageDraw.Draw(canvas).text((0, 42), chr(codepoint), font=face, anchor="ls", fill=255)
        reference = [(pixel / 255) ** 0.5 for pixel in
                     canvas.resize((12, 12), Image.Resampling.BOX).tobytes()]
        numerator = sum(a * b for a, b in zip(actual, reference))
        denominator = math.sqrt(sum(a * a for a in actual) * sum(b * b for b in reference))
        similarity = numerator / denominator if denominator else 0
        print(f"U+{codepoint:04X} FreeType pixel cosine {similarity:.3f}")
        if similarity < 0.95:
            raise ValueError(f"Unexpected orientation or shape for U+{codepoint:04X}")
    print("Six independent FreeType comparisons passed.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("jigmo_archive", type=Path)
    parser.add_argument("sample_bitmaps", type=Path)
    args = parser.parse_args()
    compare(args.jigmo_archive, args.sample_bitmaps)


if __name__ == "__main__":
    main()
