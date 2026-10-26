#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <memory>

// The photo-real rotary caps are independent RGBA layers extracted from the
// chosen, user-approved faceplate. Only the caps rotate; the labels remain fixed.
class BA6AKnobLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    void setSprites(const std::array<juce::Image, 4>& source) { sprites = source; }
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;
private:
    std::array<juce::Image, 4> sprites;
};

class RCABA6AAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer {
public:
    explicit RCABA6AAudioProcessorEditor(RCABA6AAudioProcessor&);
    ~RCABA6AAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    void timerCallback() override;
    void configureKnob(juce::Slider&, const char* parameter,
                       std::unique_ptr<SliderAttachment>& attachment,
                       const juce::String& componentID);
    void drawNeedle(juce::Graphics&);
    void drawServiceDrawer(juce::Graphics&);

    RCABA6AAudioProcessor& audioProcessor;
    BA6AKnobLookAndFeel knobStyle;
    juce::Image faceplate;
    std::array<juce::Slider, 4> mainKnobs;
    std::array<std::unique_ptr<SliderAttachment>, 4> mainBindings;
    std::array<juce::Slider, 6> serviceKnobs;
    std::array<std::unique_ptr<SliderAttachment>, 6> serviceBindings;
    juce::TextButton serviceButton {"SERVICE PANEL"};
    juce::TextButton powerButton {"POWER ON"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerBinding;
    float meterDisplay = 0.0f;
    float serviceOpening = 0.0f;
    bool serviceOpen = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RCABA6AAudioProcessorEditor)
};
