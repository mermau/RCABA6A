#include "PluginEditor.h"
#include <BinaryData.h>
#include "DSP/ControlCalibration.h"
#include <cmath>

using namespace ba6a;

namespace {
constexpr float pi = juce::MathConstants<float>::pi;
float rad(float deg) noexcept { return deg * pi / 180.0f; }
juce::Point<float> polar(juce::Point<float> c, float r, float deg) noexcept {
    return {c.x + r * std::sin(rad(deg)), c.y - r * std::cos(rad(deg))};
}
const juce::Colour ink {0xffe6dabd};          // ivory print on the olive panel
const juce::Colour inkShadow {0x8c000000};
const juce::String minus = juce::String(juce::CharPointer_UTF8("\xe2\x88\x92"));

// Knob filmstrip: 24 frames, -165 .. +180 degrees, 15 degrees apart, 2x scale.
constexpr int stripFrames = 24;
constexpr float stripHalfArt = 76.0f;

juce::String dialLegend(int knob, int i) {
    if (knob == 0) {
        if (i == 0) return "OFF";
        const int v = hw::inputLabelDbm(i);
        return v < 0 ? minus + juce::String(-v) : juce::String(v);
    }
    if (knob == 3) return i == 0 ? "OFF" : juce::String(hw::outputLabelDbm(i));
    if (knob == 2) {
        static const char* names[]{"A", "B", "OFF", "SINGLE", "DUAL"};
        return names[i];
    }
    static const char* meter[]{"OFF", "GR", "V1", "V2", "V3", "V4", "V5", "V6", "FIL", "COARSE\nBAL", "FINE\nBAL"};
    return meter[i];
}
bool legendPrinted(int knob, int i) {
    return (knob == 0 || knob == 3) ? (i % 2 == 0) : true;   // dial plates letter every other contact
}

// --------------------------------------------------- meter geometry (art) --
const juce::Rectangle<float> meterFace {606.0f, 111.0f, 239.0f, 140.0f};
const juce::Point<float> meterPivot {725.5f, 300.0f};
constexpr float meterScaleRadius = 150.0f;
float meterDegrees(float fraction) noexcept { return panel::meterAngleForFraction(fraction); }
} // namespace

juce::Font gui::Fonts::semiBold(float h) const {
    return juce::Font(juce::FontOptions(semi).withHeight(h));
}
juce::Font gui::Fonts::regular(float h) const {
    return juce::Font(juce::FontOptions(medium).withHeight(h));
}

// =============================================================== DetentKnob
DetentKnob::DetentKnob(juce::RangedAudioParameter& p, int knobType, const juce::Image& s)
    : param(p),
      attachment(p, [this](float v) { index = juce::roundToInt(v); repaint(); }, nullptr),
      type(knobType), positions(panel::positionsForKnob(knobType)), strip(s) {
    setTitle(p.getName(64));
    setDescription("Stepped switch. Drag, scroll or click a legend; double-click for the default.");
    setRepaintsOnMouseActivity(false);
    attachment.sendInitialUpdate();
}

juce::Point<float> DetentKnob::toArt(juce::Point<float> local) const noexcept {
    const float s = artScale();
    return {local.x / s - gui::knobLegendRadius, local.y / s - gui::knobLegendRadius};
}

bool DetentKnob::hitTest(int x, int y) {
    const auto a = toArt({static_cast<float>(x), static_cast<float>(y)});
    const float r = a.getDistanceFromOrigin();
    return r <= 74.0f || legendAt({static_cast<float>(x), static_cast<float>(y)}) >= 0;
}

int DetentKnob::legendAt(juce::Point<float> local) const noexcept {
    const auto a = toArt(local);
    const float r = a.getDistanceFromOrigin();
    if (r < 76.0f || r > 126.0f) return -1;
    const float deg = juce::radiansToDegrees(std::atan2(a.x, -a.y));
    int best = -1; float bestDiff = 1e9f;
    for (int i = 0; i < positions; ++i) {
        float diff = std::abs(deg - panel::angleForIndex(type, i));
        diff = std::min(diff, 360.0f - diff);
        if (diff < bestDiff) { bestDiff = diff; best = i; }
    }
    const float tolerance = (type == 0 || type == 3) ? 7.5f : 13.0f;
    return bestDiff <= tolerance ? best : -1;
}

void DetentKnob::setIndexFromUser(int newIndex) {
    newIndex = juce::jlimit(0, positions - 1, newIndex);
    if (newIndex == index) return;
    if (dragging) attachment.setValueAsPartOfGesture(static_cast<float>(newIndex));
    else attachment.setValueAsCompleteGesture(static_cast<float>(newIndex));
}

void DetentKnob::mouseDown(const juce::MouseEvent& e) {
    const int legend = legendAt(e.position);
    if (toArt(e.position).getDistanceFromOrigin() > 74.0f && legend >= 0) {
        setIndexFromUser(legend);
        return;
    }
    dragging = true;
    dragStart = index;
    attachment.beginGesture();
}

