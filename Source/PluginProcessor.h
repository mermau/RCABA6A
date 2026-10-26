#pragma once
#include <JuceHeader.h>
#include "DSP/BA6AEngine.h"
#include <atomic>

class RCABA6AAudioProcessor final : public juce::AudioProcessor {
public:
    RCABA6AAudioProcessor();
    ~RCABA6AAudioProcessor() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> meterGainReduction {0.0f};
    std::atomic<float> meterPeak {0.0f};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    ba6a::BA6AEngine engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RCABA6AAudioProcessor)
};
