"""Static renders for visual inspection. NOT screenshots of the running JUCE plugin.

Run: python assets/make_preview.py
Also produces preview_alternate.png and bezel_comparison.png for validation.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageChops
import math
p=Path(__file__).resolve().parent
W,H=1672,941
KNOBS=[('input',278,649,34),('meter',630,648,-137),
       ('mode',1045,649,70),('output',1387,650,16)]

# For these indices the JUCE rotary LookAndFeel maps parameter positions to
# angular endpoints shown on the original marked panel.
def angle(index, position):
    if index in (0,3): return -143 + position*286
    if index==2: return [-47,-94,0,46,79][round(position*4)]
    return [-137,-106,-77,-46,0,43,77,99,119,137][round(position*9)]

def render(positions, dest):
    bg=Image.open(p/'panel.jpg').convert('RGBA')
    for i,(name,cx,cy,orig) in enumerate(KNOBS):
        sprite=Image.open(p/f'knob_{name}.png').convert('RGBA')
        rotated=sprite.rotate(orig-angle(i,positions[i]),Image.Resampling.BICUBIC,
                              expand=False)
        bg.alpha_composite(rotated,(cx-sprite.width//2,cy-sprite.height//2))
    # Gain-reduction at zero in these comparison views. Live meter is JUCE.
    needle=Image.new('RGBA',bg.size,(0,0,0,0))
    d=ImageDraw.Draw(needle)
    d.line((835,458,1008,270),fill=(26,22,18,220),width=4)
    d.line((834,458,1007,270),fill=(194,163,113,190),width=2)
    mask=Image.new('L',bg.size,0)
    ImageDraw.Draw(mask).rectangle((620,136,1065,328),fill=255)
    needle.putalpha(ImageChops.multiply(needle.getchannel('A'),mask))
    bg.alpha_composite(needle)
    bg=bg.convert('RGB')
    bg.resize((1254,706),Image.Resampling.LANCZOS).save(dest,optimize=True)
    return bg

if __name__=='__main__':
    first=render([.65,1,.75,1],p/'preview.png')
    second=render([0,0,.25,0],p/'preview_alternate.png')
    # Side-by-side at 42% for quickly detecting rotating dial or shadows.
    left=first.resize((836,471),Image.Resampling.LANCZOS)
    right=second.resize((836,471),Image.Resampling.LANCZOS)
    sheet=Image.new('RGB',(1672,471))
    sheet.paste(left,(0,0));sheet.paste(right,(836,0))
    sheet.save(p/'bezel_comparison.png',optimize=True)
    print('Produced corrected preview and two-position comparison sheet')