void DetentKnob::mouseDrag(const juce::MouseEvent& e) {
    if (!dragging) return;
    const float pixelsPerStep = positions > 11 ? 11.0f : 20.0f;
    const float travel = static_cast<float>(e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY());
    setIndexFromUser(dragStart + juce::roundToInt(travel / pixelsPerStep));
}

void DetentKnob::mouseUp(const juce::MouseEvent&) {
    if (dragging) attachment.endGesture();
    dragging = false;
}

void DetentKnob::mouseDoubleClick(const juce::MouseEvent&) {
    setIndexFromUser(juce::roundToInt(param.convertFrom0to1(param.getDefaultValue())));
}

void DetentKnob::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
    wheelAccumulator += (w.isReversed ? -w.deltaY : w.deltaY) + w.deltaX;
    if (std::abs(wheelAccumulator) >= 0.12f) {
        setIndexFromUser(index + (wheelAccumulator > 0 ? 1 : -1));
        wheelAccumulator = 0.0f;
    }
}

void DetentKnob::paint(juce::Graphics& g) {
    const float angle = panel::angleForIndex(type, index);
    const int frame = juce::jlimit(0, stripFrames - 1, juce::roundToInt((angle + 165.0f) / 15.0f));
    const int fs = strip.getHeight();
    const float s = artScale();
    const float c = gui::knobLegendRadius * s, half = stripHalfArt * s;
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(strip, juce::roundToInt(c - half), juce::roundToInt(c - half),
                juce::roundToInt(2 * half), juce::roundToInt(2 * half),
                frame * fs, 0, fs, fs);
}

// ================================================================ TrimPot
TrimPot::TrimPot(juce::RangedAudioParameter& p, juce::String text)
    : label(std::move(text)), param(p),
      attachment(p, [this](float v) { normalised = param.convertTo0to1(v); repaint(); }, nullptr) {
    setTitle(p.getName(64));
    setTooltip(p.getName(64) + " - drag (shift for fine), double-click to reset");
    attachment.sendInitialUpdate();
}

void TrimPot::paint(juce::Graphics& g) {
    const auto b = getLocalBounds().toFloat();
    const float r = std::min(b.getWidth(), b.getHeight()) * 0.5f - 2.0f;
    const auto c = b.getCentre();
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillEllipse(juce::Rectangle<float>(2 * r, 2 * r).withCentre(c.translated(1.5f, 2.5f)));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffd9d2c2), c.x - r, c.y - r,
                                           juce::Colour(0xff6d675b), c.x + r, c.y + r, false));
    g.fillEllipse(juce::Rectangle<float>(2 * r, 2 * r).withCentre(c));
    g.setColour(juce::Colour(0xff3a372f));
    g.drawEllipse(juce::Rectangle<float>(2 * r, 2 * r).withCentre(c), 1.2f);
    const float a = rad(-135.0f + 270.0f * normalised);
    const juce::Point<float> d{std::sin(a), -std::cos(a)};
    juce::Path slot;
    slot.addRectangle(-r * 0.86f, -r * 0.13f, r * 1.72f, r * 0.26f);
    g.setColour(juce::Colour(0xff26231d));
    g.fillPath(slot, juce::AffineTransform::rotation(a + pi * 0.5f).translated(c));
    g.setColour(juce::Colour(0xffb2261c));
    g.fillEllipse(juce::Rectangle<float>(r * 0.22f, r * 0.22f).withCentre(c + d * (r * 0.68f)));
}

void TrimPot::mouseDown(const juce::MouseEvent&) { dragStartValue = normalised; attachment.beginGesture(); }
void TrimPot::mouseDrag(const juce::MouseEvent& e) {
    const float span = e.mods.isShiftDown() ? 900.0f : 220.0f;
    const float n = juce::jlimit(0.0f, 1.0f, dragStartValue +
        static_cast<float>(e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) / span);
    attachment.setValueAsPartOfGesture(param.convertFrom0to1(n));
}
void TrimPot::mouseUp(const juce::MouseEvent&) { attachment.endGesture(); }
void TrimPot::mouseDoubleClick(const juce::MouseEvent&) {
    attachment.setValueAsCompleteGesture(param.convertFrom0to1(param.getDefaultValue()));
}
void TrimPot::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    const float step = (e.mods.isShiftDown() ? 0.002f : 0.01f) * (w.deltaY > 0 ? 1.0f : -1.0f);
    attachment.setValueAsCompleteGesture(param.convertFrom0to1(juce::jlimit(0.0f, 1.0f, normalised + step)));
}

