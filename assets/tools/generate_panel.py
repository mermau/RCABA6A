#!/usr/bin/env python3
"""Builds assets/panel_clean.jpg from the approved photographic panel.

The approved picture (assets/panel.jpg, 1448x384) carried dial legends, tick
marks, knob bodies and a VU-meter face that do not exist on the RCA BA-6A.
This script removes them and re-synthesises the olive panel texture so that
the plugin can draw the hardware-correct legends, the DB GAIN REDUCTION meter
and the rotating knobs on top of a clean surface:

  1. mask = knob bodies + every bright legend/tick pixel around each knob +
     the "COMPRESSOR / LIMITER" line (the BA-6A is a "LIMITING AMPLIFIER");
  2. low-frequency illumination inside the mask by normalised convolution;
  3. high-frequency paint texture by random patch quilting from clean areas;
  4. static contact shadows for the new knobs (light from the upper left).

Usage: python3 -I assets/tools/generate_panel.py assets/panel.jpg assets/panel_clean.jpg
"""
import sys
import numpy as np
import cv2

KNOBS = [(208, 226), (462, 226), (975, 226), (1228, 226)]
KNOB_RADIUS = 70          # new knob skirt radius (art pixels)
BEZEL = (586, 90, 866, 288)  # meter bezel, never touched
HEADINGS_BOTTOM = 96         # INPUT/METER/BAL-LIMIT/OUTPUT headings stay


def build_mask(img):
    h, w = img.shape[:2]
    lum = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY).astype(np.float32)
    background = cv2.medianBlur(lum.astype(np.uint8), 21).astype(np.float32)
    bright = (lum - background) > 18          # ivory print on olive paint
    yy, xx = np.mgrid[0:h, 0:w]
    mask = np.zeros((h, w), np.uint8)
    for cx, cy in KNOBS:
        r = np.hypot(xx - cx, yy - cy)
        mask[r <= 86] = 255                   # old knob body and its shadow
        ring = (r > 60) & (r < 152) & (yy > HEADINGS_BOTTOM) & (yy < 372)
        mask[ring & bright] = 255
    # (OFF) under the old OUTPUT scale sits further out.
    mask[310:335, 1096:1140][bright[310:335, 1096:1140]] = 255
    # Second title line "COMPRESSOR / LIMITER".
    mask[334:358, 612:842] = 255
    # Never touch the meter bezel or the rack ears.
    x0, y0, x1, y1 = BEZEL
    mask[y0:y1, x0:x1] = 0
    mask[:, :70] = 0
    mask[:, 1380:] = 0
    mask = cv2.dilate(mask, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (9, 9)))
    mask[y0:y1, x0:x1] = 0
    return mask


def low_frequency(img, mask, sigma, exclude=None):
    keep = (mask == 0).astype(np.float32)
    if exclude is not None:
        keep *= (exclude == 0).astype(np.float32)
    f = img.astype(np.float32)
    num = cv2.GaussianBlur(f * keep[..., None], (0, 0), sigma)
    den = cv2.GaussianBlur(keep, (0, 0), sigma)[..., None]
    return num / np.maximum(den, 1e-4)


def donor_patches(img, mask, size, rng, count=900):
    h, w = img.shape[:2]
    forbidden = cv2.dilate(mask, np.ones((size, size), np.uint8))
    x0, y0, x1, y1 = BEZEL
    forbidden[y0 - size:y1 + size, x0 - size:x1 + size] = 255
    forbidden[:, :90] = 255
    forbidden[:, 1360:] = 255
    forbidden[:18, :] = 255
    forbidden[366:, :] = 255
    # screws
    for sx, sy in [(101, 45), (101, 357), (880, 52), (880, 355), (1348, 45), (1348, 357)]:
        cv2.circle(forbidden, (sx, sy), 30 + size, 255, -1)
    # headings and the RCA BA-6A title stay out of the donor set
    forbidden[50:100, :] = 255
    forbidden[295:335, 600:860] = 255
    hf = img.astype(np.float32) - cv2.GaussianBlur(img.astype(np.float32), (0, 0), 6)
    patches = []
    tries = 0
    while len(patches) < count and tries < 200000:
        tries += 1
        x = int(rng.integers(0, w - size)); y = int(rng.integers(0, h - size))
        if forbidden[y:y + size, x:x + size].any():
            continue
        p = hf[y:y + size, x:x + size].copy()
        if rng.random() < 0.5:
            p = p[:, ::-1]
        patches.append(p)
    if not patches:
        raise SystemExit("no donor texture found")
    return patches, hf


