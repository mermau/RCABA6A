#!/usr/bin/env python3
"""Restore old engraved marks in the original photographed rings (build-time only).
Requires numpy, scipy, OpenCV and Pillow on the GitHub macOS runner.
Never re-run on the already modified output.
"""
import math,sys
from pathlib import Path
import numpy as np
import cv2
from PIL import Image,ImageDraw,ImageFont
from scipy.ndimage import median_filter,gaussian_filter

INPUT=tuple(-150.+i*300./24. for i in range(25))
OUTPUT=tuple(-139.+i*278./15. for i in range(16))
METER=(-136.,-105.,-75.,-46.,-21.,0.,34.,65.,100.,136.)
DIALS=((276,649,0),(630,648,1),(1033,649,2),(1385,650,3))
MODE=(-92.,-48.,0.,42.,79.)

def font(size):
    for path in ('/System/Library/Fonts/Supplemental/Arial.ttf',
                 '/System/Library/Fonts/Supplemental/Arial Narrow.ttf',
                 '/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf',
                 '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'):
        if Path(path).exists(): return ImageFont.truetype(path,size)
    return ImageFont.load_default()

def restore_metal(original):
    """Unified photographic restoration of all four ring bands.

    No circular overlays are added: reconstruct just the ink-bearing annular
    region by robust sampling of genuine metal along each source radius.
    A continuous annular reconstruction removes the old graduations and
    residual glyph fragments that the previous hand-picked masks missed.
    """
    output=original.copy()
    h,w=original.shape[:2]
    for cx,cy,index in DIALS:
        radius=np.arange(94.,153.,.65,dtype=np.float32)[:,None]
        angles=np.linspace(-math.pi,math.pi,1440,endpoint=False,
                           dtype=np.float32)[None,:]
        xs=(cx+radius*np.sin(angles)).astype(np.float32)
        ys=(cy-radius*np.cos(angles)).astype(np.float32)
        sample=cv2.remap(original,xs,ys,cv2.INTER_LINEAR,
                         borderMode=cv2.BORDER_REFLECT).astype(np.float32)
        # Angular median rejects printed letters and scratches but preserves
        # the broad photographic illumination of the metal.
        median=median_filter(sample,size=(1,115,1),mode='wrap')
        smooth=gaussian_filter(median,sigma=(1.3,12.,0),
                              mode=('nearest','wrap','nearest'))
        # Retain subtle photographic texture rather than flat vector metal.
        grain=sample-gaussian_filter(sample,sigma=(1.,2.5,0),
                                     mode=('nearest','wrap','nearest'))
        clean=np.clip(smooth+np.clip(grain,-1.5,1.5),0,255).astype(np.uint8)
        x0=max(0,cx-160);x1=min(w,cx+161)
        y0=max(0,cy-160);y1=min(h,cy+161)
        yy,xx=np.mgrid[y0:y1,x0:x1].astype(np.float32)
        dist=np.hypot(xx-cx,yy-cy)
        angle=np.arctan2(xx-cx,-(yy-cy))
        px=((angle+math.pi)/(2*math.pi)*1440.).astype(np.float32)
        py=np.clip((dist-94.)/.65,0,len(radius)-1).astype(np.float32)
        reconstructed=cv2.remap(clean,px,py,cv2.INTER_LINEAR,
                                borderMode=cv2.BORDER_WRAP)
        # Soft blend only at photo-metal ring edges.
        alpha=(np.clip((dist-94.)/2.5,0,1)*
               np.clip((153.-dist)/2.5,0,1)).astype(np.float32)
        region=output[y0:y1,x0:x1]
        region[:]=np.clip(region*(1-alpha[:,:,None])+
                          reconstructed*alpha[:,:,None],0,255).astype(np.uint8)
    return output

def engrave(photo):
    background=Image.fromarray(cv2.cvtColor(photo,cv2.COLOR_BGR2RGB)).convert('RGBA')
    layer=Image.new('RGBA',background.size,(0,0,0,0))
    draw=ImageDraw.Draw(layer)
    for cx,cy,index in DIALS:
        angles=INPUT if index==0 else METER if index==1 else MODE if index==2 else OUTPUT
        for i,angle in enumerate(angles):
            a=math.radians(angle)
            major=index!=0 or i%2==0
            r1,r2=(99.5,112.) if major else (101.,106.)
            x1,y1=cx+math.sin(a)*r1,cy-math.cos(a)*r1
            x2,y2=cx+math.sin(a)*r2,cy-math.cos(a)*r2
            draw.line((x1+.65,y1+.65,x2+.65,y2+.65),fill=(241,229,210,65),width=2)
            draw.line((x1,y1,x2,y2),fill=(30,25,21,200),width=2 if major else 1)
            label=None
            if index==0 and major:
                value=-24+i*2
                label=('+'+str(value)) if value>0 else str(value)
            elif index==1:
                label=('IN','V1','V2','V3','V4','V5','V6','FIL','COMPR.','BAL.')[i]
            elif index==2:
                label=('B','A','OFF','SINGLE','DUAL')[i]
            elif index==3:
                label='OFF' if i==0 else str(2*i)
            if label:
                rr=125. if index==2 else 126. if index==1 else 128.
                tx,ty=cx+math.sin(a)*rr,cy-math.cos(a)*rr
                face=font(12 if index==2 else 13 if index==1 else 14)
                box=draw.textbbox((0,0),label,font=face)
                tx-=(box[2]-box[0])*.5
                ty-=(box[3]-box[1])*.5
                draw.text((tx+.5,ty+.6),label,font=face,fill=(255,241,211,48))
                draw.text((tx,ty),label,font=face,fill=(25,21,18,238))
        caption={0:'INPUT',1:'METER',2:'BAL-LIMIT',3:'OUTPUT'}[index]
        face=font(20)
        box=draw.textbbox((0,0),caption,font=face)
        draw.text((cx-(box[2]-box[0])*.5,cy+114),caption,
                  font=face,fill=(25,20,17,242))
    merged=Image.alpha_composite(background,layer).convert('RGB')
    return cv2.cvtColor(np.array(merged),cv2.COLOR_RGB2BGR)

def main():
    path=Path(sys.argv[1] if len(sys.argv)>1 else 'assets/panel.jpg')
    original=cv2.imread(str(path),cv2.IMREAD_COLOR)
    if original is None or original.shape[:2]!=(941,1672):
        raise RuntimeError('Original source photo must be 1672 x 941')
    output=engrave(restore_metal(original))
    if not cv2.imwrite(str(path),output,[cv2.IMWRITE_JPEG_QUALITY,95]):
        raise RuntimeError('Could not write corrected photographic faceplate')
    print('Baked photographic dial corrections into',path)

if __name__=='__main__':main()
