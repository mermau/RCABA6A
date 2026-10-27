#!/usr/bin/env swift
// Generates the Standalone icon using only the macOS runner's AppKit SDK.
import AppKit
import Foundation
let dest = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "assets/app_icon.png"
guard let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil,
 pixelsWide: 1024, pixelsHigh: 1024, bitsPerSample: 8,
 samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
 colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)
else { fatalError("Unable to allocate icon bitmap") }
NSGraphicsContext.saveGraphicsState()
guard let ctx = NSGraphicsContext(bitmapImageRep: bitmap) else { fatalError("Graphics") }
NSGraphicsContext.current = ctx
func colour(_ hex: UInt32) -> NSColor {
    NSColor(calibratedRed: CGFloat((hex >> 16) & 255) / 255.0,
            green: CGFloat((hex >> 8) & 255) / 255.0,
            blue: CGFloat(hex & 255) / 255.0, alpha: 1.0)
}
func label(_ name: String, _ rect: NSRect, _ size: CGFloat) {
    let style = NSMutableParagraphStyle(); style.alignment = .center
    (name as NSString).draw(in: rect, withAttributes: [
      .font: NSFont.boldSystemFont(ofSize: size),
      .foregroundColor: colour(0xF1DBB9),
      .paragraphStyle: style])
}
let caseShape = NSBezierPath(roundedRect: NSRect(x: 30, y: 30, width: 964, height: 964),
                            xRadius: 144, yRadius: 144)
NSGradient(starting: colour(0x84816D), ending: colour(0x333A2F))!.draw(in: caseShape, angle: -65)
colour(0xBFB69B).setStroke()
caseShape.lineWidth = 12
caseShape.stroke()
let ring = NSBezierPath(ovalIn: NSRect(x: 244, y: 408, width: 536, height: 536))
colour(0xE3C5A5).setFill(); ring.fill()
let badge = NSBezierPath(ovalIn: NSRect(x: 258, y: 422, width: 508, height: 508))
NSGradient(starting: colour(0xC6322A), ending: colour(0x68211A))!.draw(in: badge, angle: 80)
label("RCA", NSRect(x: 293, y: 551, width: 438, height: 172), 151)
label("BA-6A", NSRect(x: 169, y: 268, width: 686, height: 133), 128)
label("LIMITING AMPLIFIER", NSRect(x: 118, y: 194, width: 788, height: 55), 41)
label("COLLATERAL EFFECTS", NSRect(x: 118, y: 112, width: 788, height: 46), 30)
ctx.flushGraphics()
NSGraphicsContext.restoreGraphicsState()
guard let png = bitmap.representation(using: .png, properties: [:]) else { fatalError("PNG") }
try png.write(to: URL(fileURLWithPath: dest))
print("App icon created: \(dest)")
