#!/usr/bin/env swift
import AppKit
import Foundation
let dest=CommandLine.arguments.count>1 ? CommandLine.arguments[1] : "assets/app_icon.png"
guard let bitmap=NSBitmapImageRep(bitmapDataPlanes:nil,pixelsWide:1024,pixelsHigh:1024,bitsPerSample:8,samplesPerPixel:4,hasAlpha:true,isPlanar:false,colorSpaceName:.deviceRGB,bytesPerRow:0,bitsPerPixel:0) else { fatalError("Bitmap") }
NSGraphicsContext.saveGraphicsState()
guard let context=NSGraphicsContext(bitmapImageRep:bitmap) else { fatalError("Context") }
NSGraphicsContext.current=context
func c(_ rgb:UInt32,_ alpha:CGFloat=1)->NSColor {
  NSColor(calibratedRed:CGFloat((rgb>>16)&255)/255,green:CGFloat((rgb>>8)&255)/255,blue:CGFloat(rgb&255)/255,alpha:alpha)
}
func label(_ s:String,_ rect:NSRect,_ size:CGFloat,_ colour:NSColor) {
 let p=NSMutableParagraphStyle();p.alignment = .center
 let attrs:[NSAttributedString.Key:Any]=[.font:NSFont.boldSystemFont(ofSize:size),.foregroundColor:colour,.paragraphStyle:p]
 (s as NSString).draw(in:rect,withAttributes:attrs)
}
let outer=NSBezierPath(roundedRect:NSRect(x:15,y:15,width:994,height:994),xRadius:140,yRadius:140)
c(0x151513).setFill();outer.fill()
let panel=NSBezierPath(roundedRect:NSRect(x:35,y:35,width:954,height:954),xRadius:120,yRadius:120)
NSGradient(starting:c(0x837b65),ending:c(0x343a2f))!.draw(in:panel,angle:-65)
c(0xcec2ad).setStroke();panel.lineWidth=8;panel.stroke()
for i in 0..<130 {
 let x=CGFloat((i*397+117)%830+95),y=CGFloat((i*269+43)%830+95)
 c(i%3==0 ? 0xdfc69d : 0x141e18,0.24).setFill()
 NSBezierPath(ovalIn:NSRect(x:x,y:y,width:CGFloat(i%4+1),height:CGFloat(i%3+1))).fill()
}
let rim=NSBezierPath(ovalIn:NSRect(x:254,y:427,width:516,height:516))
c(0xe2c8a1).setFill();rim.fill()
let red=NSBezierPath(ovalIn:NSRect(x:265,y:438,width:494,height:494))
NSGradient(starting:c(0xc5332d),ending:c(0x681e18))!.draw(in:red,angle:80)
c(0xe8c69b).setStroke();red.lineWidth=7;red.stroke()
label("RCA",NSRect(x:290,y:568,width:444,height:164),158,c(0xf5dfb8))
label("BA-6A",NSRect(x:184,y:272,width:656,height:152),128,c(0xf1d4a5))
label("LIMITING AMPLIFIER",NSRect(x:112,y:192,width:800,height:57),43,c(0xeddbba))
for x in [115.0,884.0] {
 for y in [115.0,873.0] {
  c(0x1d1b19).setFill()
  NSBezierPath(ovalIn:NSRect(x:x,y:y,width:30,height:30)).fill()
 }
}
context.flushGraphics()
NSGraphicsContext.restoreGraphicsState()
guard let png=bitmap.representation(using:.png,properties:[:]) else {fatalError("PNG")}
try png.write(to:URL(fileURLWithPath:dest))
print("Created app icon \(dest)")
