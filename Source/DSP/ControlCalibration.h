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
// Static markings and their corresponding knob indicator share this map.
// 0 INPUT and 16 OUTPUT lie exactly at 12 o'clock.
constexpr std::array<float,25> inputDegrees{{
-150.f,-137.5f,-125.f,-112.5f,-100.f,-87.5f,-75.f,-62.5f,-50.f,-37.5f,-25.f,-12.5f,0.f,
12.5f,25.f,37.5f,50.f,62.5f,75.f,87.5f,100.f,112.5f,125.f,137.5f,150.f
}};
// Noon bisects the two center labels: 14 and 16.
constexpr std::array<float,16> outputDegrees{{
-139.f,-120.46667f,-101.93333f,-83.4f,-64.86667f,-46.33333f,
-27.8f,-9.26667f,9.26667f,27.8f,46.33333f,64.86667f,
83.4f,101.93333f,120.46667f,139.f
}};
// Ten clockwise positions: IN,V1,V2,V3,V4,V5,V6,FIL,COMPR,BAL.
constexpr std::array<float,10> meterDegrees{{
-136.f,-94.f,-67.f,-44.f,-22.f,0.f,28.f,60.f,93.f,136.f
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
