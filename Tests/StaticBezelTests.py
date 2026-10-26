"""Regression: the engraved dial and fixed cast shadows must not rotate.

The two test stills are the same raster faceplate with radically different cap
positions, rendered by assets/make_preview.py with the same path as JUCE's
rotated RGBA knob sprites. If even a sliver of an etched ring has been
captured by a sprite this test will find changing pixels outside the cap.
"""
from pathlib import Path
from PIL import Image, ImageChops
import math
import sys

assets=Path(sys.argv[1])
first=Image.open(assets/'preview.png').convert('RGB')
second=Image.open(assets/'preview_alternate.png').convert('RGB')
assert first.size==second.size==(1254,706)
scale=1254/1672
knobs=[('input',278,649),('meter',630,648),
       ('mode',1045,649),('output',1387,650)]
changed_on_cap=0
for name,cx,cy in knobs:
    sprite=Image.open(assets/f'knob_{name}.png').convert('RGBA')
    assert sprite.size==(206,206)
    # Sprite may not contain any pixels from the permanently stationary bezel.
    for y in range(206):
        for x in range(206):
            radius=math.hypot(x-103,y-103)
            if radius>=82 and sprite.getpixel((x,y))[3]!=0:
                raise AssertionError(f'{name}: stationary dial embedded in sprite at radius {radius:.1f}')
    xc,yc=cx*scale,cy*(706/941)
    # The entire engraved annulus (beyond the small rotating cap) is invariant.
    for yy in range(max(0,int(yc-140*scale)), min(706,int(yc+140*scale))):
        for xx in range(max(0,int(xc-140*scale)), min(1254,int(xc+140*scale))):
            rr=math.hypot((xx+.5-xc)/scale,(yy+.5-yc)/(706/941))
            if 84 <= rr <= 139 and first.getpixel((xx,yy))!=second.getpixel((xx,yy)):
                raise AssertionError(f'{name}: bezel/ticks moved at {xx},{yy} r={rr:.1f}')
            if rr < 62 and first.getpixel((xx,yy))!=second.getpixel((xx,yy)):
                changed_on_cap+=1
if changed_on_cap < 1000:
    raise AssertionError('Knob animation not apparent between test poses')
print(f'Static bezels preserved for all four knobs; {changed_on_cap} moved cap pixels')
