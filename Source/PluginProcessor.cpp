#include "PluginProcessor.h"
#include "PluginEditor.h"

RCABA6AAudioProcessor::RCABA6AAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "BA6A_STATE", createParameters()) {}

juce::AudioProcessorValueTreeState::ParameterLayout RCABA6AAudioProcessor::createParameters() {
    using PI = juce::AudioParameterInt;
    using PF = juce::AudioParameterFloat;
    using PB = juce::AudioParameterBool;
    using PC = juce::AudioParameterChoice;
    using PID = juce::ParameterID;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<PI>(PID{"input", 1}, "Input attenuator", 0, 20, 13));
    p.push_back(std::make_unique<PI>(PID{"output", 1}, "Output attenuator", 0, 20, 20));
    p.push_back(std::make_unique<PC>(PID{"mode", 1}, "BAL-LIMIT",
        juce::StringArray{"A", "B", "OFF", "SINGLE", "DUAL"}, 3));
    p.push_back(std::make_unique<PC>(PID{"meter", 1}, "Meter selection",
        juce::StringArray{"V1", "V2", "V3", "V4", "V5", "V6", "FIL", "COARSE BAL", "FINE BAL", "GR"}, 9));
    p.push_back(std::make_unique<PB>(PID{"power", 1}, "Power", true));
    p.push_back(std::make_unique<PF>(PID{"balanceA", 1}, "Balance A",
        juce::NormalisableRange<float>(-1.f, 1.f), 0.f));
    p.push_back(std::make_unique<PF>(PID{"balanceB", 1}, "Balance B",
        juce::NormalisableRange<float>(-1.f, 1.f), 0.f));
    p.push_back(std::make_unique<PF>(PID{"zero", 1}, "Meter zero",
        juce::NormalisableRange<float>(-1.f, 1.f), 0.f));
    p.push_back(std::make_unique<PF>(PID{"fineOutput", 1}, "Fine output",
        juce::NormalisableRange<float>(-1.f, 1.f), 0.f));
    p.push_back(std::make_unique<PF>(PID{"hum", 1}, "Hum balance",
        juce::NormalisableRange<float>(-1.f, 1.f), 0.f));
    p.push_back(std::make_unique<PF>(PID{"heater", 1}, "Heater voltage adjustment",
        juce::NormalisableRange<float>(-1.f, 1.f), 0.f));
    return {p.begin(), p.end()};
}

void RCABA6AAudioProcessor::prepareToPlay(double sr, int) { engine.prepare(sr); }
void RCABA6AAudioProcessor::releaseResources() {}
bool RCABA6AAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
        && layouts.getMainInputChannelSet() == out;
}

void RCABA6AAudioProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    auto get = [this](const char* name) noexcept {
        return apvts.getRawParameterValue(name)->load(std::memory_order_relaxed);
    };
    ba6a::Controls c;
    c.inputStep = juce::roundToInt(get("input"));
    c.outputStep = juce::roundToInt(get("output"));
    c.mode = static_cast<ba6a::Mode>(juce::roundToInt(get("mode")));
    c.power = get("power") > 0.5f;
    c.balanceA = get("balanceA"); c.balanceB = get("balanceB");
    c.fineOutputDb = get("fineOutput");
    c.heater = get("heater"); c.hum = get("hum");
    engine.setControls(c);

    for (int channel = 2; channel < audio.getNumChannels(); ++channel)
        audio.clear(channel, 0, audio.getNumSamples());
    if (audio.getNumChannels() == 0) return;
    engine.process(audio.getWritePointer(0),
                   audio.getNumChannels() > 1 ? audio.getWritePointer(1) : nullptr,
                   static_cast<std::size_t>(audio.getNumSamples()));
    meterGainReduction.store(engine.gainReductionDb(), std::memory_order_relaxed);
    meterPeak.store(engine.lastPeak(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* RCABA6AAudioProcessor::createEditor() {
    return new RCABA6AAudioProcessorEditor(*this);
}
void RCABA6AAudioProcessor::getStateInformation(juce::MemoryBlock& destination) {
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destination);
}
void RCABA6AAudioProcessor::setStateInformation(const void* data, int bytes) {
    if (auto xml = getXmlFromBinary(data, bytes)) {
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new RCABA6AAudioProcessor();
}
