#!/usr/bin/env python3
"""
Generate GB2312 HZK16 font file (94x94, 32 bytes per glyph => 282752 bytes).

Default input font:
  tools/font_gen/input_font/STKAITI.TTF

Default output file:
  tools/font_gen/output/HZK16
"""

from pathlib import Path
import argparse

from PIL import Image, ImageDraw, ImageFont


def parse_args():
    parser = argparse.ArgumentParser(description="Generate HZK16 from OTF/TTF font")
    parser.add_argument(
        "--font",
        default="tools/font_gen/input_font/STKAITI.TTF",
        help="Path to source font file",
    )
    parser.add_argument(
        "--out",
        default="tools/font_gen/output/HZK16",
        help="Path to output HZK16 file",
    )
    parser.add_argument("--size", type=int, default=16, help="Pixel size (default: 16)")
    return parser.parse_args()


def draw_glyph_16x16(ch: str, font: ImageFont.FreeTypeFont):
    img = Image.new("1", (16, 16), 0)
    draw = ImageDraw.Draw(img)

    if hasattr(draw, "textbbox"):
        bbox = draw.textbbox((0, 0), ch, font=font)
        w = bbox[2] - bbox[0]
        h = bbox[3] - bbox[1]
        x = (16 - w) // 2 - bbox[0]
        y = (16 - h) // 2 - bbox[1]
    else:
        w, h = draw.textsize(ch, font=font)
        x = (16 - w) // 2
        y = (16 - h) // 2

    draw.text((x, y), ch, fill=1, font=font)
    return img


def write_hzk16(font_path: Path, out_path: Path, size: int):
    font = ImageFont.truetype(str(font_path), size)

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("wb") as f:
        for qh in range(0xA1, 0xF8):
            for wh in range(0xA1, 0xFF):
                raw = bytes([qh, wh])
                try:
                    ch = raw.decode("gb2312")
                except UnicodeDecodeError:
                    ch = " "

                img = draw_glyph_16x16(ch, font)

                for y in range(16):
                    hi = 0
                    lo = 0
                    for x in range(8):
                        if img.getpixel((x, y)):
                            hi |= 1 << (7 - x)
                    for x in range(8, 16):
                        if img.getpixel((x, y)):
                            lo |= 1 << (15 - x)
                    f.write(bytes([hi, lo]))


def main():
    args = parse_args()
    font_path = Path(args.font)
    out_path = Path(args.out)

    if not font_path.exists():
        raise FileNotFoundError(f"Font not found: {font_path}")

    write_hzk16(font_path, out_path, args.size)
    size = out_path.stat().st_size
    print(f"Generated: {out_path}")
    print(f"Size: {size} bytes")
    print("Expected for standard HZK16: 282752 bytes")


if __name__ == "__main__":
    main()
