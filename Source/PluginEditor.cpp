#include "PluginEditor.h"
#include <BinaryData.h>
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr float artWidth = 1672.0f;
constexpr float artHeight = 941.0f;
constexpr float pi = juce::MathConstants<float>::pi;
constexpr float toRad(float degrees) noexcept { return degrees * pi / 180.0f; }
const juce::Colour darkMetal{0xff131618};
const juce::Colour ivory{0xffe9d4ae};

// Hand-measured painted pointer angle (relative to 12 o'clock) in each sprite.
constexpr std::array<float, 4> originalPointer{34.0f,-137.0f,70.0f,16.0f};
constexpr std::array<std::pair<float,float>, 4> knobOrigins{{
    {278.0f,649.0f}, {630.0f,648.0f}, {1045.0f,649.0f}, {1387.0f,650.0f}
}};
float targetAngle(int n, float pos) noexcept {
    pos = juce::jlimit(0.0f, 1.0f, pos);
    if (n == 0 || n == 3) // 20-position stepped attenuators
        return -143.0f + pos * 286.0f;
    if (n == 2) { // RCA front-panel mechanical order is NOT parameter-list order.
        constexpr float mode[] = {-47.f, -94.f, 0.f, 46.f, 79.f};
        return mode[juce::jlimit(0,4,juce::roundToInt(pos*4.0f))];
    }
    constexpr float meter[] = {-137.f,-106.f,-77.f,-46.f,0.f,43.f,77.f,99.f,119.f,137.f};
    return meter[juce::jlimit(0,9,juce::roundToInt(pos*9.0f))];
}
} // namespace

void BA6AKnobLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y,
    int width, int height, float pos, float, float, juce::Slider& slider) {
    const auto id = slider.getComponentID();
    if (id.startsWith("photo")) {
        const int n = id.substring(5).getIntValue();
        if (juce::isPositiveAndBelow(n, 4) && sprites[static_cast<std::size_t>(n)].isValid()) {
            const juce::Graphics::ScopedSaveState save(g);
            const float centerX = x + width * .5f;
            const float centerY = y + height * .5f;
            // RGBA alpha must end at radius 79 px in the unscaled 206px sprite.
            // The complete numbered bezel and its cast shadow stay in panel.jpg,
            // never under this rotation transform. Regression tested with
            // radically different control positions in StaticBezelTests.py.
            g.addTransform(juce::AffineTransform::rotation(
                toRad(targetAngle(n, pos)-originalPointer[static_cast<std::size_t>(n)]),
                centerX, centerY));
            g.drawImageWithin(sprites[static_cast<std::size_t>(n)], x, y, width, height,
                              juce::RectanglePlacement::stretchToFit, false);
            return;
        }
    }
    // Internal trim-pot controls are intentionally smaller, vector-drawn.
    const float cx=x+width*.5f, cy=y+height*.5f;
    const float r=std::min(width,height)*.45f;
    auto dial=juce::Rectangle<float>(cx-r,cy-r,2*r,2*r);
    g.setColour(juce::Colours::black.withAlpha(.7f));
    g.fillEllipse(dial.translated(2.f,3.f));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff605e58),cx-r,cy-r,
                                           darkMetal,cx+r,cy+r,false));
    g.fillEllipse(dial);
    g.setColour(juce::Colour(0xffb0a084));g.drawEllipse(dial,1.2f);
    const float angle = toRad(-135.f + pos*270.f);
    g.setColour(ivory);
    g.drawLine(cx,cy,cx+std::sin(angle)*r*.78f,
               cy-std::cos(angle)*r*.78f, 2.f);
}

RCABA6AAudioProcessorEditor::RCABA6AAudioProcessorEditor(RCABA6AAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), audioProcessor(p) {
    faceplate = juce::ImageFileFormat::loadFrom(BinaryData::panel_jpg, BinaryData::panel_jpgSize);
    knobStyle.setSprites({{
        juce::ImageFileFormat::loadFrom(BinaryData::knob_input_png, BinaryData::knob_input_pngSize),
        juce::ImageFileFormat::loadFrom(BinaryData::knob_meter_png, BinaryData::knob_meter_pngSize),
        juce::ImageFileFormat::loadFrom(BinaryData::knob_mode_png, BinaryData::knob_mode_pngSize),
        juce::ImageFileFormat::loadFrom(BinaryData::knob_output_png, BinaryData::knob_output_pngSize)
    }});
    const char* mainIDs[] = {"input","meter","mode","output"};
    const char* mainNames[] = {"INPUT","METER","BAL–LIMIT","OUTPUT"};
    for (int n=0;n<4;++n) {
        auto& s = mainKnobs[static_cast<std::size_t>(n)];
        configureKnob(s, mainIDs[n],mainBindings[static_cast<std::size_t>(n)],
                      juce::String("photo")+juce::String(n));
        s.setName(mainNames[n]);
        s.setTooltip(juce::String(mainNames[n])+" — drag vertically or use mouse wheel");
    }
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
    addAndMakeVisible(powerButton);
    powerBinding = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts,"power",powerButton);
    setResizable(true,true);
    setResizeLimits(940,529,1672,941);
    getConstrainer()->setFixedAspectRatio(static_cast<double>(artWidth/artHeight));
    setSize(1254,706);
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
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
    s.setRotaryParameters(-.79f*pi,.79f*pi,true);
    s.setLookAndFeel(&knobStyle);
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
    for (auto& slider:serviceKnobs) {
        slider.setVisible(serviceOpening > .06f);
        slider.setAlpha(serviceOpening);
    }
    powerButton.setVisible(serviceOpening>.06f);
    powerButton.setAlpha(serviceOpening);
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
    drawServiceDrawer(g);
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

void RCABA6AAudioProcessorEditor::drawServiceDrawer(juce::Graphics& g) {
    if (serviceOpening <= .01f) return;
    // Overlay is deliberately limited to OPEN SERVICE mode, leaving the chosen
    // photo-real front panel pristine during ordinary mixing.
    const float alpha=juce::jlimit(0.f,1.f,serviceOpening);
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
