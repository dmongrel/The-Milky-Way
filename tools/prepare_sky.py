"""Builds assets/sky/*.jpg from the NASA/ESA originals in the shared references directory.

    python tools/prepare_sky.py "<references dir>"

Andromeda: the PHAST mosaic is a strip with jagged black tile edges. This crops it around the
nucleus and fades the light to black well inside those edges and the crop, so the billboard in
the sky shows only the galaxy, with no outline. The deep field is copied as it is.
"""

import os
import shutil
import sys

import numpy as np
from PIL import Image, ImageFilter

Image.MAX_IMAGE_PIXELS = None

refs = sys.argv[1]
here = os.path.dirname(os.path.abspath(__file__))
out_dir = os.path.join(here, "..", "assets", "sky")
os.makedirs(out_dir, exist_ok=True)

# ---- Andromeda -----------------------------------------------------------------------------
src = Image.open(os.path.join(refs, "imagery", "stsci_2025-005a_6000.jpg")).convert("RGB")
w, h = src.size

# The nucleus: the brightest point of a heavily blurred copy.
small = src.resize((w // 8, h // 8), Image.BILINEAR).convert("L").filter(ImageFilter.GaussianBlur(12))
lum = np.asarray(small, dtype=np.float32)
ny, nx = np.unravel_index(np.argmax(lum), lum.shape)
cx, cy = nx * 8 + 4, ny * 8 + 4
print(f"andromeda nucleus at ({cx}, {cy}) of {w}x{h}")

# Where the mosaic has data: anything not the pure black outside it. Shrink that region and blur
# it, so the light fades out before any tile edge.
covered = Image.fromarray((np.asarray(src.convert("L")) > 6).astype(np.uint8) * 255)
covered = covered.resize((w // 4, h // 4), Image.BILINEAR)
covered = covered.point(lambda v: 255 if v > 128 else 0)
covered = covered.filter(ImageFilter.MinFilter(61))  # about 240 px of the original
covered = covered.filter(ImageFilter.GaussianBlur(45))
mask = np.asarray(covered.resize((w, h), Image.BILINEAR), dtype=np.float32) / 255.0
mask = mask * mask  # a slower start to the fade

# Crop symmetric about the nucleus horizontally, so the billboard's centre is the galaxy's.
half = min(cx, w - cx)
box = (cx - half, 0, cx + half, h)
pixels = np.asarray(src, dtype=np.float32)[:, box[0]:box[2]]
mask = mask[:, box[0]:box[2]]

# An elliptical fade about the nucleus as well, so the crop's sides never show either.
yy, xx = np.mgrid[0:h, 0:2 * half].astype(np.float32)
d = np.sqrt(((xx - half) / (half * 0.98)) ** 2 + ((yy - cy) / (h * 0.62)) ** 2)
ellipse = np.clip((1.0 - d) / 0.35, 0.0, 1.0)
ellipse = ellipse * ellipse * (3 - 2 * ellipse)

out = pixels * (mask * ellipse)[..., None]
img = Image.fromarray(np.clip(out, 0, 255).astype(np.uint8))
img = img.resize((2048, round(2048 * h / (2 * half))), Image.LANCZOS)
img.save(os.path.join(out_dir, "andromeda.jpg"), quality=90, optimize=True)
print("andromeda.jpg", img.size, f"nucleus row {cy / h:.3f} of the height")

# ---- Deep field ----------------------------------------------------------------------------
shutil.copyfile(os.path.join(refs, "imagery", "stsci_2012-37a_1600.jpg"), os.path.join(out_dir, "deep-field.jpg"))
print("deep-field.jpg copied")
