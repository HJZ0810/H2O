"""Rasterize Unicode 17 Han characters into the R9 fixed-record bitmap table."""

import argparse
import hashlib
import io
import json
from pathlib import Path
from zipfile import ZipFile

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

SIDE = 12
RECORD_BYTES = SIDE * SIDE // 2
LAST_HAN = 0x33479
FONT_RELEASE = "Jigmo 2025-09-12"
FONT_SHA256 = "5744c7386d129475d87607ca66d043c8793c65448adeaedc921b6931890e5d0b"
GRAY_LEVELS = tuple(round((value / 255) ** 0.74 * 15) for value in range(256))


def han_codepoints(unicode_data):
    points = set()
    first = None
    for line in unicode_data.splitlines():
        address, name, *_ = line.split(";")
        cp = int(address, 16)
        if name.startswith("<CJK Ideograph"):
            if name.endswith(", First>"):
                first = cp
            elif name.endswith(", Last>"):
                if first is None or cp < first:
                    raise ValueError("Unexpected UnicodeData ideograph range")
                points.update(range(first, cp + 1))
                first = None
        elif name.startswith("CJK COMPATIBILITY IDEOGRAPH-") or cp == 0x3007:
            points.add(cp)
    if first is not None or len(points) != 102999 or max(points) != LAST_HAN:
        raise ValueError("Use the Unicode 17.0.0 UnicodeData.txt file")
    return points


def rasterize(face, cp):
    # Oversample to preserve thin, asymmetric strokes at the TFT size.
    size = SIDE * 3
    text = chr(cp)
    left, top, right, bottom = face.getbbox(text, anchor="ls", stroke_width=1)
    x = max(-left, min((size - int(face.getlength(text))) // 2, size - right))
    y = max(-top, min(size - 6, size - bottom))
    tile = Image.new("L", (size, size))
    ImageDraw.Draw(tile).text((x, y), text, font=face, fill=255, anchor="ls", stroke_width=1)
    tile = tile.resize((SIDE, SIDE), Image.Resampling.LANCZOS)
    values = tile.tobytes()
    bitmap = bytes((GRAY_LEVELS[values[i]] << 4) | GRAY_LEVELS[values[i+1]]
                   for i in range(0, len(values), 2))
    if len(bitmap) != RECORD_BYTES or not any(bitmap):
        raise ValueError(f"Invalid 12x12 bitmap for U+{cp:04X}")
    return bitmap


def generate(archive_path, unicode_path):
    archive_bytes = archive_path.read_bytes()
    font_sha = hashlib.sha256(archive_bytes).hexdigest()
    if font_sha != FONT_SHA256:
        raise ValueError(f"Use the official {FONT_RELEASE} archive")
    unicode_bytes = unicode_path.read_bytes()
    points = han_codepoints(unicode_bytes.decode("utf-8"))
    remaining = set(points)
    table = bytearray((LAST_HAN + 1) * RECORD_BYTES)
    with ZipFile(io.BytesIO(archive_bytes)) as archive:
        for name in ("Jigmo.ttf", "Jigmo2.ttf", "Jigmo3.ttf"):
            member = next(p for p in archive.namelist() if Path(p).name == name)
            raw = archive.read(member)
            with TTFont(io.BytesIO(raw), lazy=True) as font:
                selected = remaining.intersection(font.getBestCmap())
            face = ImageFont.truetype(io.BytesIO(raw), SIDE * 3, layout_engine=ImageFont.Layout.BASIC)
            for cp in sorted(selected):
                offset = cp * RECORD_BYTES
                table[offset:offset + RECORD_BYTES] = rasterize(face, cp)
            remaining.difference_update(selected)
            print(f"{name}: {len(selected)} encoded Han glyphs")
    if remaining:
        raise ValueError(f"Missing {len(remaining)} assigned codepoints, starting at U+{min(remaining):04X}")
    metadata = {
        "unicode": "17.0.0",
        "han_codepoints": len(points),
        "glyph_size": [SIDE, SIDE],
        "bytes_per_codepoint": RECORD_BYTES,
        "row_format": "six bytes per row, two 4-bit levels per byte, high nibble first",
        "atlas_size_bytes": len(table),
        "sha256": hashlib.sha256(table).hexdigest(),
        "font": FONT_RELEASE,
        "license": "CC0-1.0",
        "font_archive_sha256": font_sha,
        "unicode_data_sha256": hashlib.sha256(unicode_bytes).hexdigest(),
    }
    return table, metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("jigmo_archive", type=Path)
    parser.add_argument("unicode_data", type=Path)
    parser.add_argument("output", type=Path, help="directory for han-12.bin and manifest.json")
    args = parser.parse_args()
    table, metadata = generate(args.jigmo_archive, args.unicode_data)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "han-12.bin").write_bytes(table)
    (args.output / "manifest.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    with ZipFile(args.jigmo_archive) as source:
        (args.output / "Jigmo-LICENSE.txt").write_bytes(source.read("LICENSE.txt"))
    print(f"{metadata['han_codepoints']} assigned Han codepoints, {len(table)} byte range file")


if __name__ == "__main__":
    main()
