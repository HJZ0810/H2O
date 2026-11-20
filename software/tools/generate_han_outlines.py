"""Export Jigmo TrueType contours for on-device R9 glyph rasterization."""

import argparse
import hashlib
import io
import json
import struct
from pathlib import Path
from zipfile import ZipFile

from fontTools.ttLib import TTFont

from generate_han_atlas import FONT_RELEASE, FONT_SHA256, LAST_HAN, han_codepoints

INDEX_BYTES = (LAST_HAN + 1) * 8
MAX_POINTS = 1040


def encode_outline(font, name):
    glyf = font["glyf"]
    coordinates, ends, flags = glyf[name].getCoordinates(glyf)
    if not ends or not coordinates or len(coordinates) > MAX_POINTS:
        raise ValueError(f"Unsupported outline {name}: {len(coordinates)} points")
    if font["head"].unitsPerEm != 1024:
        raise ValueError("Expected 1024 font units per em")

    record = bytearray(b"HGO\x01")
    record.extend(struct.pack("<HH", len(ends), len(coordinates)))
    for end in ends:
        record.extend(struct.pack("<H", end))
    for (x, y), flag in zip(coordinates, flags):
        record.extend(struct.pack("<hhB", round(x), round(y), flag & 1))
    return record, len(coordinates), len(ends)


def generate(archive_path, unicode_path, output):
    archive_bytes = archive_path.read_bytes()
    if hashlib.sha256(archive_bytes).hexdigest() != FONT_SHA256:
        raise ValueError(f"Use the official {FONT_RELEASE} archive")
    unicode_bytes = unicode_path.read_bytes()
    remaining = han_codepoints(unicode_bytes.decode("utf-8"))
    index = bytearray(INDEX_BYTES)
    max_points = max_contours = max_record = count = 0
    output.mkdir(parents=True, exist_ok=True)
    asset = output / "han-outline.bin"
    with asset.open("wb+") as dst, ZipFile(io.BytesIO(archive_bytes)) as archive:
        dst.truncate(INDEX_BYTES)
        dst.seek(INDEX_BYTES)
        for member_name in ("Jigmo.ttf", "Jigmo2.ttf", "Jigmo3.ttf"):
            member = next(p for p in archive.namelist() if Path(p).name == member_name)
            with TTFont(io.BytesIO(archive.read(member))) as font:
                cmap = font.getBestCmap()
                selected = remaining.intersection(cmap)
                for cp in sorted(selected):
                    record, points, contours = encode_outline(font, cmap[cp])
                    offset = dst.tell()
                    if offset + len(record) > 0xffffffff:
                        raise ValueError("Outline table exceeds the 32-bit index")
                    struct.pack_into("<II", index, cp * 8, offset, len(record))
                    dst.write(record)
                    max_points = max(max_points, points)
                    max_contours = max(max_contours, contours)
                    max_record = max(max_record, len(record))
                remaining.difference_update(selected)
                count += len(selected)
                print(f"{member_name}: {len(selected)} outlines", flush=True)
        if remaining:
            raise ValueError(f"Missing {len(remaining)} assigned glyphs, first U+{min(remaining):04X}")
        dst.seek(0)
        dst.write(index)
    digest = hashlib.sha256()
    with asset.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    metadata = {
        "unicode": "17.0.0",
        "han_codepoints": count,
        "font": FONT_RELEASE,
        "license": "CC0-1.0",
        "font_archive_sha256": FONT_SHA256,
        "unicode_data_sha256": hashlib.sha256(unicode_bytes).hexdigest(),
        "index_bytes": INDEX_BYTES,
        "record_format": "HGO1, contours and quadratic points in 1024 units per em",
        "max_points": max_points,
        "max_contours": max_contours,
        "max_record_bytes": max_record,
        "asset_size_bytes": asset.stat().st_size,
        "sha256": digest.hexdigest(),
    }
    (output / "manifest.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    with ZipFile(archive_path) as source:
        (output / "Jigmo-LICENSE.txt").write_bytes(source.read("LICENSE.txt"))
    print(json.dumps(metadata, indent=2), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("jigmo_archive", type=Path)
    parser.add_argument("unicode_data", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    generate(args.jigmo_archive, args.unicode_data, args.output)


if __name__ == "__main__":
    main()
