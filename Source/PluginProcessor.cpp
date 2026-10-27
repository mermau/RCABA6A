#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <map>

namespace {
juce::StringArray inputChoices() {
    juce::StringArray s{"OFF"};
    for (int i = 1; i <= 20; ++i) {
        const int dbm = ba6a::hw::inputLabelDbm(i);
        s.add(dbm > 0 ? "+" + juce::String(dbm) : juce::String(dbm));
    }
    return s;
}
juce::StringArray outputChoices() {
    juce::StringArray s{"OFF"};
    for (int i = 1; i <= 20; ++i) s.add(juce::String(ba6a::hw::outputLabelDbm(i)));
    return s;
}
const juce::StringArray balLimitChoices{"A", "B", "OFF", "SINGLE", "DUAL"};
const juce::StringArray meterChoices{"OFF", "GR", "V1", "V2", "V3", "V4", "V5", "V6",
                                     "FIL", "COARSE BAL", "FINE BAL"};
} // namespace

RCABA6AAudioProcessor::RCABA6AAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "BA6A_STATE", createParameters()) {
    apvts.state.setProperty("controlLayoutVersion", stateVersion, nullptr);
    using namespace ba6a::ids;
    params.input = apvts.getRawParameterValue(input);
    params.output = apvts.getRawParameterValue(output);
    params.balLimit = apvts.getRawParameterValue(balLimit);
    params.meter = apvts.getRawParameterValue(meter);
    params.power = apvts.getRawParameterValue(power);
    params.balanceA = apvts.getRawParameterValue(balanceA);
    params.balanceB = apvts.getRawParameterValue(balanceB);
    params.zero = apvts.getRawParameterValue(zero);
    params.fineOutput = apvts.getRawParameterValue(fineOutput);
    params.heater = apvts.getRawParameterValue(heater);
    params.stereoLink = apvts.getRawParameterValue(stereoLink);
    params.noise = apvts.getRawParameterValue(noise);
    params.mains = apvts.getRawParameterValue(mains);
}

juce::AudioProcessorValueTreeState::ParameterLayout RCABA6AAudioProcessor::createParameters() {
    using PC = juce::AudioParameterChoice;
    using PF = juce::AudioParameterFloat;
    using PB = juce::AudioParameterBool;
    using PID = juce::ParameterID;
    using namespace ba6a;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    const auto trim = juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f);
    p.push_back(std::make_unique<PC>(PID{ids::input, 2}, "Input (dBm at verge)", inputChoices(), hw::inputDefaultIndex));
    p.push_back(std::make_unique<PC>(PID{ids::output, 2}, "Output (dBm at verge)", outputChoices(), hw::outputDefaultIndex));
    p.push_back(std::make_unique<PC>(PID{ids::balLimit, 2}, "Bal-Limit", balLimitChoices, hw::balLimitDefaultIndex));
    p.push_back(std::make_unique<PC>(PID{ids::meter, 2}, "Meter switch", meterChoices, hw::meterDefaultIndex));
    p.push_back(std::make_unique<PB>(PID{ids::power, 1}, "Power", true));
    p.push_back(std::make_unique<PF>(PID{ids::balanceA, 1}, "Balance A (cathode)", trim, 0.0f));
    p.push_back(std::make_unique<PF>(PID{ids::balanceB, 1}, "Balance B (screen)", trim, 0.0f));
    p.push_back(std::make_unique<PF>(PID{ids::zero, 1}, "Zero adj. (meter)", trim, 0.0f));
    p.push_back(std::make_unique<PF>(PID{ids::fineOutput, 2}, "Fine output (R35)",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    p.push_back(std::make_unique<PF>(PID{ids::hum, 1}, "Hum adj. (preamp supply)", trim, 0.0f));
    p.push_back(std::make_unique<PF>(PID{ids::heater, 1}, "Heater voltage adj.", trim, 0.0f));
    p.push_back(std::make_unique<PB>(PID{ids::stereoLink, 2}, "Stereo link", true));
    p.push_back(std::make_unique<PB>(PID{ids::noise, 2}, "Residual noise and hum", true));
    p.push_back(std::make_unique<PC>(PID{ids::mains, 3}, "Mains frequency", juce::StringArray{"60 Hz", "50 Hz"}, 0));
    return {p.begin(), p.end()};
}

void RCABA6AAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    preparedBlock = std::max(1, samplesPerBlock);
    // The engine needs >= 88.2 kHz: 2x oversampling at 44.1/48 kHz.
    const size_t factorLog2 = sampleRate < 80000.0 ? 1u : 0u;
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        2, factorLog2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampler->initProcessing(static_cast<size_t>(preparedBlock));
    engine.prepare(sampleRate * static_cast<double>(1u << factorLog2));
    readControls();
    engine.setControls(controls);
    engine.reset();
    const int latency = juce::roundToInt(oversampler->getLatencyInSamples());
    setLatencySamples(latency);
    bypassDelay.setSize(2, std::max(1, latency));
    bypassDelay.clear();
    bypassWrite = 0;
}

