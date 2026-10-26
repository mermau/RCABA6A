"""Regenerate JUCE photo layers from the user-approved BA-6A front-panel image.

Only the front face of a knob is a rotating RGBA sprite. The recessed dial,
bezel, legends, tick marks and cast shadow are ALWAYS in the stationary layer.
The color/photo backing also keeps the meter needle-free for live animation.

Dependencies for asset authoring only: Pillow, numpy and opencv-python.
The plugin itself depends on neither of these libraries.
"""
from pathlib import Path
import numpy as np
from PIL import Image, ImageFilter
import cv2

folder = Path(__file__).resolve().parent
master = Image.open(folder / 'faceplate_master.png').convert('RGB')
if master.size != (1672, 941):
    raise SystemExit('Unexpected original artwork dimensions')

KNOBS = [('input', 278, 649), ('meter', 630, 648),
         ('mode', 1045, 649), ('output', 1387, 650)]
SIDE = 206
# Measurements on the source: the ~80 px rotating cap sits inside the ~90 px
# dark mounting bezel and the ~139 px engraved dial. A 91px circular mask (v0.2)
# incorrectly swept the mounting bezel and some graduation marks around.
CAP_RADIUS = 79.0
ALPHA_FEATHER = 2.0

# Extract ONLY the moveable cap. Fully transparent beyond radius 79px.
for name, cx, cy in KNOBS:
    cap = master.crop((cx - SIDE//2, cy - SIDE//2,
                       cx + SIDE//2, cy + SIDE//2)).convert('RGBA')
    yy, xx = np.mgrid[0:SIDE, 0:SIDE]
    # Pixels sample centers; this prevents an off-center 1px outline.
    radius = np.hypot(xx - SIDE/2, yy - SIDE/2)
    alpha = np.clip((CAP_RADIUS - radius) / ALPHA_FEATHER, 0, 1)
    # An exact circular mask keeps all bezel lettering outside the sprite.
    cap.putalpha(Image.fromarray(np.uint8(np.rint(alpha * 255)), 'L'))
    cap.save(folder / ('knob_' + name + '.png'), optimize=True)

# Static panel must retain the entire fixed metal dial and mounting bezel.
# Hide the original face of each knob INSIDE the new rotating cap; never cover
# the engraved scale/black recess around the cap with a giant black circle.
canvas = np.asarray(master, dtype=np.float32).copy()
for name, cx, cy in KNOBS:
    radius = 82
    y0, y1, x0, x1 = cy - radius, cy + radius + 1, cx - radius, cx + radius + 1
    yy, xx = np.ogrid[-radius:radius+1, -radius:radius+1]
    rr = np.hypot(xx, yy)
    # Shadow of the socket is fixed relative to panel. Cap sprite fully
    # covers the central dark area, while thin transition blends underneath.
    a = np.clip((81 - rr)/4.,0.,1.)[...,None]
    socket_color = np.array([12., 12., 12.], dtype=np.float32)
    canvas[y0:y1,x0:x1] = canvas[y0:y1,x0:x1]*(1-a)+socket_color*a

background = Image.fromarray(np.uint8(np.clip(canvas,0,255)), 'RGB')
# Remove original stationary meter needle, leaving the printed dial intact.
array = cv2.cvtColor(np.asarray(background), cv2.COLOR_RGB2BGR)
needle_mask = np.zeros(array.shape[:2], np.uint8)
cv2.line(needle_mask, (963, 334), (1014, 264), 255, 12, lineType=cv2.LINE_AA)
cv2.line(needle_mask, (972, 321), (1014, 266), 255, 15, lineType=cv2.LINE_AA)
needle_mask[332:,:] = 0
clean = cv2.inpaint(array, needle_mask, 7, cv2.INPAINT_TELEA)
Image.fromarray(cv2.cvtColor(clean,cv2.COLOR_BGR2RGB)).save(
    folder / 'panel.jpg', quality=95, subsampling=0, optimize=True)
print('Built corrected stationary etched dials + 4 cap-only rotating sprites.')
