#!/usr/bin/env python3
"""Convert sim frames (480x320 RGB565 little-endian .raw) to .png. Needs Pillow."""
import glob, os, sys
from PIL import Image

W, H = 480, 320
out = sys.argv[1] if len(sys.argv) > 1 else "sim_out"
for f in sorted(glob.glob(os.path.join(out, "*.raw"))):
    data = open(f, "rb").read()
    img = Image.new("RGB", (W, H))
    px = img.load()
    for i in range(W * H):
        v = data[2 * i] | (data[2 * i + 1] << 8)
        px[i % W, i // W] = ((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
    img.save(f[:-4] + ".png")
    os.remove(f)
    print("wrote", f[:-4] + ".png")