void RCABA6AAudioProcessor::releaseResources() {
    if (oversampler) oversampler->reset();
}

bool RCABA6AAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
        && layouts.getMainInputChannelSet() == out;
}

void RCABA6AAudioProcessor::readControls() noexcept {
    using namespace ba6a;
    auto idx = [](std::atomic<float>* p, int hi) {
        return juce::jlimit(0, hi, juce::roundToInt(p->load(std::memory_order_relaxed)));
    };
    auto val = [](std::atomic<float>* p) { return p->load(std::memory_order_relaxed); };
    controls.inputIndex = idx(params.input, 20);
    controls.outputIndex = idx(params.output, 20);
    controls.balLimit = static_cast<hw::BalLimit>(idx(params.balLimit, 4));
    controls.meter = static_cast<hw::MeterSwitch>(idx(params.meter, 10));
    controls.power = val(params.power) > 0.5f;
    controls.balanceA = val(params.balanceA);
    controls.balanceB = val(params.balanceB);
    controls.zeroAdj = val(params.zero);
    controls.fineOutput = val(params.fineOutput);
    controls.heaterAdj = val(params.heater);
    controls.stereoLink = val(params.stereoLink) > 0.5f;
    controls.residualNoise = val(params.noise) > 0.5f;
    controls.mains50Hz = idx(params.mains, 1) == 1;
}

void RCABA6AAudioProcessor::runEngine(juce::AudioBuffer<float>& audio, int start, int count) noexcept {
    const int channels = std::min(2, audio.getNumChannels());
    juce::dsp::AudioBlock<float> block(audio.getArrayOfWritePointers(),
                                       static_cast<size_t>(channels),
                                       static_cast<size_t>(start), static_cast<size_t>(count));
    auto up = oversampler->processSamplesUp(block);
    engine.process(up.getChannelPointer(0), channels > 1 ? up.getChannelPointer(1) : nullptr,
                   up.getNumSamples());
    oversampler->processSamplesDown(block);
}

void RCABA6AAudioProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int inputs = getTotalNumInputChannels();
    for (int c = inputs; c < audio.getNumChannels(); ++c) audio.clear(c, 0, audio.getNumSamples());
    if (audio.getNumChannels() == 0 || oversampler == nullptr) return;

    readControls();
    engine.setControls(controls);
    for (int start = 0; start < audio.getNumSamples(); start += preparedBlock)
        runEngine(audio, start, std::min(preparedBlock, audio.getNumSamples() - start));

    meterNeedle.store(engine.meterTarget(), std::memory_order_relaxed);
    meterGainReduction.store(engine.gainReductionDb(), std::memory_order_relaxed);
}

