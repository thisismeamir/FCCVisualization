#!/usr/bin/env python3
"""Generates PNG reader fixtures with an independent encoder (Pillow/zlib):
dynamic Huffman blocks and adaptive row filters. Committed; rerun only when
the fixtures must change. The pixel formula is repeated in test_raster.cc.
"""
from pathlib import Path
from PIL import Image

HERE = Path(__file__).resolve().parent
W, H = 37, 23
img = Image.new("RGBA", (W, H))
img.putdata([((x * 7) % 256, (y * 11) % 256, (x * y) % 256, 255 - (x + y) % 128)
             for y in range(H) for x in range(W)])
img.save(HERE / "pillow_rgba.png", optimize=True)
