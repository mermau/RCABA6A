#include "PluginEditor.h"
#include <BinaryData.h>
#include <algorithm>
#include <cmath>
#include <utility>
#include "DSP/ControlCalibration.h"

namespace {
constexpr float artWidth = 1672.0f;
constexpr float artHeight = 941.0f;
constexpr float pi = juce::MathConstants<float>::pi;
constexpr float toRad(float degrees) noexcept { return degrees * pi / 180.0f; }
const juce::Colour darkMetal{0xff131618};
const juce::Colour ivory{0xffe9d4ae};

// Artist-approved faceplate remains stationary, including engraved scales and shadows.
constexpr std::array<std::pair<float,float>, 4> knobOrigins{{
    {276.0f,649.0f}, {630.0f,648.0f}, {1033.0f,649.0f}, {1385.0f,650.0f}
}};

// The pointer follows an exact discrete calibration position.
float targetAngle(int knob,float pos) noexcept {
    return ba6a::panel::angleForKnob(knob,pos);
}
} // namespace

void BA6AKnobLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y,
    int width, int height, float pos, float, float, juce::Slider& slider) {
    const float cx = x + width * 0.5f;
    const float cy = y + height * 0.5f;
    const float componentSize = static_cast<float>(std::min(width,height));
    const auto id = slider.getComponentID();
    if (id.startsWith("photo")) {
        const int n = id.substring(5).getIntValue();
        if (juce::isPositiveAndBelow(n,4)) {
            // IMPORTANT: no photograph, bezel, panel shadow or lighting is rotated.
            // A fixed shaded mechanical body obscures old baked-in white pointers.
            // Only the independently painted enamel indicator changes angle.
            const float r = componentSize * 0.445f;
            juce::Path fluted;
            for (int i=0; i<=160; ++i) {
                const float a = (2.0f * pi * i) / 160.0f;
                const float fluting = 0.975f + 0.025f * std::cos(a * 10.0f);
                const float rr = r * fluting;
                const float px = cx + std::sin(a) * rr;
                const float py = cy - std::cos(a) * rr;
                if (i == 0) fluted.startNewSubPath(px,py);
                else fluted.lineTo(px,py);
            }
            fluted.closeSubPath();

            // Stationary cast shadow and relief keep the virtual light fixed.
            g.setColour(juce::Colours::black.withAlpha(0.40f));
            g.fillPath(fluted, juce::AffineTransform::translation(2.5f,4.5f));
            juce::ColourGradient outer(juce::Colour(0xff4b4742),cx-r*0.68f,cy-r*0.78f,
                                        juce::Colour(0xff090a0b),cx+r*0.70f,cy+r*0.90f,false);
            outer.addColour(0.22,juce::Colour(0xff32312f));
            outer.addColour(0.63,juce::Colour(0xff161719));
            g.setGradientFill(outer);
            g.fillPath(fluted);

            g.setColour(juce::Colour(0xff08090b));
            g.strokePath(fluted,juce::PathStrokeType(1.6f));
            g.setColour(juce::Colour(0xffa29a88).withAlpha(0.24f));
            juce::Path bevel;
            bevel.addArc(cx-r*0.93f,cy-r*0.93f,r*1.86f,r*1.86f,
                         pi*1.16f,pi*1.85f,true);
            g.strokePath(bevel,juce::PathStrokeType(2.2f));

            auto face = juce::Rectangle<float>(cx-r*0.79f,cy-r*0.79f,
                                               r*1.58f,r*1.58f);
            juce::ColourGradient faceLight(juce::Colour(0xff333231),
                                           cx-r*0.53f,cy-r*0.71f,
                                           juce::Colour(0xff0d0e10),
                                           cx+r*0.58f,cy+r*0.78f,false);
            faceLight.addColour(0.48,juce::Colour(0xff1a1b1d));
            g.setGradientFill(faceLight);
            g.fillEllipse(face);
            g.setColour(juce::Colour(0xff090a0b).withAlpha(0.77f));
            g.drawEllipse(face,2.0f);

            // Fixed specular arc; no rotation transform applied to this layer.
            juce::Path spec;
            spec.addArc(face.getX()+2.f,face.getY()+2.f,
                        face.getWidth()-4.f,face.getHeight()-4.f,
                        pi*1.20f, pi*1.74f, true);
            g.setColour(juce::Colour(0xfff1e2cc).withAlpha(0.15f));
            g.strokePath(spec,juce::PathStrokeType(2.8f));

            // Pointer is the ONLY mobile graphical layer.
            const float rad = toRad(targetAngle(n,pos));
            juce::Path stripe;
            const float halfWidth = r * 0.043f;
            stripe.startNewSubPath(-halfWidth, -r*0.32f);
            stripe.lineTo(-halfWidth*0.62f, -r*1.008f);
            stripe.lineTo(halfWidth*0.62f, -r*1.008f);
            stripe.lineTo(halfWidth, -r*0.32f);
            stripe.closeSubPath();
            const auto transform = juce::AffineTransform::rotation(rad).translated(cx,cy);
            g.setColour(juce::Colours::black.withAlpha(0.58f));
            g.fillPath(stripe,transform.translated(1.5f,1.6f));
            g.setColour(juce::Colour(0xffeee3d1));
            g.fillPath(stripe,transform);
            g.setColour(juce::Colour(0xff8e8473).withAlpha(0.70f));
            g.strokePath(stripe,juce::PathStrokeType(0.8f),transform);
            return;
        }
    }

    // The six internal adjustment controls use their own static-light shading.
    const float r = componentSize * 0.45f;
    const auto dial = juce::Rectangle<float>(cx-r,cy-r,r*2.f,r*2.f);
    g.setColour(juce::Colours::black.withAlpha(.7f));
    g.fillEllipse(dial.translated(2.f,3.f));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff605e58),cx-r,cy-r,
                                           darkMetal,cx+r,cy+r,false));
    g.fillEllipse(dial);
    g.setColour(juce::Colour(0xffb0a084)); g.drawEllipse(dial,1.2f);
    const float angle = toRad(-135.f + pos*270.f);
    g.setColour(ivory);
    g.drawLine(cx,cy,cx+std::sin(angle)*r*.78f,
               cy-std::cos(angle)*r*.78f,2.f);
}