// ============================================================ ToggleLever
ToggleLever::ToggleLever(juce::RangedAudioParameter& p, juce::String upT, juce::String downT,
                         const gui::Fonts& f)
    : param(p), attachment(p, [this](float v) { up = v > 0.5f; repaint(); }, nullptr),
      upText(std::move(upT)), downText(std::move(downT)), fonts(f) {
    setTitle(p.getName(64));
    setTooltip(p.getName(64) + " - click to flip");
    attachment.sendInitialUpdate();
}

void ToggleLever::paint(juce::Graphics& g) {
    const float s = static_cast<float>(getWidth()) / artW;
    g.addTransform(juce::AffineTransform::scale(s));
    const juce::Point<float> c {artW * 0.5f, artH * 0.5f};
    const juce::Colour engraving {0xff1d1b17};

    // Position legends, the active one in full ink.
    g.setFont(fonts.semiBold(13.0f));
    g.setColour(engraving.withAlpha(up ? 1.0f : 0.45f));
    g.drawText(upText, juce::Rectangle<float>(0.0f, 0.0f, artW, 15.0f), juce::Justification::centred);
    g.setColour(engraving.withAlpha(up ? 0.45f : 1.0f));
    g.drawText(downText, juce::Rectangle<float>(0.0f, artH - 15.0f, artW, 15.0f), juce::Justification::centred);

    // Hex nut.
    juce::Path nut;
    const float rn = 15.0f;
    for (int k = 0; k < 6; ++k) {
        const float a = rad(30.0f + 60.0f * static_cast<float>(k));
        const juce::Point<float> p {c.x + rn * std::cos(a), c.y + rn * std::sin(a)};
        if (k == 0) nut.startNewSubPath(p); else nut.lineTo(p);
    }
    nut.closeSubPath();
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillPath(nut, juce::AffineTransform::translation(1.5f, 2.5f));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe4e1d8), c.x - rn, c.y - rn,
                                           juce::Colour(0xff5d5a52), c.x + rn, c.y + rn, false));
    g.fillPath(nut);
    g.setColour(juce::Colour(0xff3a3832));
    g.strokePath(nut, juce::PathStrokeType(0.8f));

    // Threaded bushing.
    const float rb = 9.0f;
    const auto bush = juce::Rectangle<float>(2 * rb, 2 * rb).withCentre(c);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff9b978c), c.x - rb, c.y - rb,
                                           juce::Colour(0xff36342e), c.x + rb, c.y + rb, false));
    g.fillEllipse(bush);
    g.setColour(juce::Colour(0xff2a2824).withAlpha(0.7f));
    g.drawEllipse(bush.reduced(1.5f), 0.6f);
    g.drawEllipse(bush.reduced(3.0f), 0.6f);
    g.setColour(juce::Colour(0xff17160f));
    g.fillEllipse(bush.reduced(4.2f));

    // The lever leaves the bushing towards the viewer and is thrown only about
    // 18 degrees off the panel normal (as on a real bat-handle toggle), so
    // from the front it is strongly foreshortened: a short shaft and the
    // rounded bat end seen almost end-on.
    const float dir = up ? -1.0f : 1.0f;
    const float leverLength = 34.0f, throwDeg = 18.0f;
    const float reach = leverLength * std::sin(rad(throwDeg));      // ~10.5 art px on screen
    const float height = leverLength * std::cos(rad(throwDeg));     // tip height above the panel
    const juce::Point<float> tip {c.x, c.y + dir * reach};
    const float wBase = 5.0f, rTip = 5.0f;

    // Shadow: the lever stands off the panel, light from the upper left.
    {
        const juce::Point<float> shadowTip = tip + juce::Point<float>(0.30f, 0.38f) * height;
        juce::Path sh;
        sh.startNewSubPath(c.x - wBase * 0.5f, c.y);
        sh.lineTo(shadowTip.x - rTip * 0.9f, shadowTip.y);
        sh.lineTo(shadowTip.x + rTip * 0.9f, shadowTip.y);
        sh.lineTo(c.x + wBase * 0.5f, c.y);
        sh.closeSubPath();
        sh.addEllipse(juce::Rectangle<float>(rTip * 2.0f, rTip * 1.6f).withCentre(shadowTip));
        g.setColour(juce::Colours::black.withAlpha(0.16f));
        g.fillPath(sh, juce::AffineTransform::translation(0.8f, 0.8f));
        g.setColour(juce::Colours::black.withAlpha(0.20f));
        g.fillPath(sh);
    }

    // Pivot collar (the lever's root inside the bushing).
    const auto collar = juce::Rectangle<float>(wBase * 1.6f, wBase * 1.6f).withCentre(c);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffeeebe3), collar.getX(), collar.getY(),
                                           juce::Colour(0xff4c4a43), collar.getRight(), collar.getBottom(), false));
    g.fillEllipse(collar);
    g.setColour(juce::Colour(0xff26241f).withAlpha(0.7f));
    g.drawEllipse(collar, 0.6f);

    // Short visible part of the shaft, widening towards the viewer.
    juce::Path shaft;
    shaft.startNewSubPath(c.x - wBase * 0.5f, c.y);
    shaft.lineTo(tip.x - rTip * 0.92f, tip.y);
    shaft.lineTo(tip.x + rTip * 0.92f, tip.y);
    shaft.lineTo(c.x + wBase * 0.5f, c.y);
    shaft.closeSubPath();
    {
        juce::ColourGradient across(juce::Colour(0xff3d3b36), c.x - rTip, 0.0f,
                                    juce::Colour(0xff2b2a26), c.x + rTip, 0.0f, false);
        across.addColour(0.28, juce::Colour(0xfff6f4ee));
        across.addColour(0.45, juce::Colour(0xffc9c5ba));
        across.addColour(0.78, juce::Colour(0xff6d6a61));
        g.setGradientFill(across);
        g.fillPath(shaft);
        g.setColour(juce::Colour(0xff26241f).withAlpha(0.7f));
        g.strokePath(shaft, juce::PathStrokeType(0.5f));
    }

    // Bat end, nearest to the viewer: a chrome dome seen almost end-on.
    const auto end = juce::Rectangle<float>(rTip * 2.0f, rTip * 2.0f * std::cos(rad(throwDeg))).withCentre(tip);
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillEllipse(end.translated(0.6f, 0.9f));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfffdfcf8), end.getX() + end.getWidth() * 0.33f,
                                           end.getY() + end.getHeight() * 0.30f,
                                           juce::Colour(0xff4f4c45), end.getRight(), end.getBottom(), true));
    g.fillEllipse(end);
    g.setColour(juce::Colour(0xff26241f).withAlpha(0.75f));
    g.drawEllipse(end, 0.6f);
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.fillEllipse(juce::Rectangle<float>(rTip * 0.55f, rTip * 0.45f)
                      .withCentre({end.getX() + end.getWidth() * 0.34f, end.getY() + end.getHeight() * 0.32f}));
}

