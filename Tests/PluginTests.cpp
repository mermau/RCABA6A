// JUCE-level integration tests: processor I/O and latency, parameter layout,
// migration of sessions saved by v0.8.x, editor construction/resizing and
// GUI snapshots (written to the directory given as argv[1], if any).
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>

static int failures = 0;
static void check(bool ok, const juce::String& what) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    if (!ok) ++failures;
}

static int choiceIndex(RCABA6AAudioProcessor& p, const char* id) {
    return juce::roundToInt(p.apvts.getRawParameterValue(id)->load());
}

static double runSine(RCABA6AAudioProcessor& p, double sr, float dbfs, int channels, int blocks = 200) {
    juce::AudioBuffer<float> buf(channels, 480);
    juce::MidiBuffer midi;
    const float amp = juce::Decibels::decibelsToGain(dbfs);
    double peak = 0, phase = 0;
    for (int b = 0; b < blocks; ++b) {
        for (int i = 0; i < buf.getNumSamples(); ++i) {
            const float v = amp * static_cast<float>(std::sin(phase));
            phase += 2.0 * juce::MathConstants<double>::pi * 1000.0 / sr;
            for (int c = 0; c < channels; ++c) buf.setSample(c, i, v);
        }
        p.processBlock(buf, midi);
        if (b > blocks / 2) peak = std::max(peak, static_cast<double>(buf.getMagnitude(0, 0, buf.getNumSamples())));
    }
    return juce::Decibels::gainToDecibels(peak);
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI juce;
    const juce::File outDir = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]) : juce::File();

    // ---- processor -------------------------------------------------------
    {
        RCABA6AAudioProcessor p;
        check(choiceIndex(p, ba6a::ids::input) == 8, "INPUT default 0 dBm");
        check(choiceIndex(p, ba6a::ids::output) == 8, "OUTPUT default 18 dBm (not OFF)");
        check(choiceIndex(p, ba6a::ids::balLimit) == 2, "BAL-LIMIT default OFF");
        check(choiceIndex(p, ba6a::ids::meter) == 1, "METER default GR");
        auto* in = dynamic_cast<juce::AudioParameterChoice*>(p.apvts.getParameter(ba6a::ids::input));
        auto* out = dynamic_cast<juce::AudioParameterChoice*>(p.apvts.getParameter(ba6a::ids::output));
        check(in != nullptr && in->choices.size() == 21 && in->choices[1] == "+14" && in->choices[20] == "-24",
              "INPUT choices OFF, +14 .. -24");
        check(out != nullptr && out->choices.size() == 21 && out->choices[1] == "11" && out->choices[20] == "30",
              "OUTPUT choices OFF, 11 .. 30");

        p.setPlayConfigDetails(2, 2, 48000.0, 480);
        p.prepareToPlay(48000.0, 480);
        check(p.getLatencySamples() > 0, "2x oversampling latency reported at 48 kHz: " + juce::String(p.getLatencySamples()));
        // INPUT 0 dBm (verge at -18 dBFS), OUTPUT 18: a signal 6 dB below verge
        // comes out 6 dB below the verge output (-18.15 dBFS).
        const double below = runSine(p, 48000.0, -24.0f, 2);
        check(std::abs(below - (-24.15)) < 0.4, "unity gain below verge at default settings: " + juce::String(below, 2) + " dBFS");
        p.apvts.getParameter(ba6a::ids::balLimit)->setValueNotifyingHost(p.apvts.getParameter(ba6a::ids::balLimit)->convertTo0to1(3.0f));
        const double limited = runSine(p, 48000.0, -2.0f, 2, 400);
        check(limited > -18.5 && limited < -15.5, "SINGLE: +16 dB over verge limited to about verge +1.5 dB: " + juce::String(limited, 2) + " dBFS");
        check(p.meterGainReduction.load() > 13.0f, "GR reported to the editor: " + juce::String(p.meterGainReduction.load(), 1) + " dB");
        p.releaseResources();
        p.setPlayConfigDetails(1, 1, 96000.0, 512);
        p.prepareToPlay(96000.0, 512);
        check(p.getLatencySamples() == 0, "no oversampling at 96 kHz");
        const double mono = runSine(p, 96000.0, -30.0f, 1);
        check(std::isfinite(mono) && mono < -10.0, "mono layout processes: " + juce::String(mono, 2) + " dBFS");
        // Host block larger than announced.
        juce::AudioBuffer<float> big(1, 4096); big.clear(); juce::MidiBuffer midi;
        p.processBlock(big, midi);
        check(std::isfinite(big.getMagnitude(0, 0, 4096)), "oversized host block handled");
    }

    // ---- state ------------------------------------------------------------
    {
        // Session saved by v0.8.1 (layout 3): input +24 (max drive), output 20,
        // mode index 0 = "B" in the old order, meter "COMPR", fine -0.5 dB.
        juce::ValueTree old("BA6A_STATE");
        old.setProperty("controlLayoutVersion", 3, nullptr);
        auto add = [&old](const char* id, float v) {
            juce::ValueTree c("PARAM"); c.setProperty("id", id, nullptr); c.setProperty("value", v, nullptr);
            old.appendChild(c, nullptr);
        };
        add("input", 24.0f); add("output", 20.0f); add("mode", 0.0f); add("meter", 8.0f);
        add("power", 1.0f); add("balanceA", 0.25f); add("fineOutput", -0.5f); add("heater", -0.2f);
        juce::MemoryBlock mb;
        juce::AudioProcessor::copyXmlToBinary(*old.createXml(), mb);
        RCABA6AAudioProcessor p;
        p.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
        check(choiceIndex(p, ba6a::ids::input) == 20, "migrated input +24 (max drive) -> -24 dBm contact");
        check(choiceIndex(p, ba6a::ids::output) == 10, "migrated output 20 -> 20 dBm contact");
        check(choiceIndex(p, ba6a::ids::balLimit) == 1, "migrated BAL-LIMIT old 'B' -> B");
        check(choiceIndex(p, ba6a::ids::meter) == 1, "migrated meter 'COMPR' -> GR");
        check(std::abs(p.apvts.getRawParameterValue(ba6a::ids::balanceA)->load() - 0.25f) < 1e-4f, "migrated balance A");
        check(std::abs(p.apvts.getRawParameterValue(ba6a::ids::fineOutput)->load() - 0.719f) < 0.01f, "migrated fine output");
        check(std::abs(p.apvts.getRawParameterValue(ba6a::ids::heater)->load() + 0.2f) < 1e-4f, "migrated heater");
        // Round trip in the current layout.
        p.apvts.getParameter(ba6a::ids::output)->setValueNotifyingHost(p.apvts.getParameter(ba6a::ids::output)->convertTo0to1(3.0f));
        juce::MemoryBlock saved; p.getStateInformation(saved);
        RCABA6AAudioProcessor q; q.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        check(choiceIndex(q, ba6a::ids::output) == 3 && choiceIndex(q, ba6a::ids::input) == 20, "state round trip");
        // Garbage must not crash or change anything.
        const char junk[] = "not a plugin state";
        q.setStateInformation(junk, sizeof junk);
        check(choiceIndex(q, ba6a::ids::output) == 3, "invalid state ignored");
        // Very old layout 1 (0..20 contact indices).
        juce::ValueTree v1("BA6A_STATE");
        juce::ValueTree c1("PARAM"); c1.setProperty("id", "output", nullptr); c1.setProperty("value", 0.0f, nullptr); v1.appendChild(c1, nullptr);
        juce::MemoryBlock m1; juce::AudioProcessor::copyXmlToBinary(*v1.createXml(), m1);
        RCABA6AAudioProcessor r; r.setStateInformation(m1.getData(), static_cast<int>(m1.getSize()));
        check(choiceIndex(r, ba6a::ids::output) >= 0, "layout 1 state loads");
    }

    // ---- editor -------------------------------------------------------------
    {
        RCABA6AAudioProcessor p;
        p.setPlayConfigDetails(2, 2, 48000.0, 480);
        p.prepareToPlay(48000.0, 480);
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        auto* e = dynamic_cast<RCABA6AAudioProcessorEditor*>(ed.get());
        check(e != nullptr, "editor created");
        e->setSize(1448, 384);
        check(std::abs(e->getWidth() / static_cast<double>(e->getHeight()) - 1448.0 / 384.0) < 0.01, "panel aspect ratio");
        auto snap = [&](const juce::String& name) {
            for (int i = 0; i < 120; ++i) e->advanceMeter(1.0f / 60.0f);
            const auto img = e->createComponentSnapshot(e->getLocalBounds(), true, 1.0f);
            check(img.isValid() && img.getWidth() == e->getWidth(), "snapshot " + name);
            if (outDir != juce::File()) {
                outDir.createDirectory();
                juce::FileOutputStream os(outDir.getChildFile(name + ".png"));
                if (os.openedOk()) { os.setPosition(0); os.truncate(); juce::PNGImageFormat().writeImageToStream(img, os); }
            }
            return img;
        };
        snap("gui_default");
        // A busy state: INPUT -12, METER GR, BAL-LIMIT DUAL, OUTPUT 26, limiting a loud tone.
        auto set = [&p](const char* id, float v) { auto* q = p.apvts.getParameter(id); q->setValueNotifyingHost(q->convertTo0to1(v)); };
        set(ba6a::ids::input, 14.0f); set(ba6a::ids::balLimit, 4.0f); set(ba6a::ids::output, 16.0f);
        runSine(p, 48000.0, -6.0f, 2, 300);
        snap("gui_limiting");
        set(ba6a::ids::meter, 9.0f); set(ba6a::ids::balLimit, 0.0f); set(ba6a::ids::input, 0.0f);
        runSine(p, 48000.0, -60.0f, 2, 100);
        e->setChassisOpen(true);
        check(e->getHeight() > juce::roundToInt(1448.0 * 384.0 / 1448.0) + 100, "chassis drawer enlarges the editor");
        snap("gui_chassis_balancing");
        set(ba6a::ids::stereoLink, 0.0f); set(ba6a::ids::mains, 1.0f);
        snap("gui_chassis_levers");
        e->setChassisOpen(false);
        set(ba6a::ids::power, 0.0f);
        runSine(p, 48000.0, -60.0f, 2, 100);
        snap("gui_power_off");
        e->setSize(900, 239);
        snap("gui_small");
    }

    std::printf(failures ? "\n%d FAILURE(S)\n" : "\nALL PLUGIN TESTS PASSED\n", failures);
    return failures ? 1 : 0;
}
