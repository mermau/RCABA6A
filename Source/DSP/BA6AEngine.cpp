#include "BA6AEngine.h"

namespace ba6a {

float BA6AEngine::smoothCoefficient(float seconds, double sr) noexcept {
    return std::exp(-1.0f / (std::max(0.000001f, seconds) * static_cast<float>(sr)));
}

float BA6AEngine::inputAttenuationDb(int step) noexcept {
    if (step <= 0) return -120.0f;
    return -2.0f * static_cast<float>(20 - std::clamp(step, 1, 20));
}
float BA6AEngine::outputAttenuationDb(int step) noexcept {
    return -static_cast<float>(20 - std::clamp(step, 0, 20));
}

void BA6AEngine::prepare(double newSampleRate) noexcept {
    sampleRate = std::max(8000.0, newSampleRate);
    // RCA manual p. 5-8: SINGLE 600 us / 330 ms;
    // DUAL adds 900 ms / 2 s memory in parallel with the single RC filter.
    fastAttack = smoothCoefficient(0.0006f, sampleRate);
    fastRelease = smoothCoefficient(0.33f, sampleRate);
    slowAttack = smoothCoefficient(0.9f, sampleRate);
    slowRelease = smoothCoefficient(2.0f, sampleRate);
    controlSmoothAttack = smoothCoefficient(0.0015f, sampleRate);
    controlSmoothRelease = smoothCoefficient(0.007f, sampleRate);
    hpCoeff = std::exp(-2.0f * kPi * 17.0f / static_cast<float>(sampleRate));
    lpCoeff = 1.0f - std::exp(-2.0f * kPi * 19000.0f / static_cast<float>(sampleRate));
    reset();
}

void BA6AEngine::reset() noexcept {
    fastEnvelope = slowEnvelope = currentReduction = visibleReduction = visiblePeak = 0.0f;
    highPass = {}; lowPass = {};
}

void BA6AEngine::setControls(const Controls& incoming) noexcept {
    controls = incoming;
    controls.inputStep = std::clamp(controls.inputStep, 0, 20);
    controls.outputStep = std::clamp(controls.outputStep, 0, 20);
    padGain = dbToGain(inputAttenuationDb(controls.inputStep));
    if (controls.inputStep == 0) padGain = 0.0f;
    outGain = dbToGain(outputAttenuationDb(controls.outputStep) +
                       std::clamp(controls.fineOutputDb, -1.0f, 1.0f));
}

float BA6AEngine::hp(float x, HPState& s) noexcept {
    const float y = hpCoeff * (s.previousY + x - s.previousX);
    s.previousX = x;
    s.previousY = y;
    return y;
}
float BA6AEngine::lp(float x, LPState& s) noexcept {
    s.y += lpCoeff * (x - s.y);
    return s.y;
}

// Small asymmetric transfer meant to evoke tube harmonic structure, NOT a
// full 6SK7 / 6J7 / 6V6 SPICE model. The HP stage removes generated DC.
float BA6AEngine::shape(float x, float drive, float asymmetry) noexcept {
    const float p = std::tanh(x * drive + asymmetry) - std::tanh(asymmetry);
    return p / (drive * (1.0f - std::tanh(asymmetry) * std::tanh(asymmetry)));
}

float BA6AEngine::processSample(float x, int channel, float reductionGain) noexcept {
    // Virtual input transformer, matched push-pull remote-cutoff 6SK7 cell,
    // two 6J7 stages and 6V6 output stage are approximated in aggregate.
    const float imbalance = 0.024f * (controls.balanceA + 0.6f * controls.balanceB);
    const float heaterBias = std::clamp(controls.heater, -1.0f, 1.0f);
    const float first = shape(x * reductionGain, 1.45f + 0.24f * heaterBias, imbalance);
    const float second = shape(first, 1.20f, 0.035f);
    const float third = shape(second, 1.45f, 0.065f);
    return lp(hp(third, highPass[static_cast<std::size_t>(channel)]),
              lowPass[static_cast<std::size_t>(channel)]);
}

void BA6AEngine::process(float* left, float* right, std::size_t frames) noexcept {
    if (left == nullptr) return;
    const bool stereo = right != nullptr;
    for (std::size_t i = 0; i < frames; ++i) {
        if (!controls.power) {
            left[i] = 0.0f;
            if (stereo) right[i] = 0.0f;
            continue;
        }
        if (controls.mode == Mode::BalanceA || controls.mode == Mode::BalanceB) {
            // The real service switch disconnects the program output while
            // injecting 60 Hz for alignment; no diagnostic tone reaches DAW.
            left[i] = 0.0f;
            if (stereo) right[i] = 0.0f;
            continue;
        }

        const float inL = left[i] * padGain;
        const float inR = stereo ? right[i] * padGain : 0.0f;
        const float gain = dbToGain(-currentReduction);
        const float stageL = processSample(inL, 0, gain);
        const float stageR = stereo ? processSample(inR, 1, gain) : 0.0f;
        // Feedback from the V6 output/transformer, pre output attenuator.
        const float detector = std::max(std::abs(stageL), std::abs(stageR));
        fastEnvelope = detector + (fastEnvelope - detector) *
                       (detector > fastEnvelope ? fastAttack : fastRelease);
        slowEnvelope = fastEnvelope + (slowEnvelope - fastEnvelope) *
                       (fastEnvelope > slowEnvelope ? slowAttack : slowRelease);

        if (controls.mode == Mode::Off) {
            currentReduction *= controlSmoothRelease;
        } else {
            const float env = controls.mode == Mode::Dual
                                ? std::max(fastEnvelope, slowEnvelope) : fastEnvelope;
            // High loop gain models the strong remote-cutoff feedback.
            // Threshold is a documented digital-domain calibration assumption.
            const float feedbackError = std::max(0.0f, gainToDb(env) + 26.0f);
            const float targetReduction = std::clamp(feedbackError * 9.526f, 0.0f, 35.0f);
            const float coeff = targetReduction > currentReduction
                              ? controlSmoothAttack : controlSmoothRelease;
            currentReduction = targetReduction + coeff * (currentReduction - targetReduction);
        }
        const float outputL = stageL * outGain;
        const float outputR = stageR * outGain;
        left[i] = std::isfinite(outputL) ? outputL : 0.0f;
        if (stereo) right[i] = std::isfinite(outputR) ? outputR : 0.0f;
        visiblePeak = std::max(detector, visiblePeak * 0.9998f);
    }
    // GUI frame-rate smoothing happens in the editor, not per audio block.
    visibleReduction = currentReduction;
}

} // namespace ba6a