void ToggleLever::mouseUp(const juce::MouseEvent& e) {
    if (getLocalBounds().contains(e.getPosition()))
        attachment.setValueAsCompleteGesture(up ? 0.0f : 1.0f);
}

// ============================================================ PanelButton
void PanelButton::paintButton(juce::Graphics& g, bool highlighted, bool down) {
    auto b = getLocalBounds().toFloat().reduced(1.0f);
    const float r = b.getHeight() * 0.3f;
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(b.translated(0.0f, 1.0f), r);
    g.setGradientFill(juce::ColourGradient(juce::Colour(down ? 0xff15170f : 0xff2b2e22), 0, b.getY(),
                                           juce::Colour(0xff1a1c14), 0, b.getBottom(), false));
    g.fillRoundedRectangle(b, r);
    g.setColour(ink.withAlpha(highlighted ? 0.75f : 0.4f));
    g.drawRoundedRectangle(b, r, 1.0f);
    g.setColour(ink.withAlpha(highlighted ? 1.0f : 0.85f));
    g.setFont(fonts.semiBold(b.getHeight() * 0.78f).withExtraKerningFactor(0.08f));
    g.drawText(getButtonText(), b, juce::Justification::centred);
}

// ========================================================== ChassisDrawer
ChassisDrawer::ChassisDrawer(juce::AudioProcessorValueTreeState& s, const gui::Fonts& f) : fonts(f) {
    auto p = [&s](const char* id) { return s.getParameter(id); };
    const char* idsTrim[]{ids::balanceA, ids::balanceB, ids::zero, ids::fineOutput, ids::hum, ids::heater};
    const char* labels[]{"BALANCE A", "BALANCE B", "ZERO ADJ.", "FINE OUTPUT", "HUM ADJ.", "HEATER VOLTAGE ADJ."};
    for (size_t i = 0; i < trims.size(); ++i) {
        trims[i] = std::make_unique<TrimPot>(*p(idsTrim[i]), labels[i]);
        addAndMakeVisible(*trims[i]);
    }
    power = std::make_unique<ToggleLever>(*p(ids::power), "ON", "OFF", fonts);
    link = std::make_unique<ToggleLever>(*p(ids::stereoLink), "LINK", "DUAL", fonts);
    noise = std::make_unique<ToggleLever>(*p(ids::noise), "ON", "OFF", fonts);
    mains = std::make_unique<ToggleLever>(*p(ids::mains), "50 c/s", "60 c/s", fonts);
    for (auto* c : {power.get(), link.get(), noise.get(), mains.get()}) addAndMakeVisible(*c);
}

