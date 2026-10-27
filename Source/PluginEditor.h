#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <memory>

namespace ba6a::gui {
// Artwork coordinate system: the approved photographic panel is 1448 x 384.
constexpr float artWidth = 1448.0f;
constexpr float panelHeight = 384.0f;
constexpr float drawerHeight = 200.0f;
constexpr float knobLegendRadius = 134.0f;       // knob component half size
constexpr std::array<juce::Point<float>, 4> knobCentres{{
    {208.0f, 226.0f}, {462.0f, 226.0f}, {975.0f, 226.0f}, {1228.0f, 226.0f}}};

struct Fonts {
    juce::Typeface::Ptr semi, medium;
    juce::Font semiBold(float h) const;
    juce::Font regular(float h) const;
};
}

// A stepped switch (INPUT, METER, BAL-LIMIT, OUTPUT). The knob turns through
// pre-rendered frames whose lighting is fixed; clicking a legend selects it.
class DetentKnob final : public juce::Component {
public:
    DetentKnob(juce::RangedAudioParameter&, int knobType, const juce::Image& strip);
    void paint(juce::Graphics&) override;
    bool hitTest(int x, int y) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    int getIndex() const noexcept { return index; }

private:
    float artScale() const noexcept { return static_cast<float>(getWidth()) / (2.0f * ba6a::gui::knobLegendRadius); }
    juce::Point<float> toArt(juce::Point<float> local) const noexcept;
    void setIndexFromUser(int newIndex);
    int legendAt(juce::Point<float> local) const noexcept;

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    const int type, positions;
    const juce::Image& strip;
    int index = 0, dragStart = 0;
    float wheelAccumulator = 0.0f;
    bool dragging = false;
};

// Screwdriver-adjusted chassis potentiometer (BALANCE A/B, ZERO, FINE OUTPUT ...).
class TrimPot final : public juce::Component, public juce::SettableTooltipClient {
public:
    TrimPot(juce::RangedAudioParameter&, juce::String label);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    const juce::String label;

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    float normalised = 0.5f, dragStartValue = 0.5f;
};

// Bat-handle toggle switch seen from the front: hex nut, threaded bushing,
// one continuous chrome lever (pivot collar, tapered shaft, rounded bat end).
// Parameter value 1 = lever up.
class ToggleLever final : public juce::Component, public juce::SettableTooltipClient {
public:
    ToggleLever(juce::RangedAudioParameter&, juce::String upText, juce::String downText,
                const ba6a::gui::Fonts&);
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    static constexpr float artW = 70.0f, artH = 104.0f;

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    juce::String upText, downText;
    const ba6a::gui::Fonts& fonts;
    bool up = true;
};

// Small plate on the panel that opens the chassis drawer.
class PanelButton final : public juce::Button {
public:
    PanelButton(const ba6a::gui::Fonts& f) : juce::Button("CHASSIS"), fonts(f) {}
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;
private:
    const ba6a::gui::Fonts& fonts;
};

// The chassis top, reachable in the hardware by opening the hinged panel.
class ChassisDrawer final : public juce::Component {
public:
    ChassisDrawer(juce::AudioProcessorValueTreeState&, const ba6a::gui::Fonts&);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    const ba6a::gui::Fonts& fonts;
    std::array<std::unique_ptr<TrimPot>, 6> trims;
    std::unique_ptr<ToggleLever> power, link, noise, mains;
};

class RCABA6AAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer {
public:
    explicit RCABA6AAudioProcessorEditor(RCABA6AAudioProcessor&);
    ~RCABA6AAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    // Exposed for the headless GUI test.
    void setChassisOpen(bool open);
    void advanceMeter(float seconds);

private:
    void timerCallback() override;
    void applyAspect(int width);
    void drawLegends(juce::Graphics&) const;
    void drawMeter(juce::Graphics&) const;
    void drawTitle(juce::Graphics&) const;
    juce::Rectangle<int> meterAreaOnScreen() const;

    RCABA6AAudioProcessor& audioProcessor;
    ba6a::gui::Fonts fonts;
    juce::Image panel, strip;
    std::array<std::unique_ptr<DetentKnob>, 4> knobs;
    std::unique_ptr<ChassisDrawer> drawer;
    PanelButton chassisButton {fonts};
    juce::TooltipWindow tooltips {this, 600};
    float needle = 1.0f, needleVelocity = 0.0f, lamp = 1.0f;
    bool chassisOpen = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RCABA6AAudioProcessorEditor)
};
