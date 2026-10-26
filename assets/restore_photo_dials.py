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
DIALS=((276,649,0),(630,648,1),(1385,650,3))

def font(size):
    for path in ('/System/Library/Fonts/Supplemental/Arial.ttf',
                 '/System/Library/Fonts/Supplemental/Arial Narrow.ttf',
                 '/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf',
                 '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'):
        if Path(path).exists(): return ImageFont.truetype(path,size)
    return ImageFont.load_default()

def blank_rings(original):
    out=original.copy()
    yy,xx=np.indices(original.shape[:2],dtype=np.float32)
    for cx,cy,index in DIALS:
        az=np.linspace(-math.pi,math.pi,720,endpoint=False)[None,:]
        radii=np.arange(95.,145.,.5)[:,None]
        mapx=(cx+np.sin(az)*radii).astype(np.float32)
        mapy=(cy-np.cos(az)*radii).astype(np.float32)
        photo=cv2.remap(original,mapx,mapy,cv2.INTER_LINEAR,
                        borderMode=cv2.BORDER_REFLECT).astype(np.float32)
        lum=photo.mean(2)
        baseline=median_filter(lum,size=(1,41),mode='wrap')
        angles=np.broadcast_to(az,photo.shape[:2])
        rr=np.broadcast_to(radii,photo.shape[:2])
        okay=(lum>=baseline-11)&(lum>90)
        okay &= ~((np.abs(angles)>math.pi-.32)&(rr>110))
        weights=okay.astype(np.float32)
        blend=gaussian_filter(photo*weights[:,:,None],(1.2,35.,0),
                              mode=('nearest','wrap','nearest'))
        norm=gaussian_filter(weights,(1.2,35.),mode=('nearest','wrap'))
        metal=blend/(norm[:,:,None]+1.e-4)
        grain=photo-gaussian_filter(photo,(1.4,2.5,0),mode=('nearest','wrap','nearest'))
        metal=np.clip(metal+np.clip(grain,-2.,2.),0,255).astype(np.uint8)
        dx,dy=xx-cx,yy-cy
        r=np.hypot(dx,dy)
        theta=np.arctan2(dx,-dy)
        row=np.clip((r-95.)/.5,0,len(radii)-1).astype(np.float32)
        col=((theta+math.pi)/(2*math.pi)*720.).astype(np.float32)
        restored=cv2.remap(metal,col,row,cv2.INTER_LINEAR,borderMode=cv2.BORDER_WRAP)
        alpha=(np.clip((r-95.)/4.,0,1)*np.clip((145.-r)/3.,0,1)).astype(np.float32)
        out=np.clip(out*(1-alpha[:,:,None])+restored*alpha[:,:,None],0,255).astype(np.uint8)
    return out

def remove_relics(photo):
    glyphs={
      276:[(274,526),(218,545),(180,581),(159,628),(170,705),(185,724),
           (207,754),(332,753),(369,726),(387,681),(392,628),(368,579),(326,546)],
      630:[(629,526),(558,552),(518,605),(502,658),(536,728),
           (698,550),(736,602),(744,658),(725,712)],
      1385:[(1370,528),(1305,545),(1267,626),(1262,661),(1280,712),
            (1308,750),(1450,751),(1490,651),(1478,594),(1434,549)]}
    mask=np.zeros(photo.shape[:2],dtype=np.uint8)
    for cx,cy,index in DIALS:
        for x,y in glyphs[cx]:cv2.ellipse(mask,(x,y),(20,12),0,0,360,255,-1)
        cv2.rectangle(mask,(cx-52,cy+113),(cx+52,cy+141),255,-1)
    return cv2.inpaint(photo,mask,5,cv2.INPAINT_TELEA)

def engrave(photo):
    background=Image.fromarray(cv2.cvtColor(photo,cv2.COLOR_BGR2RGB)).convert('RGBA')
    layer=Image.new('RGBA',background.size,(0,0,0,0))
    draw=ImageDraw.Draw(layer)
    for cx,cy,index in DIALS:
        angles=INPUT if index==0 else METER if index==1 else OUTPUT
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
            elif index==3:
                label='OFF' if i==0 else str(2*i)
            if label:
                rr=126. if index==1 else 128.
                tx,ty=cx+math.sin(a)*rr,cy-math.cos(a)*rr
                face=font(13 if index==1 else 14)
                box=draw.textbbox((0,0),label,font=face)
                tx-=(box[2]-box[0])*.5
                ty-=(box[3]-box[1])*.5
                draw.text((tx+.5,ty+.6),label,font=face,fill=(255,241,211,48))
                draw.text((tx,ty),label,font=face,fill=(25,21,18,238))
        caption={0:'INPUT',1:'METER',3:'OUTPUT'}[index]
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
    output=engrave(remove_relics(blank_rings(original)))
    if not cv2.imwrite(str(path),output,[cv2.IMWRITE_JPEG_QUALITY,95]):
        raise RuntimeError('Could not write corrected photographic faceplate')
    print('Baked photographic dial corrections into',path)

if __name__=='__main__':main()