void ChassisDrawer::resized() {
    const float s = static_cast<float>(getWidth()) / gui::artWidth;
    auto place = [s](juce::Component& c, float x, float y, float w, float h) {
        c.setBounds(juce::Rectangle<float>(w * s, h * s).withCentre({x * s, y * s}).toNearestInt());
    };
    for (size_t i = 0; i < trims.size(); ++i)
        place(*trims[i], 110.0f + 135.0f * static_cast<float>(i), 66.0f, 54.0f, 54.0f);
    const float lw = ToggleLever::artW, lh = ToggleLever::artH;
    place(*power, 975.0f, 76.0f, lw, lh);
    place(*link, 1110.0f, 76.0f, lw, lh);
    place(*noise, 1215.0f, 76.0f, lw, lh);
    place(*mains, 1320.0f, 76.0f, lw, lh);
}

void ChassisDrawer::paint(juce::Graphics& g) {
    const float s = static_cast<float>(getWidth()) / gui::artWidth;
    g.addTransform(juce::AffineTransform::scale(s));
    const juce::Rectangle<float> all {0.0f, 0.0f, gui::artWidth, gui::drawerHeight};
    // RCA "light umber gray" chassis seen with the hinged panel open.
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff9c9686), 0, 0, juce::Colour(0xff6f6a5d), 0, gui::drawerHeight, false));
    g.fillRect(all);
    juce::Random rng(0xBA6A);
    for (int i = 0; i < 900; ++i) {
        g.setColour(juce::Colours::white.withAlpha(0.025f + 0.03f * rng.nextFloat()));
        const float y = rng.nextFloat() * gui::drawerHeight;
        g.fillRect(rng.nextFloat() * gui::artWidth, y, 30.0f + 160.0f * rng.nextFloat(), 0.6f);
    }
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.65f), 0, 0, juce::Colours::transparentBlack, 0, 22, false));
    g.fillRect(0.0f, 0.0f, gui::artWidth, 22.0f);
    g.setColour(juce::Colour(0xff1d1b17));
    g.setFont(fonts.semiBold(16.5f));
    for (size_t i = 0; i < 6; ++i) {
        const float x = 110.0f + 135.0f * static_cast<float>(i);
        g.drawFittedText(trims[i]->label, juce::Rectangle<int>(juce::roundToInt(x - 68), 100, 136, 20), juce::Justification::centred, 1);
    }
    g.drawText("POWER", juce::Rectangle<float>(915.0f, 132.0f, 120.0f, 18.0f), juce::Justification::centred);
    g.setColour(juce::Colour(0xff26231d));
    g.setFont(fonts.semiBold(14.5f));
    g.drawText("STEREO", juce::Rectangle<float>(1050.0f, 132.0f, 120.0f, 18.0f), juce::Justification::centred);
    g.drawText("NOISE + HUM", juce::Rectangle<float>(1155.0f, 132.0f, 120.0f, 18.0f), juce::Justification::centred);
    g.drawText("MAINS", juce::Rectangle<float>(1260.0f, 132.0f, 120.0f, 18.0f), juce::Justification::centred);
    g.setFont(fonts.regular(13.0f).withExtraKerningFactor(0.1f));
    g.drawText("PLUGIN OPTIONS", juce::Rectangle<float>(1110.0f, 10.0f, 210.0f, 16.0f), juce::Justification::centred);
    g.drawLine(1075.0f, 18.0f, 1135.0f, 18.0f, 1.0f);
    g.drawLine(1295.0f, 18.0f, 1360.0f, 18.0f, 1.0f);
    g.setColour(juce::Colour(0xff1d1b17));
    g.setFont(fonts.regular(15.0f));
    g.drawFittedText("BALANCING THE 6SK7 TUBES (instruction book p. 7):  INPUT to OFF  -  METER to COARSE BAL  -  "
                     "BAL-LIMIT to A, turn BALANCE A for minimum  -  BAL-LIMIT to B, turn BALANCE B for minimum  -  "
                     "repeat on FINE BAL until there is no deflection.\n"
                     "ZERO ADJ.: BAL-LIMIT to OFF, METER to GR, set full scale.   HEATER: METER to FIL, set the pointer on the HEATER mark.   "
                     "HUM ADJ. only affects an external preamplifier supply.",
                     juce::Rectangle<int>(40, 160, 1368, 34), juce::Justification::centredLeft, 2, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawRect(all, 1.0f);
}

