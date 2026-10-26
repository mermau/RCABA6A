#!/usr/bin/env python3
"""Rebuild four matching, photorealistic dial scales on the approved panel.

Run ONCE after checkout and before JUCE BinaryData generation. Always start
from the original, tracked assets/panel.jpg, never a previously processed JPG.

The source rings are photographic; this code preserves the chassis, outer
bezel shadows and panel aging. It uses one clean metal colour/lighting profile
for ALL FOUR rings, erases *all* old ink/tick regions (including the baseline
and inner ticks), and prints the calibrated ticks just once.
"""
import math
import sys
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy.ndimage import gaussian_filter

RINGS = ((276, 649, 0), (630, 648, 1), (1033, 649, 2), (1385, 650, 3))
ANGLES = {
    0: tuple(-150.0 + i * 12.5 for i in range(25)),
    1: (-136., -94., -67., -44., -22., 0., 28., 60., 93., 136.),
    2: (-92., -48., 0., 42., 79.),
    3: tuple(-139.0 + i * (278.0 / 15.0) for i in range(16)),
}
METERS = ("IN", "V1", "V2", "V3", "V4", "V5", "V6", "FIL", "CMP", "BAL")
MODES = ("B", "A", "OFF", "SINGLE", "DUAL")

def font(size):
    for path in (
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial Narrow.ttf",
        "/System/Library/Fonts/Supplemental/Helvetica.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
    ):
        if Path(path).exists():
            return ImageFont.truetype(path, size)
    raise RuntimeError("No TrueType font available")

def restore_metal(original):
    """Cover the complete obsolete-ink annulus, never a second metal disc."""
    output = original.copy()
    r0, r1, nang = 87.0, 153.0, 1440
    radii = np.linspace(r0, r1, 133, dtype=np.float32)[:, None]
    az = np.linspace(-math.pi, math.pi, nang, endpoint=False,
                     dtype=np.float32)[None, :]

    # BAL-LIMIT was the best-preserved original photographic metal. Build a
    # common clean metal profile from the *unprinted* bright pixels, then use
    # it for every bezel, including BAL-LIMIT itself.
    donor_x = (1033.0 + np.sin(az) * radii).astype(np.float32)
    donor_y = (649.0 - np.cos(az) * radii).astype(np.float32)
    samples = cv2.remap(original, donor_x, donor_y, cv2.INTER_LINEAR,
                        borderMode=cv2.BORDER_REFLECT).astype(np.float32)
    profile = np.percentile(samples, 70, axis=1)
    profile = gaussian_filter(profile, sigma=(2.0, 0.0))
    metal = np.repeat(profile[:, None, :], nang, axis=1)
    metal += np.cos(az[:, :, None] + .6) * .7
    metal += np.sin(az[:, :, None] * 2.0 - .2) * .6

    for index, (cx, cy, selector) in enumerate(RINGS):
        y0, y1 = max(0, cy - 159), min(original.shape[0], cy + 160)
        x0, x1 = max(0, cx - 159), min(original.shape[1], cx + 160)
        yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
        radius = np.hypot(xx - cx, yy - cy)
        angle = np.arctan2(xx - cx, cy - yy)
        theta_px = np.mod((angle + math.pi) / (2.0 * math.pi) * nang
                          + index * 79, nang).astype(np.float32)
        # The edge area was also printed on the original: sample the unprinted
        # middle metal near both boundaries, rather than returning old ink.
        texture_r = np.clip(radius, 100.0, 141.0)
        radial_px = ((texture_r - r0) / (r1 - r0)
                     * (metal.shape[0] - 1)).astype(np.float32)
        texture = cv2.remap(metal, theta_px, radial_px, cv2.INTER_LINEAR,
                            borderMode=cv2.BORDER_WRAP)
        rng = np.random.default_rng(120 + index)
        grain = gaussian_filter(
            rng.normal(0.0, 1.7, radius.shape).astype(np.float32), .38)
        texture = np.clip(texture + grain[:, :, None], 0, 255)

        # Crucial change: old tick marks begin near r=90, while old captions
        # leave fragments near r=149. The former 94..145 mask missed BOTH.
        alpha = (np.clip((radius - 85.0) / 3.5, 0.0, 1.0) *
                 np.clip((154.5 - radius) / 3.8, 0.0, 1.0))
        region = output[y0:y1, x0:x1]
        region[:] = np.clip(region * (1.0 - alpha[:, :, None])
                            + texture * alpha[:, :, None], 0, 255
                            ).astype(np.uint8)
    return output

