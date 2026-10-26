#include "DSP/BA6AEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

static void require(bool pass, const char* message) {
    if (!pass) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
    std::cout << "PASS: " << message << '\n';
}

static float measureSine(ba6a::BA6AEngine& engine, float magnitude,
                         int frames = 96000, int blockSize = 256) {
    std::vector<float> buf(static_cast<size_t>(blockSize));
    double sum = 0;
    int count = 0;
    for (int start = 0; start < frames; start += blockSize) {
        const int n = std::min(blockSize, frames - start);
        for (int i = 0; i < n; ++i)
            buf[static_cast<size_t>(i)] = magnitude * std::sin(2.0 * 3.141592653589793 *
                                               1000.0 * (start + i) / 48000.0);
        engine.process(buf.data(), nullptr, static_cast<size_t>(n));
        if (start >= frames / 2) for (int i = 0; i < n; ++i) {
            sum += buf[static_cast<size_t>(i)] * buf[static_cast<size_t>(i)]; ++count;
        }
    }
    return std::sqrt(sum / std::max(1, count));
}
int main() {
    using namespace ba6a;
    require(BA6AEngine::inputAttenuationDb(24) == 0.0f, "input +24 maximum drive");
    require(BA6AEngine::inputAttenuationDb(23) == -2.0f, "input switch 2 dB steps");
    require(BA6AEngine::inputAttenuationDb(0) == -48.0f, "input -24 minimum drive");
    require(BA6AEngine::outputAttenuationDb(14) == -2.0f, "output switch 2 dB steps");
    require(BA6AEngine::outputAttenuationDb(0) <= -100.0f, "output OFF");
    BA6AEngine engine; engine.prepare(48000.0);
    Controls c; c.inputStep = 24; c.outputStep = 15; c.mode = Mode::Single;
    engine.setControls(c);
    const float low = measureSine(engine, 0.005f);
    engine.reset(); const float high = measureSine(engine, 0.4f);
    require(low > 0 && high > low, "output rises with input");
    require(high / low < 25.0f, "strong limiting above verge");
    require(engine.gainReductionDb() > 3.0f, "gain-reduction detector is active");
    // Figure 4 in the RCA handbook indicates approximately 20 dB in -> 1.9 dB
    // out ABOVE the limiting verge. The digital reference is independently set.
    engine.reset(); const float curveLo = measureSine(engine, 0.08f, 192000);
    engine.reset(); const float curveHi = measureSine(engine, 0.8f, 192000);
    const float compressedSpanDb = gainToDb(curveHi / curveLo);
    require(compressedSpanDb > 1.3f && compressedSpanDb < 2.9f,
            "approx 20 dB -> 2 dB compressed static slope");
    engine.reset(); c.mode = Mode::Off; engine.setControls(c);
    const float off = measureSine(engine, 0.3f, 48000);
    require(off > high, "limiting OFF increases level");
    c.power = false; engine.setControls(c);
    float silence[256]; std::fill(std::begin(silence), std::end(silence), 0.4f);
    engine.process(silence, nullptr, 256);
    require(*std::max_element(std::begin(silence), std::end(silence)) == 0.0f,
            "power OFF mutes the physical amplifier");
    c.power = true; c.mode = Mode::BalanceA; engine.setControls(c);
    std::fill(std::begin(silence), std::end(silence), 0.3f);
    engine.process(silence, nullptr, 256);
    require(silence[0] == 0, "service BAL A isolates program output");
    c.mode = Mode::Dual; engine.reset(); engine.setControls(c);
    auto loud = std::vector<float>(96000);
    for (std::size_t i=0; i<loud.size(); ++i)
        loud[i] = 0.3f * std::sin(2.0 * 3.141592653589793 * 1000.0 * i / 48000.0);
    engine.process(loud.data(), nullptr, loud.size());
    const float grBefore = engine.gainReductionDb();
    auto quiet = std::vector<float>(2400, 0.0f);
    engine.process(quiet.data(), nullptr, quiet.size());
    require(engine.gainReductionDb() > 0.5f && grBefore > 0.5f,
            "DUAL remembers sustained level for at least 50 ms");
    // DUAL adds a slow RC in parallel; after sustained material, its gain
    // recovery must be materially slower than SINGLE recovery.
    auto residualAfterSustain = [](Mode mode) {
        BA6AEngine e; e.prepare(48000.0);
        Controls ctrl; ctrl.inputStep=24; ctrl.mode=mode; e.setControls(ctrl);
        std::vector<float> tone(48000*3);
        for (std::size_t i=0; i<tone.size(); ++i)
            tone[i]=0.8f * std::sin(2.0 * 3.141592653589793 * 1000.0*i / 48000.0);
        for(std::size_t offset=0; offset<tone.size(); offset+=256)
            e.process(tone.data()+offset, nullptr, std::min(std::size_t(256),tone.size()-offset));
        std::vector<float> gap(12000,0.f);
        e.process(gap.data(),nullptr,gap.size());
        return e.gainReductionDb();
    };
    const float singleRecovery = residualAfterSustain(Mode::Single);
    const float dualRecovery = residualAfterSustain(Mode::Dual);
    require(dualRecovery > singleRecovery + 5.f,
            "DUAL release is slower than SINGLE after sustained peaks");
    c.mode = Mode::Single; engine.reset(); engine.setControls(c);
    auto transient = std::vector<float>(2048, 0.1f);
    engine.process(transient.data(), nullptr, transient.size());
    require(std::all_of(transient.begin(), transient.end(),
               [](float x){ return std::isfinite(x); }), "no NaN or Inf");
    engine.prepare(96000.0); engine.setControls(c);
    std::vector<float> l(300, 0.15f), r(300, -0.25f);
    engine.process(l.data(), r.data(), 300);
    require(std::all_of(r.begin(), r.end(), [](float x){return std::isfinite(x);}),
            "stereo linked path finite at 96k");
    std::cout << "All BA-6A DSP tests passed.\n";
}
