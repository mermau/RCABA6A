#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Mechanical detents and artwork coordinates, independent of JUCE.
namespace ba6a::panel {
constexpr int inputMin=-24, inputMax=24, outputMax=30, detentDb=2;
constexpr int inputDetents=25, outputDetents=16;
constexpr int inputStepFromLabel(int n) noexcept {
    return std::clamp((std::clamp(n,-24,24)+25)/2,0,24);
}
constexpr int outputStepFromLabel(int n) noexcept {
    return std::clamp((std::clamp(n,0,30)+1)/2,0,15);
}
// Panel clockwise: B, A, OFF, SINGLE, DUAL.
// DSP enum order: A, B, OFF, SINGLE, DUAL.
constexpr int modeChoiceToDspIndex(int n) noexcept {
    return n==0 ? 1 : n==1 ? 0 : std::clamp(n,2,4);
}
// Degrees clockwise from twelve o'clock; one value per detent.
constexpr std::array<float,25> inputDegrees{{
    -151.f,-138.5f,-126.f,-113.5f,-101.f,-90.f,-79.f,-67.f,
    -55.f,-42.f,-29.f,-15.f,0.f,13.f,26.f,40.f,
    54.f,66.f,78.f,90.f,102.f,114.5f,127.f,139.f,152.f
}};
constexpr std::array<float,16> outputDegrees{{
    -145.f,-134.f,-123.f,-107.f,-90.f,-74.f,-59.f,-45.f,
    -31.f,-15.f,0.f,30.f,60.f,76.f,90.f,145.f
}};
constexpr std::array<float,10> meterDegrees{{
    -92.f,-65.f,-49.f,-34.f,0.f,35.f,62.f,91.f,118.f,138.f
}};
constexpr std::array<float,5> balLimitDegrees{{-92.f,-48.f,0.f,42.f,79.f}};
template <std::size_t N> float snappedAngle(const std::array<float,N>& arr,float normalized) noexcept {
    const auto idx=static_cast<std::size_t>(std::lround(std::clamp(normalized,0.f,1.f)*static_cast<float>(N-1)));
    return arr[std::min(idx,N-1)];
}
inline float angleForKnob(int knob,float normalized) noexcept {
    switch(knob) {
        case 0: return snappedAngle(inputDegrees,normalized);
        case 1: return snappedAngle(meterDegrees,normalized);
        case 2: return snappedAngle(balLimitDegrees,normalized);
        case 3: return snappedAngle(outputDegrees,normalized);
        default: return 0.f;
    }
}
} // namespace ba6a::panel