// ================================================================= Editor
RCABA6AAudioProcessorEditor::RCABA6AAudioProcessorEditor(RCABA6AAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), audioProcessor(p) {
    fonts.semi = juce::Typeface::createSystemTypefaceFor(BinaryData::barlow_semibold_ttf, BinaryData::barlow_semibold_ttfSize);
    fonts.medium = juce::Typeface::createSystemTypefaceFor(BinaryData::barlow_medium_ttf, BinaryData::barlow_medium_ttfSize);
    panel = juce::ImageFileFormat::loadFrom(BinaryData::panel_clean_jpg, BinaryData::panel_clean_jpgSize);
    strip = juce::ImageFileFormat::loadFrom(BinaryData::knob_strip_png, BinaryData::knob_strip_pngSize);

    const char* knobIds[]{ids::input, ids::meter, ids::balLimit, ids::output};
    for (size_t i = 0; i < knobs.size(); ++i) {
        knobs[i] = std::make_unique<DetentKnob>(*p.apvts.getParameter(knobIds[i]), static_cast<int>(i), strip);
        addAndMakeVisible(*knobs[i]);
    }
    drawer = std::make_unique<ChassisDrawer>(p.apvts, fonts);
    addChildComponent(*drawer);

    chassisButton.setTooltip("Open the hinged panel: chassis controls (balance, zero, fine output, heater, power)");
    chassisButton.onClick = [this] { setChassisOpen(!chassisOpen); };
    addAndMakeVisible(chassisButton);

    setResizable(true, true);
    const int width = static_cast<int>(p.apvts.state.getProperty("editorWidth", 1158));
    applyAspect(juce::jlimit(900, 2172, width));
    startTimerHz(60);
}

RCABA6AAudioProcessorEditor::~RCABA6AAudioProcessorEditor() { stopTimer(); }

void RCABA6AAudioProcessorEditor::applyAspect(int width) {
    const float h = gui::panelHeight + (chassisOpen ? gui::drawerHeight : 0.0f);
    const double ratio = static_cast<double>(gui::artWidth / h);
    setResizeLimits(900, juce::roundToInt(900 / ratio), 2172, juce::roundToInt(2172 / ratio));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio(ratio);
    setSize(width, juce::roundToInt(width / ratio));
}

void RCABA6AAudioProcessorEditor::setChassisOpen(bool open) {
    chassisOpen = open;
    chassisButton.setButtonText(open ? "CLOSE" : "CHASSIS");
    drawer->setVisible(open);
    applyAspect(getWidth());
}

void RCABA6AAudioProcessorEditor::resized() {
    const float s = static_cast<float>(getWidth()) / gui::artWidth;
    for (size_t i = 0; i < knobs.size(); ++i) {
        const auto c = gui::knobCentres[i];
        const float r = gui::knobLegendRadius;
        knobs[i]->setBounds(juce::Rectangle<float>((c.x - r) * s, (c.y - r) * s, 2 * r * s, 2 * r * s).toNearestInt());
    }
    chassisButton.setBounds(juce::Rectangle<float>(1236.0f * s, 361.0f * s, 82.0f * s, 18.0f * s).toNearestInt());
    drawer->setBounds(0, juce::roundToInt(gui::panelHeight * s), getWidth(),
                      getHeight() - juce::roundToInt(gui::panelHeight * s));
    audioProcessor.apvts.state.setProperty("editorWidth", getWidth(), nullptr);
}

juce::Rectangle<int> RCABA6AAudioProcessorEditor::meterAreaOnScreen() const {
    const float s = static_cast<float>(getWidth()) / gui::artWidth;
    return (meterFace.expanded(3.0f) * s).getSmallestIntegerContainer();
}

void RCABA6AAudioProcessorEditor::advanceMeter(float seconds) {
    // D'Arsonval movement: about 0.3 s to settle, slightly under-damped.
    const float target = audioProcessor.meterNeedle.load(std::memory_order_relaxed);
    const float wn = 2.0f * pi * 1.7f, zeta = 0.72f;
    const int steps = 8;
    const float dt = seconds / static_cast<float>(steps);
    for (int k = 0; k < steps; ++k) {
        const float acc = wn * wn * (target - needle) - 2.0f * zeta * wn * needleVelocity;
        needleVelocity += acc * dt;
        needle += needleVelocity * dt;
        if (needle < -0.015f) { needle = -0.015f; needleVelocity *= -0.25f; }   // rest pin
        if (needle > 1.07f) { needle = 1.07f; needleVelocity *= -0.25f; }       // end stop
    }
    const bool powered = audioProcessor.apvts.getRawParameterValue(ids::power)->load() > 0.5f;
    lamp += (powered ? 0.12f : 0.05f) * ((powered ? 1.0f : 0.0f) - lamp);
}

void RCABA6AAudioProcessorEditor::timerCallback() {
    const float before = needle, lampBefore = lamp;
    advanceMeter(1.0f / 60.0f);
    if (std::abs(needle - before) > 1.0e-4f || std::abs(lamp - lampBefore) > 1.0e-3f)
        repaint(meterAreaOnScreen());
}

void RCABA6AAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black);
    const float s = static_cast<float>(getWidth()) / gui::artWidth;
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(s));
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    if (panel.isValid())
        g.drawImage(panel, juce::Rectangle<float>(0.0f, 0.0f, gui::artWidth, gui::panelHeight));
    drawLegends(g);
    drawTitle(g);
    drawMeter(g);
}