def quilt(shape, patches, size, rng):
    h, w = shape[:2]
    step = size // 2
    acc = np.zeros((h + size, w + size, 3), np.float32)
    wsum = np.zeros((h + size, w + size, 1), np.float32)
    win = np.hanning(size + 2)[1:-1]
    win2 = np.outer(win, win)[..., None].astype(np.float32)
    for y in range(-step, h, step):
        for x in range(-step, w, step):
            p = patches[int(rng.integers(0, len(patches)))]
            ys, xs = max(0, y), max(0, x)
            py, px = ys - y, xs - x
            acc[ys:y + size, xs:x + size] += p[py:, px:] * win2[py:, px:]
            wsum[ys:y + size, xs:x + size] += win2[py:, px:]
    out = acc[:h, :w] / np.maximum(wsum[:h, :w], 1e-4)
    return out


def main():
    src, dst = sys.argv[1], sys.argv[2]
    img = cv2.imread(src, cv2.IMREAD_COLOR)
    assert img is not None and img.shape[:2] == (384, 1448), "expected the 1448x384 approved panel"
    rng = np.random.default_rng(0xBA6A)
    mask = build_mask(img)
    # Bright print (title letters, screws' highlights) must not leak into the
    # illumination estimate of the erased areas.
    lum = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY).astype(np.float32)
    bright = ((lum - cv2.medianBlur(lum.astype(np.uint8), 21)) > 12).astype(np.uint8) * 255
    bright = cv2.dilate(bright, np.ones((7, 7), np.uint8))
    lf = low_frequency(img, mask, 22, bright)
    lf_fine = low_frequency(img, mask, 7, bright)
    size = 36
    patches, hf_src = donor_patches(img, mask, size, rng)
    texture = quilt(img.shape, patches, size, rng)
    # Restore the texture contrast lost by overlap averaging.
    ref_std = np.std(np.concatenate([p.reshape(-1, 3) for p in patches[:200]]), axis=0)
    texture *= (ref_std / np.maximum(np.std(texture.reshape(-1, 3), axis=0), 1e-3))
    # Blend the two illumination estimates: fine near the mask border, coarse inside.
    dist = cv2.distanceTransform(mask, cv2.DIST_L2, 5)[..., None]
    t = np.clip(dist / 14.0, 0, 1)
    fill = (lf_fine * (1 - t) + lf * t) + texture
    soft = cv2.GaussianBlur(mask.astype(np.float32) / 255.0, (0, 0), 1.6)[..., None]
    out = img.astype(np.float32) * (1 - soft) + fill * soft

    # Contact shadows of the new knobs: soft, offset towards the lower right.
    h, w = mask.shape
    shadow = np.zeros((h, w), np.float32)
    for cx, cy in KNOBS:
        cv2.circle(shadow, (cx + 5, cy + 8), KNOB_RADIUS + 3, 1.0, -1, cv2.LINE_AA)
    shadow = cv2.GaussianBlur(shadow, (0, 0), 9)
    occlusion = np.zeros((h, w), np.float32)
    for cx, cy in KNOBS:
        cv2.circle(occlusion, (cx, cy), KNOB_RADIUS + 1, 1.0, -1, cv2.LINE_AA)
    occlusion = cv2.GaussianBlur(occlusion, (0, 0), 3)
    out *= (1 - 0.62 * shadow - 0.25 * occlusion)[..., None]

    out = np.clip(out, 0, 255).astype(np.uint8)
    cv2.imwrite(dst, out, [cv2.IMWRITE_JPEG_QUALITY, 94])
    print("wrote", dst)


if __name__ == "__main__":
    main()