void RCABA6AAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) {
    // Keep the dry signal time-aligned with the oversampling latency.
    const int latency = getLatencySamples();
    if (latency <= 0) return;
    const int channels = std::min(audio.getNumChannels(), bypassDelay.getNumChannels());
    for (int i = 0; i < audio.getNumSamples(); ++i) {
        for (int c = 0; c < channels; ++c) {
            float* line = bypassDelay.getWritePointer(c);
            const float delayed = line[bypassWrite];
            line[bypassWrite] = audio.getSample(c, i);
            audio.setSample(c, i, delayed);
        }
        bypassWrite = (bypassWrite + 1) % latency;
    }
    meterNeedle.store(engine.meterTarget(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* RCABA6AAudioProcessor::createEditor() {
    return new RCABA6AAudioProcessorEditor(*this);
}

void RCABA6AAudioProcessor::getStateInformation(juce::MemoryBlock& destination) {
    auto state = apvts.copyState();
    state.setProperty("controlLayoutVersion", stateVersion, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}

void RCABA6AAudioProcessor::setStateInformation(const void* data, int bytes) {
    const auto xml = getXmlFromBinary(data, bytes);
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType())) return;
    apvts.replaceState(migrateState(juce::ValueTree::fromXml(*xml)));
}

juce::ValueTree RCABA6AAudioProcessor::migrateState(const juce::ValueTree& old) {
    using namespace ba6a;
    const int version = static_cast<int>(old.getProperty("controlLayoutVersion", 1));
    std::map<juce::String, float> v;
    for (int i = 0; i < old.getNumChildren(); ++i) {
        const auto child = old.getChild(i);
        if (child.hasProperty("id") && child.hasProperty("value"))
            v[child.getProperty("id").toString()] = static_cast<float>(child.getProperty("value"));
    }
    if (version >= stateVersion) {
        auto copy = old.createCopy();
        copy.setProperty("controlLayoutVersion", stateVersion, nullptr);
        return copy;
    }
    auto has = [&](const char* id) { return v.find(id) != v.end(); };
    auto get = [&](const char* id, float fallback) { return has(id) ? v[id] : fallback; };

    // ---- v1/v2 -> v3 (layouts of the earlier ChatGPT builds) ----
    if (version == 1) {
        if (has("input"))  v["input"] = static_cast<float>(-24 + 2 * juce::roundToInt(juce::jlimit(0.0f, 20.0f, v["input"]) * 24.0f / 20.0f));
        if (has("output")) v["output"] = static_cast<float>(juce::jlimit(0, 30, 2 * juce::roundToInt((juce::jlimit(0.0f, 20.0f, v["output"]) + 10.0f) * 0.5f)));
        if (has("mode") && v["mode"] < 1.5f) v["mode"] = 1.0f - juce::roundToInt(v["mode"]);
    }
    if (version < 3 && has("meter")) {
        constexpr int oldToV3[]{1, 2, 3, 4, 5, 6, 7, 9, 9, 8};
        v["meter"] = static_cast<float>(oldToV3[juce::jlimit(0, 9, juce::roundToInt(v["meter"]))]);
    }

    // ---- v3 -> v4: hardware-correct scales ----
    // v3 INPUT marking L had attenuation (24 - L) dB; v4 legend V = attenuation - 24.
    const int oldIn = juce::roundToInt(get("input", 0.0f));
    const int verge = juce::jlimit(-24, 14, -oldIn);
    const int inputIndex = juce::jlimit(1, 20, (16 - verge + 1) / 2);
    // v3 OUTPUT marking had attenuation (30 - marking) dB, 0 = OFF; same in v4.
    const int oldOut = juce::roundToInt(get("output", 0.0f));
    const int outputIndex = oldOut <= 0 ? 0 : juce::jlimit(11, 30, oldOut) - 10;
    // v3 BAL-LIMIT order B, A, OFF, SINGLE, DUAL -> A, B, OFF, SINGLE, DUAL.
    int mode = juce::jlimit(0, 4, juce::roundToInt(get("mode", 2.0f)));
    if (mode < 2) mode = 1 - mode;
    // v3 METER: IN, V1..V6, FIL, COMPR, BAL -> OFF, GR, V1..V6, FIL, COARSE BAL, FINE BAL.
    constexpr int meterV3ToV4[]{1, 2, 3, 4, 5, 6, 7, 8, 1, 9};
    const int meterIndex = meterV3ToV4[juce::jlimit(0, 9, juce::roundToInt(get("meter", 0.0f)))];
    // v3 fine output was +-1 dB; v4 is the R35 rheostat (0 .. -0.695 dB).
    const float fine = juce::jlimit(0.0f, 1.0f, -get("fineOutput", 0.0f) / -hw::fineOutputMaxLossDb);

    juce::ValueTree state("BA6A_STATE");
    state.setProperty("controlLayoutVersion", stateVersion, nullptr);
    auto add = [&state](const char* id, float value) {
        juce::ValueTree p("PARAM");
        p.setProperty("id", id, nullptr);
        p.setProperty("value", value, nullptr);
        state.appendChild(p, nullptr);
    };
    add(ids::input, static_cast<float>(inputIndex));
    add(ids::output, static_cast<float>(outputIndex));
    add(ids::balLimit, static_cast<float>(mode));
    add(ids::meter, static_cast<float>(meterIndex));
    add(ids::power, get("power", 1.0f) > 0.5f ? 1.0f : 0.0f);
    add(ids::balanceA, juce::jlimit(-1.0f, 1.0f, get("balanceA", 0.0f)));
    add(ids::balanceB, juce::jlimit(-1.0f, 1.0f, get("balanceB", 0.0f)));
    add(ids::zero, juce::jlimit(-1.0f, 1.0f, get("zero", 0.0f)));
    add(ids::fineOutput, fine);
    add(ids::hum, juce::jlimit(-1.0f, 1.0f, get("hum", 0.0f)));
    add(ids::heater, juce::jlimit(-1.0f, 1.0f, get("heater", 0.0f)));
    add(ids::stereoLink, 1.0f);
    add(ids::noise, 1.0f);
    add(ids::mains, 0.0f);
    return state;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new RCABA6AAudioProcessor();
}