void RCABA6AAudioProcessorEditor::drawTitle(juce::Graphics& g) const {
    {
        // Small maker's mark, bottom left, in the same ivory print.
        const auto f = fonts.semiBold(12.5f).withExtraKerningFactor(0.22f);
        g.setFont(f);
        const juce::Rectangle<float> r {128.0f, 362.0f, 220.0f, 16.0f};
        g.setColour(inkShadow);
        g.drawText("COLLATERAL EFFECTS", r.translated(0.7f, 0.9f), juce::Justification::centredLeft);
        g.setColour(ink.withAlpha(0.82f));
        g.drawText("COLLATERAL EFFECTS", r, juce::Justification::centredLeft);
    }
    // The BA-6A is a "Limiting Amplifier" (RCA MI-11225), not a compressor.
    const auto f = fonts.regular(21.0f).withExtraKerningFactor(0.10f);
    g.setFont(f);
    const juce::Rectangle<float> r {540.0f, 334.0f, 372.0f, 24.0f};
    g.setColour(inkShadow);
    g.drawText("LIMITING AMPLIFIER", r.translated(0.8f, 1.0f), juce::Justification::centred);
    g.setColour(juce::Colour(0xffddd2b8));
    g.drawText("LIMITING AMPLIFIER", r, juce::Justification::centred);
}

void RCABA6AAudioProcessorEditor::drawLegends(juce::Graphics& g) const {
    for (int knob = 0; knob < 4; ++knob) {
        const auto c = gui::knobCentres[static_cast<size_t>(knob)];
        const int n = panel::positionsForKnob(knob);
        const bool stepped = knob == 0 || knob == 3;
        for (int i = 0; i < n; ++i) {
            const float deg = panel::angleForIndex(knob, i);
            const bool major = !stepped || legendPrinted(knob, i);
            const float r0 = 77.0f, r1 = major ? 88.0f : 84.0f;
            for (int pass = 0; pass < 2; ++pass) {
                const auto off = pass == 0 ? juce::Point<float>(0.8f, 1.0f) : juce::Point<float>();
                g.setColour(pass == 0 ? inkShadow : ink);
                g.drawLine(juce::Line<float>(polar(c, r0, deg) + off, polar(c, r1, deg) + off), major ? 2.1f : 1.5f);
            }
            if (!major) continue;
            const auto text = dialLegend(knob, i);
            const bool twoLines = text.containsChar('\n');
            const float h = knob == 1 ? (twoLines ? 14.0f : 17.5f) : (knob == 2 ? 18.0f : 18.5f);
            g.setFont(fonts.semiBold(h));
            const float rr = knob == 1 ? (twoLines ? 108.0f : 103.0f)
                           : knob == 2 ? 102.0f + 10.0f * std::abs(std::sin(rad(deg)))
                           : 103.0f + (text == "OFF" ? 5.0f : 0.0f);
            auto p = polar(c, rr, deg);
            if (knob == 2) p.x += 20.0f * std::sin(rad(deg)) * (text.length() > 2 ? 1.0f : 0.2f);
            const auto box = juce::Rectangle<float>(76.0f, twoLines ? 30.0f : 21.0f).withCentre(p);
            for (int pass = 0; pass < 2; ++pass) {
                g.setColour(pass == 0 ? inkShadow : ink);
                g.drawFittedText(text, (pass == 0 ? box.translated(0.8f, 1.0f) : box).toNearestInt(),
                                 juce::Justification::centred, twoLines ? 2 : 1, 1.0f);
            }
        }
    }
}

