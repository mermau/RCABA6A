#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Pure C++17 core: deterministic, allocation-free in the audio callback,
// deliberately independent of JUCE to allow quick native unit tests.
namespace ba6a {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinLevel = 1.0e-9f;

enum class Mode : int { BalanceA = 0, BalanceB, Off, Single, Dual };

struct Controls {
    int inputStep = 13;    // 0 = full attenuation/mute, 1..20 = 38..0 dB pad
    int outputStep = 20;   // 0..20 = -20..0 dB stepped output attenuator
    Mode mode = Mode::Single;
    bool power = true;
    float fineOutputDb = 0.0f; // service adjustment, [-1,+1] dB
    float balanceA = 0.0f;     // service calibration, [-1,+1]
    float balanceB = 0.0f;     // service calibration, [-1,+1]
    float heater = 0.0f;       // service calibration, [-1,+1]
    float hum = 0.0f;          // service calibration, [-1,+1]
};

inline float dbToGain(float dB) noexcept { return std::pow(10.0f, dB / 20.0f); }
inline float gainToDb(float amplitude) noexcept {
    return 20.0f * std::log10(std::max(amplitude, kMinLevel));
}

class BA6AEngine {
public:
    void prepare(double newSampleRate) noexcept;
    void reset() noexcept;
    void setControls(const Controls& newControls) noexcept;
    // Non-interleaved mono or stereo. right == nullptr means mono.
    void process(float* left, float* right, std::size_t frames) noexcept;
    float gainReductionDb() const noexcept { return visibleReduction; }
    float lastPeak() const noexcept { return visiblePeak; }
    static float inputAttenuationDb(int step) noexcept;
    static float outputAttenuationDb(int step) noexcept;

private:
    struct HPState { float previousX = 0, previousY = 0; };
    struct LPState { float y = 0; };
    float hp(float x, HPState& s) noexcept;
    float lp(float x, LPState& s) noexcept;
    float shape(float x, float drive, float asymmetry) noexcept;
    static float smoothCoefficient(float seconds, double sr) noexcept;
    float processSample(float x, int channel, float gain) noexcept;

    Controls controls;
    double sampleRate = 48000.0;
    float fastAttack = 0, fastRelease = 0;
    float slowAttack = 0, slowRelease = 0;
    float controlSmoothAttack = 0, controlSmoothRelease = 0;
    float hpCoeff = 0, lpCoeff = 0;
    float fastEnvelope = 0, slowEnvelope = 0;
    float currentReduction = 0, visibleReduction = 0, visiblePeak = 0;
    float padGain = 0, outGain = 1;
    std::array<HPState, 2> highPass {};
    std::array<LPState, 2> lowPass {};
};

} // namespace ba6a