RCABA6AAudioProcessorEditor::RCABA6AAudioProcessorEditor(RCABA6AAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), audioProcessor(p) {
    faceplate = juce::ImageFileFormat::loadFrom(BinaryData::panel_jpg, BinaryData::panel_jpgSize);
    const char* mainIDs[] = {"input","meter","mode","output"};
    const char* mainNames[] = {"INPUT","METER","BAL–LIMIT","OUTPUT"};
    for (int n=0;n<4;++n) {
        auto& s = mainKnobs[static_cast<std::size_t>(n)];
        configureKnob(s, mainIDs[n],mainBindings[static_cast<std::size_t>(n)],
                      juce::String("photo")+juce::String(n));
        s.setName(mainNames[n]);
        s.setTooltip(juce::String(mainNames[n])+" — drag vertically or use mouse wheel");
    }
    // Place the service drawer ABOVE the main controls, not in the editor's
    // paint(): JUCE paints all child components after the parent's paint().
    servicePanel.setInterceptsMouseClicks(false,true);
    addAndMakeVisible(servicePanel);
    servicePanel.setVisible(false);

    const char* trimIDs[] = {"balanceA","balanceB","zero","fineOutput","hum","heater"};
    for (int n=0;n<6;++n)
        configureKnob(serviceKnobs[static_cast<std::size_t>(n)], trimIDs[n],
                      serviceBindings[static_cast<std::size_t>(n)], juce::String("trim")+juce::String(n));

    serviceButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff292b27));
    serviceButton.setColour(juce::TextButton::textColourOffId,ivory);
    serviceButton.onClick = [this] {
        serviceOpen=!serviceOpen;
        serviceButton.setButtonText(serviceOpen ? "CLOSE SERVICE PANEL" : "OPEN SERVICE PANEL");
    };
    addAndMakeVisible(serviceButton);
    powerButton.setClickingTogglesState(true);
    powerButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff393b33));
    powerButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff526047));
    powerButton.setColour(juce::TextButton::textColourOffId, ivory);
    powerButton.setColour(juce::TextButton::textColourOnId, ivory);
    servicePanel.addAndMakeVisible(powerButton);
    powerBinding = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts,"power",powerButton);
    setResizable(true,true);
    setResizeLimits(940,529,1672,941);
    getConstrainer()->setFixedAspectRatio(static_cast<double>(artWidth/artHeight));
    setSize(1254,706);
    servicePanel.toFront(false);
    serviceButton.toFront(false); // close button always accessible
    startTimerHz(30);
}

RCABA6AAudioProcessorEditor::~RCABA6AAudioProcessorEditor() {
    stopTimer();
    for (auto& slider: mainKnobs) slider.setLookAndFeel(nullptr);
    for (auto& slider: serviceKnobs) slider.setLookAndFeel(nullptr);
}

void RCABA6AAudioProcessorEditor::configureKnob(
    juce::Slider& s, const char* parameter,
    std::unique_ptr<SliderAttachment>& attachment, const juce::String& id) {
    s.setComponentID(id);
    // Vertical dragging prevents diagonal drift on multi-position selectors.
    const auto discrete = juce::String(parameter)=="mode" || juce::String(parameter)=="meter";
    s.setSliderStyle(discrete ? juce::Slider::RotaryVerticalDrag
                              : juce::Slider::RotaryHorizontalVerticalDrag);
    s.setMouseDragSensitivity(juce::String(parameter)=="mode" ? 300
                              : juce::String(parameter)=="meter" ? 420
                              : juce::String(parameter)=="input" ? 560 : 460);
    s.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
    s.setRotaryParameters(-.79f*pi,.79f*pi,true);
    s.setLookAndFeel(&knobStyle);
    if (id.startsWith("trim"))
        servicePanel.addAndMakeVisible(s);
    else
        addAndMakeVisible(s);
    attachment=std::make_unique<SliderAttachment>(audioProcessor.apvts,parameter,s);
}

