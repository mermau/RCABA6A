#!/usr/bin/env python3
"""Bake exactly one set of dial markings into the approved photographic panel.

Run once on the original tracked 1672x941 assets/panel.jpg, before CMake
embeds BinaryData. No runtime overlays, synthetic discs or rotating photos.
The metal reconstruction uses the source photo's angular illumination and
microtexture; the original mounting rims, chassis and BAL-LIMIT are untouched.
"""
import math
import sys
from pathlib import Path
import cv2
import numpy as np
from scipy.ndimage import gaussian_filter, percentile_filter
from PIL import Image, ImageDraw, ImageFont

INPUT = tuple(-150.0 + i * 12.5 for i in range(25))
METER = (-136., -94., -67., -44., -22., 0., 28., 60., 93., 136.)
OUTPUT = (-139., -120.46667, -101.93333, -83.4, -64.86667, -46.33333,
          -27.8, -9.26667, 9.26667, 27.8, 46.33333, 64.86667,
          83.4, 101.93333, 120.46667, 139.)
DIALS = ((276,649,0,INPUT), (630,648,1,METER), (1385,650,3,OUTPUT))

def reconstruct(original):
    output = original.astype(np.float32).copy()
    for index, (cx, cy, selector, angles) in enumerate(DIALS):
        xx, yy = np.meshgrid(np.arange(cx-154, cx+155, dtype=np.float32),
                             np.arange(cy-154, cy+155, dtype=np.float32))
        radius = np.hypot(xx-cx, yy-cy)
        theta = np.arctan2(xx-cx, cy-yy)
        radii = np.arange(86,150,.5,dtype=np.float32)
        azimuth = np.linspace(-math.pi,math.pi,1440,endpoint=False,dtype=np.float32)
        map_x = cx + np.sin(azimuth[None,:])*radii[:,None]
        map_y = cy - np.cos(azimuth[None,:])*radii[:,None]
        photo = cv2.remap(original,map_x,map_y,cv2.INTER_LINEAR,
                          borderMode=cv2.BORDER_REFLECT).astype(np.float32)
        # Replace old ink and ticks, not the photographic ring geometry.
        metal = percentile_filter(photo,percentile=82,size=(3,351,1),mode="wrap")
        metal = gaussian_filter(metal,sigma=(4,35,0),
                                mode=("nearest","wrap","nearest"))
        grain = photo-gaussian_filter(photo,sigma=(.8,4.,0),
                                      mode=("nearest","wrap","nearest"))
        rng = np.random.default_rng(6826+index)
        noise = gaussian_filter(rng.normal(0,2,size=metal.shape).astype(np.float32),
                                sigma=(.25,.5,0))
        metal = np.uint8(np.clip(metal+.58*np.clip(grain,-2.4,2.4)+noise,0,255))
        map_r=np.clip((radius-86.)/.5,0,len(radii)-1).astype(np.float32)
        map_theta=((theta+math.pi)/(2*math.pi)*len(azimuth)).astype(np.float32)
        replacement=cv2.remap(metal,map_theta,map_r,cv2.INTER_LINEAR,
                               borderMode=cv2.BORDER_WRAP)
        # Keep the old physical rim & shadows, only resurfacing photographed metal.
        alpha=np.clip(np.minimum((radius-87.)/3.,(149.-radius)/2.5),0,1)
        roi=output[cy-154:cy+155,cx-154:cx+155]
        roi[:]=roi*(1-alpha[:,:,None])+replacement*alpha[:,:,None]
    return np.uint8(np.clip(output,0,255))

def font(size):
    for name in ("/System/Library/Fonts/Supplemental/Arial.ttf",
                 "/System/Library/Fonts/Supplemental/Arial Narrow.ttf",
                 "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                 "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"):
        if Path(name).exists(): return ImageFont.truetype(name,size)
    raise RuntimeError("No appropriate TrueType font on the runner")

def engrave(blank):
    # Typography is rasterized once into the photoreal panel at 3x resolution.
    scale=3
    photo=Image.fromarray(cv2.cvtColor(blank,cv2.COLOR_BGR2RGB)).convert("RGBA")
    ink=Image.new("RGBA",(photo.width*scale,photo.height*scale),(0,0,0,0))
    draw=ImageDraw.Draw(ink)
    for cx,cy,control,angles in DIALS:
        for contact,angle in enumerate(angles):
            rad=math.radians(angle)
            major=control!=0 or contact%2==0
            inner,outer=(96,105.5) if major else (96,100.5)
            coords=[]
            for radius in (inner,outer):
                coords.append((round(scale*(cx+math.sin(rad)*radius)),
                               round(scale*(cy-math.cos(rad)*radius))))
            draw.line([(p[0]+1,p[1]+1) for p in coords],fill=(247,235,211,115),
                      width=2*scale)
            draw.line(coords,fill=(29,25,21,230),
                      width=(2 if major else 1)*scale)
            if control==0:
                label_value=-24+contact*2
                label=("+" if label_value>0 else "")+str(label_value) if major else None
                label_radius=122
            elif control==1:
                label=("IN","V1","V2","V3","V4","V5","V6",
                       "FIL","COMPR","BAL")[contact]
                label_radius=128
            else:
                label="OFF" if contact==0 else str(contact*2)
                label_radius=122
            if label:
                x=cx+math.sin(rad)*label_radius
                y=cy-math.cos(rad)*label_radius
                typeface=font((11 if control in (0,1) else 12)*scale)
                bbox=draw.textbbox((0,0),label,font=typeface)
                px=x*scale-(bbox[2]-bbox[0])/2-bbox[0]
                py=y*scale-(bbox[3]-bbox[1])/2-bbox[1]
                draw.text((px+1,py+1),label,font=typeface,fill=(242,222,189,120))
                draw.text((px,py),label,font=typeface,fill=(30,26,21,239))
        label={0:"INPUT",1:"METER",3:"OUTPUT"}[control]
        typeface=font(16*scale)
        bbox=draw.textbbox((0,0),label,font=typeface)
        x=cx*scale-(bbox[2]-bbox[0])/2
        draw.text((x,(cy+124)*scale-bbox[1]),label,
                  font=typeface,fill=(28,23,21,232))
    ink=ink.resize(photo.size,Image.Resampling.LANCZOS)
    return Image.alpha_composite(photo,ink).convert("RGB")

def main():
    path=Path(sys.argv[1] if len(sys.argv)>1 else "assets/panel.jpg")
    original=cv2.imread(str(path),cv2.IMREAD_COLOR)
    if original is None or original.shape[:2]!=(941,1672):
        raise RuntimeError("Expected approved, unmodified 1672x941 panel.jpg")
    result=engrave(reconstruct(original))
    result.save(path,quality=95,subsampling=0,optimize=True)
    print("Single baked photographic dial layer:",path)

if __name__=="__main__":
    main()
