#pragma once
#include <JuceHeader.h>
#include "DSP/BA6AEngine.h"
#include <atomic>
#include <memory>

namespace ba6a::ids {
// Parameter IDs. v0.9 replaced the v0.8 IDs "input", "output", "mode",
// "meter" and "fineOutput" because their ranges did not match the hardware;
// old sessions are migrated in setStateInformation().
inline constexpr const char* input = "inputLevel";
inline constexpr const char* output = "outputLevel";
inline constexpr const char* balLimit = "balLimit";
inline constexpr const char* meter = "meterSwitch";
inline constexpr const char* power = "power";
inline constexpr const char* balanceA = "balanceA";
inline constexpr const char* balanceB = "balanceB";
inline constexpr const char* zero = "zero";
inline constexpr const char* fineOutput = "fineOut";
inline constexpr const char* hum = "hum";
inline constexpr const char* heater = "heater";
inline constexpr const char* stereoLink = "stereoLink";
inline constexpr const char* noise = "residualNoise";
inline constexpr const char* mains = "mains";
}

class RCABA6AAudioProcessor final : public juce::AudioProcessor {
public:
    static constexpr int stateVersion = 4;

    RCABA6AAudioProcessor();
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;
    ~RCABA6AAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // Converts any earlier layout (v1..v3) into the current parameter tree.
    static juce::ValueTree migrateState(const juce::ValueTree& oldState);
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> meterNeedle {1.0f};       // 0 = rest, 1 = full scale
    std::atomic<float> meterGainReduction {0.0f};

private:
    void readControls() noexcept;
    void runEngine(juce::AudioBuffer<float>&, int start, int count) noexcept;

    ba6a::BA6AEngine engine;
    ba6a::Controls controls;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int preparedBlock = 0;
    juce::AudioBuffer<float> bypassDelay;
    int bypassWrite = 0;

    struct Params {
        std::atomic<float>* input = nullptr;  std::atomic<float>* output = nullptr;
        std::atomic<float>* balLimit = nullptr; std::atomic<float>* meter = nullptr;
        std::atomic<float>* power = nullptr;  std::atomic<float>* balanceA = nullptr;
        std::atomic<float>* balanceB = nullptr; std::atomic<float>* zero = nullptr;
        std::atomic<float>* fineOutput = nullptr; std::atomic<float>* heater = nullptr;
        std::atomic<float>* stereoLink = nullptr; std::atomic<float>* noise = nullptr;
        std::atomic<float>* mains = nullptr;
    } params;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RCABA6AAudioProcessor)
};