void RCABA6AAudioProcessorEditor::resized() {
    const auto sx = static_cast<float>(getWidth()) / artWidth;
    const auto sy = static_cast<float>(getHeight()) / artHeight;
    auto place=[sx,sy](juce::Component& component, float cx, float cy, float width, float height) {
        component.setBounds(juce::roundToInt((cx-width*.5f)*sx),
                            juce::roundToInt((cy-height*.5f)*sy),
                            juce::roundToInt(width*sx),juce::roundToInt(height*sy));
    };
    for (int n=0;n<4;++n) {
        const auto [cx,cy]=knobOrigins[static_cast<std::size_t>(n)];
        place(mainKnobs[static_cast<std::size_t>(n)],cx,cy,206.f,206.f);
    }
    servicePanel.setBounds(getLocalBounds());
    for (int n=0;n<6;++n)
        place(serviceKnobs[static_cast<std::size_t>(n)],
              168.f+static_cast<float>(n)*221.f,782.f,104.f,104.f);
    place(powerButton,1515.f,782.f,154.f,48.f);
    place(serviceButton,842.f,913.f,270.f,34.f);
}

void RCABA6AAudioProcessorEditor::timerCallback() {
    const auto wanted=juce::jlimit(0.f,20.f,audioProcessor.meterGainReduction.load(std::memory_order_relaxed));
    meterDisplay += .22f * (wanted-meterDisplay);
    if (std::abs(meterDisplay-wanted)<.002f) meterDisplay=wanted;
    const float target=serviceOpen?1.f:0.f;
    serviceOpening += .23f * (target-serviceOpening);
    if (std::abs(serviceOpening-target)<.005f) serviceOpening=target;
    // The drawer and all its child controls share a single alpha and z-order.
    servicePanel.setVisible(serviceOpening > .005f);
    servicePanel.setAlpha(serviceOpening);
    powerButton.setButtonText(powerButton.getToggleState()?"POWER ON":"POWER OFF");
    repaint();
}

void RCABA6AAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black);
    const juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(
        static_cast<float>(getWidth())/artWidth,
        static_cast<float>(getHeight())/artHeight));
    if (faceplate.isValid()) g.drawImageAt(faceplate,0,0);
    drawNeedle(g);
}

void RCABA6AAudioProcessorEditor::drawNeedle(juce::Graphics& g) {
    const juce::Graphics::ScopedSaveState save(g);
    // The needle's physical pivot is hidden by the COMPRESSION escutcheon;
    // draw only the part behind the glass, never on top of the nameplate.
    g.reduceClipRegion(juce::Rectangle<int>(620,136,446,192));
    const float t=juce::jlimit(0.f,1.f,meterDisplay/20.f);
    const float x=1008.f-368.f*t;
    const float y=270.f-57.f*(1.f-std::pow(2.f*t-1.f,2.f));
    const float pivotX=835.f,pivotY=458.f;
    g.setColour(juce::Colour(0xff422b12).withAlpha(.30f));
    g.drawLine(pivotX+2.f,pivotY+2.f,x+2.f,y+2.f,5.f);
    g.setColour(juce::Colour(0xff181713));
    g.drawLine(pivotX,pivotY,x,y,3.4f);
    g.setColour(juce::Colour(0xffbba06d));
    g.drawLine(pivotX-.7f,pivotY,x-.7f,y,1.f);
}

void BA6AServicePanel::paint(juce::Graphics& g) {
    const juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(
        static_cast<float>(getWidth()) / artWidth,
        static_cast<float>(getHeight()) / artHeight));
    // The parent's alpha animates the whole panel uniformly, including controls.
    constexpr float alpha = 1.0f;
    g.setColour(juce::Colour(0xff171a17).withMultipliedAlpha(alpha*.97f));
    g.fillRoundedRectangle(50.f,694.f,1572.f,221.f,14.f);
    g.setColour(juce::Colour(0xffc0b39a).withMultipliedAlpha(alpha*.75f));
    g.drawRoundedRectangle(juce::Rectangle<float>(51.f,695.f,1570.f,218.f),13.f,2.f);
    const char* labels[]={"BALANCE A","BALANCE B","ZERO ADJ.","FINE OUTPUT","HUM ADJ.","HEATER ADJ."};
    g.setFont(juce::Font(juce::FontOptions(17.0f)));
    g.setColour(ivory.withMultipliedAlpha(alpha));
    for(int n=0;n<6;++n) {
        g.drawFittedText(labels[n],juce::roundToInt(106.f+n*221.f),849,124,30,
                         juce::Justification::centred,1);
    }
    g.drawFittedText("CHASSIS POWER",1441,847,147,33,juce::Justification::centred,1);
    g.setFont(juce::Font(juce::FontOptions(15.0f)));
    g.drawFittedText("INTERNAL ALIGNMENT",610,705,451,24,juce::Justification::centred,1);
}