def print_scales(blank):
    photo = Image.fromarray(cv2.cvtColor(blank, cv2.COLOR_BGR2RGB)).convert("RGBA")
    scale = 4
    ink = Image.new("RGBA", (photo.width * scale, photo.height * scale),
                    (0, 0, 0, 0))
    draw = ImageDraw.Draw(ink)

    for cx, cy, selector in RINGS:
        for i, deg in enumerate(ANGLES[selector]):
            a = math.radians(deg)
            major = selector != 0 or i % 2 == 0
            inner = 98.0
            outer = 110.0 if major else 103.0
            x1 = (cx + math.sin(a) * inner) * scale
            y1 = (cy - math.cos(a) * inner) * scale
            x2 = (cx + math.sin(a) * outer) * scale
            y2 = (cy - math.cos(a) * outer) * scale
            draw.line((x1 + 2, y1 + 2, x2 + 2, y2 + 2),
                      fill=(250, 233, 206, 64),
                      width=(2 if major else 1) * scale)
            draw.line((x1, y1, x2, y2), fill=(32, 27, 22, 230),
                      width=(2 if major else 1) * scale)

            label = None
            if selector == 0 and major:
                value = -24 + 2 * i
                label = ("+" if value > 0 else "") + str(value)
            elif selector == 1:
                label = METERS[i]
            elif selector == 2:
                label = MODES[i]
            elif selector == 3:
                label = "OFF" if i == 0 else str(i * 2)
            if label is None:
                continue

            label_radius = (132 if selector == 2 and len(label) > 3
                            else 125 if selector == 2 else 126)
            x = (cx + math.sin(a) * label_radius) * scale
            y = (cy - math.cos(a) * label_radius) * scale
            face = font((12 if selector in (0, 2, 3) else 11) * scale)
            bounds = draw.textbbox((0, 0), label, font=face)
            tx = x - (bounds[2] - bounds[0]) / 2 - bounds[0]
            ty = y - (bounds[3] - bounds[1]) / 2 - bounds[1]
            draw.text((tx + 1, ty + 1), label, font=face,
                      fill=(243, 225, 190, 81))
            draw.text((tx, ty), label, font=face,
                      fill=(32, 27, 22, 244))

        title = ("INPUT", "METER", "BAL-LIMIT", "OUTPUT")[selector]
        face = font((15 if selector == 2 else 16) * scale)
        bounds = draw.textbbox((0, 0), title, font=face)
        tx = (cx - (bounds[2] - bounds[0]) / (2 * scale)) * scale
        ty = (cy + 127) * scale - bounds[1]
        draw.text((tx, ty), title, font=face, fill=(28, 22, 18, 228))

    overlay = ink.resize(photo.size, Image.Resampling.LANCZOS)
    return Image.alpha_composite(photo, overlay).convert("RGB")

def main():
    path = Path(sys.argv[1] if len(sys.argv) > 1 else "assets/panel.jpg")
    original = cv2.imread(str(path), cv2.IMREAD_COLOR)
    if original is None or original.shape[:2] != (941, 1672):
        raise RuntimeError("Expected approved, unmodified 1672x941 panel.jpg")
    blank = restore_metal(original)
    # Regression: each of the four annular bands MUST be materially rewritten.
    for cx, cy, selector in RINGS:
        y, x = np.ogrid[:original.shape[0], :original.shape[1]]
        radius = np.hypot(x - cx, y - cy)
        band = (radius > 100) & (radius < 140)
        mean_delta = np.abs(blank.astype(np.int16) -
                            original.astype(np.int16))[band].mean()
        if mean_delta < 4.0:
            raise AssertionError(
                f"Dial {selector} was not actually cleaned (difference {mean_delta:.2f})")

    result = print_scales(blank)
    result.save(path, quality=95, subsampling=0, optimize=True)
    print("Verified four independently cleaned/photo-matched dial rings:", path)

if __name__ == "__main__":
    main()
