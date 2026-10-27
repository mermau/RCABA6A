#!/usr/bin/env python3
"""Renders assets/knob_strip.png: the BA-6A bar knob at every detent angle.

All four front-panel knobs of the BA-6A are identical black bar knobs with a
white line on the pointer end (RCA IB-18-2035-2, Fig. 1). The knob is modelled
as a height field and shaded with a FIXED light from the upper left, so when
the knob turns its geometry rotates while highlights and shadows stay put -
unlike rotating a photograph.

Frames: angles -165 .. +180 degrees in 15-degree steps (24 frames), clockwise
from twelve o'clock, i.e. every INPUT/OUTPUT/METER/BAL-LIMIT detent.
Output: one horizontal strip, FRAME x FRAME pixels per frame at 2x scale
(a 1x art pixel = 2 strip pixels).

Usage: python3 -I assets/tools/render_knobs.py assets/knob_strip.png
"""
import sys
import numpy as np
import cv2

SCALE = 2           # strip pixels per art pixel
SS = 3              # supersampling
R = 70.0            # skirt radius, art pixels
HALF = 76.0         # half frame size, art pixels
FRAME = int(2 * HALF * SCALE)
ANGLES = [-165 + 15 * i for i in range(24)]
LIGHT = np.array([-0.42, -0.62, 0.66])
LIGHT /= np.linalg.norm(LIGHT)


def smooth_edge(d, width):
    """0 outside, 1 inside, smooth over `width` (d = signed distance inside)."""
    t = np.clip(d / width, 0, 1)
    return np.sqrt(1 - (1 - t) ** 2)


def height_field(x, y):
    r = np.hypot(x, y)
    # Skirt with a rounded rim and fine turning rings.
    skirt = 7.0 * smooth_edge(R - r, 7.0)
    skirt += 0.035 * np.sin(r * 1.9) * (r < R - 6) * (r > 54)
    # Cap.
    cap = 7.5 * smooth_edge(53.0 - r, 5.0)
    # Bar: pointer towards -y (twelve o'clock at 0 degrees).
    tail, tip = 56.0, -63.0
    t = np.clip((y - tip) / (tail - tip), 0, 1)
    half_w = 10.5 + 6.5 * t
    dx = half_w - np.abs(x)
    dy_tip = y - tip
    dy_tail = tail - y
    # Rounded pointer end.
    tip_round = np.where(y < tip + half_w, half_w - np.hypot(x, y - (tip + half_w)), dx)
    d = np.minimum(np.minimum(dx, tip_round), dy_tail)
    d = np.where(dy_tip < 0, -1.0, d)
    bar = 17.0 * smooth_edge(d, 6.0)
    z = skirt + np.maximum(cap, bar * (r < R + 1)) * (skirt > 0.01)
    z = np.maximum(z, skirt)
    # White line on top of the pointer half (slightly recessed inlay).
    line = (np.abs(x) < 2.1) & (y > tip + 5) & (y < -14)
    z_top = skirt + bar
    z = np.where(line, np.maximum(z - 0.35, skirt), z)
    return z, line & (bar > 16.0), r


def render(angle_deg):
    n = FRAME * SS
    c = (np.arange(n) + 0.5) / (SCALE * SS) - HALF
    X, Y = np.meshgrid(c, c)
    a = np.radians(angle_deg)
    # Rotate sample points into knob coordinates (clockwise screen rotation).
    xr = np.cos(a) * X + np.sin(a) * Y
    yr = -np.sin(a) * X + np.cos(a) * Y
    z, line, r = height_field(xr, yr)
    px = 1.0 / (SCALE * SS)
    gy, gx = np.gradient(z, px)
    nrm = np.dstack([-gx, -gy, np.ones_like(z)])
    nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
    ndl = np.clip((nrm * LIGHT).sum(2), 0, 1)
    view = np.array([0, 0, 1.0])
    hvec = LIGHT + view; hvec /= np.linalg.norm(hvec)
    ndh = np.clip((nrm * hvec).sum(2), 0, 1)
    # Cast shadows by marching the height field towards the light.
    shadow = np.ones_like(z)
    step = 0.6  # art pixels
    lxy = LIGHT[:2] / np.linalg.norm(LIGHT[:2])
    rise = LIGHT[2] / np.linalg.norm(LIGHT[:2])
    for k in range(1, 40):
        dxp = lxy[0] * step * k * SCALE * SS
        dyp = lxy[1] * step * k * SCALE * SS
        M = np.float32([[1, 0, -dxp], [0, 1, -dyp]])
        shifted = cv2.warpAffine(z.astype(np.float32), M, (n, n), flags=cv2.INTER_LINEAR,
                                 borderMode=cv2.BORDER_CONSTANT, borderValue=0)
        blocked = shifted - (z + rise * step * k) > 0.05
        shadow = np.minimum(shadow, np.where(blocked, 0.35, 1.0))
    shadow = cv2.GaussianBlur(shadow.astype(np.float32), (0, 0), 1.2 * SS)
    # Ambient occlusion from local height differences.
    ao = np.clip(1.0 - 0.06 * np.maximum(cv2.GaussianBlur(z.astype(np.float32), (0, 0), 6 * SS) - z, 0), 0.55, 1)
    albedo = np.where(line[..., None], np.array([0.86, 0.82, 0.71]), np.array([0.012, 0.012, 0.012]))
    gloss = np.where(line, 0.25, 1.0)
    env = 0.0 + 0.22 * np.clip(-nrm[..., 1] - 0.3 * nrm[..., 0], 0, 1) ** 1.5 + 0.015 * np.clip(nrm[..., 2], 0, 1)
    diffuse = albedo * (0.05 + 1.0 * ndl[..., None] * shadow[..., None]) * ao[..., None]
    spec = (0.60 * ndh ** 90 + 0.015 * ndh ** 10) * shadow * gloss
    rim = 0.10 * (1 - nrm[..., 2]) ** 2 * np.clip(-nrm[..., 1] - 0.2 * nrm[..., 0], 0, 1)
    col = diffuse + (spec + rim + env * 0.07 * gloss * ao)[..., None] * np.array([0.95, 0.96, 1.0])
    col = np.clip(col, 0, 1) ** (1 / 2.2)
    alpha = np.clip((R + 0.5 - r) * SCALE * SS / 1.0, 0, 1)
    rgba = np.dstack([col * alpha[..., None], alpha]).astype(np.float32)
    small = cv2.resize(rgba, (FRAME, FRAME), interpolation=cv2.INTER_AREA)
    a_ = np.maximum(small[..., 3:4], 1e-6)
    rgb = np.where(small[..., 3:4] > 0, small[..., :3] / a_, 0)
    out = np.dstack([rgb, small[..., 3:4]])
    return (np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8)


def main():
    dst = sys.argv[1]
    frames = [render(a) for a in ANGLES]
    strip = np.concatenate(frames, axis=1)
    bgra = cv2.cvtColor(strip, cv2.COLOR_RGBA2BGRA)
    cv2.imwrite(dst, bgra, [cv2.IMWRITE_PNG_COMPRESSION, 9])
    print("wrote", dst, strip.shape)


if __name__ == "__main__":
    main()