void RCABA6AAudioProcessorEditor::drawMeter(juce::Graphics& g) const {
    const juce::Graphics::ScopedSaveState save(g);
    juce::Path face;
    face.addRoundedRectangle(meterFace, 11.0f);
    g.reduceClipRegion(face);

    // Card illuminated by two 3.2 V lamps under the scale.
    const float lit = 0.42f + 0.58f * lamp;
    auto shade = [lit](juce::Colour c) { return c.withMultipliedBrightness(lit); };
    g.setGradientFill(juce::ColourGradient(shade(juce::Colour(0xfff3dfb2)), 725.0f, 200.0f,
                                           shade(juce::Colour(0xffb99360)), 606.0f, 111.0f, true));
    g.fillRect(meterFace);
    for (float x : {655.0f, 796.0f}) {
        juce::ColourGradient glow(juce::Colour(0xfffff1c8).withAlpha(0.32f * lamp), x, 252.0f,
                                  juce::Colours::transparentWhite, x, 175.0f, true);
        g.setGradientFill(glow);
        g.fillRect(meterFace);
    }

    const juce::Colour printInk = juce::Colour(0xff1d1914).withAlpha(0.92f);
    const auto& P = meterPivot;
    const float R = meterScaleRadius;
    // CHECK band (tube cathode currents) between the 7 and 5 marks.
    {
        const float a0 = rad(meterDegrees(panel::meterFractionForGr(7.0f)));
        const float a1 = rad(meterDegrees(panel::meterFractionForGr(5.0f)));
        juce::Path band;
        band.addCentredArc(P.x, P.y, R - 3.0f, R - 3.0f, 0.0f, a0, a1, true);
        band.addCentredArc(P.x, P.y, R - 10.0f, R - 10.0f, 0.0f, a1, a0, false);
        band.closeSubPath();
        g.setColour(juce::Colour(0xff7f8a72).withAlpha(0.55f));
        g.fillPath(band);
    }
    // Scale arc.
    juce::Path arc;
    arc.addCentredArc(P.x, P.y, R, R, 0.0f, rad(meterDegrees(0.0f)), rad(meterDegrees(1.0f)), true);
    g.setColour(printInk);
    g.strokePath(arc, juce::PathStrokeType(1.6f));
    // Ticks and numerals.
    g.setFont(fonts.semiBold(15.0f));
    const int marks[]{20, 15, 10, 7, 5, 3, 1, 0};
    for (int gr : marks) {
        const float deg = meterDegrees(panel::meterFractionForGr(static_cast<float>(gr)));
        g.drawLine(juce::Line<float>(polar(P, R, deg), polar(P, R + 8.0f, deg)), 1.5f);
        if (gr == 1) continue;                          // unlettered on the RCA card
        const auto t = polar(P, R + 17.0f, deg);
        g.drawText(juce::String(gr), juce::Rectangle<float>(24.0f, 14.0f).withCentre(t),
                   juce::Justification::centred);
    }
    g.drawLine(juce::Line<float>(polar(P, R - 2.0f, meterDegrees(0.0f)), polar(P, R + 7.0f, meterDegrees(0.0f))), 1.5f);
    // HEATER reference mark.
    {
        const float deg = meterDegrees(panel::meterHeaterMark);
        juce::Path tri;
        tri.addTriangle(polar(P, R - 1.5f, deg), polar(P, R - 8.0f, deg - 1.3f), polar(P, R - 8.0f, deg + 1.3f));
        g.fillPath(tri);
    }
    // Lettering that follows the arc.
    auto arcText = [&g, &P](const juce::String& text, const juce::Font& f, float radius, float centreDeg) {
        g.setFont(f);
        const float total = juce::GlyphArrangement::getStringWidth(f, text);
        float deg = centreDeg - juce::radiansToDegrees(total / radius) * 0.5f;
        for (int i = 0; i < text.length(); ++i) {
            const juce::String ch = text.substring(i, i + 1);
            const float w = juce::GlyphArrangement::getStringWidth(f, ch);
            const float mid = deg + juce::radiansToDegrees(w * 0.5f / radius);
            const auto pos = polar(P, radius, mid);
            g.saveState();
            g.addTransform(juce::AffineTransform::rotation(rad(mid), pos.x, pos.y));
            g.drawText(ch, juce::Rectangle<float>(w + 6.0f, f.getHeight()).withCentre(pos),
                       juce::Justification::centred);
            g.restoreState();
            deg += juce::radiansToDegrees(w / radius);
        }
    };
    arcText("DB GAIN REDUCTION", fonts.semiBold(13.0f).withExtraKerningFactor(0.07f), R + 32.5f, 0.0f);
    arcText("CHECK", fonts.regular(12.0f).withExtraKerningFactor(0.05f), R - 19.5f,
            meterDegrees(0.5f * (panel::meterFractionForGr(7.0f) + panel::meterFractionForGr(5.0f))));
    arcText("HEATER", fonts.regular(12.0f).withExtraKerningFactor(0.05f), R - 19.5f,
            meterDegrees(panel::meterHeaterMark));

    // Pointer.
    const float deg = meterDegrees(needle);
    juce::Path pointer;
    pointer.startNewSubPath(-1.3f, -40.0f);
    pointer.lineTo(-0.45f, -(R + 9.0f));
    pointer.lineTo(0.45f, -(R + 9.0f));
    pointer.lineTo(1.3f, -40.0f);
    pointer.closeSubPath();
    const auto place = juce::AffineTransform::rotation(rad(deg)).translated(P.x, P.y);
    g.setColour(juce::Colours::black.withAlpha(0.18f * (0.4f + lamp)));
    g.fillPath(pointer, place.translated(1.6f, 2.2f));
    g.setColour(juce::Colour(0xff15120f));
    g.fillPath(pointer, place);

    // Glass and bezel shadow.
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.42f), 0, meterFace.getY(),
                                           juce::Colours::transparentBlack, 0, meterFace.getY() + 16.0f, false));
    g.fillRect(meterFace);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.10f), meterFace.getX(), meterFace.getY(),
                                           juce::Colours::transparentWhite, meterFace.getCentreX(), meterFace.getBottom(), false));
    g.fillRect(meterFace);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.strokePath(face, juce::PathStrokeType(2.0f));
}
