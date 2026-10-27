#pragma once
// Front-panel control tables of the RCA BA-6A (MI-11225).
//
// Every value here is taken from the RCA instruction book IB-18-2035-2:
//  - Fig. 1 photograph of the front panel (dial-plate legends and angles),
//  - Technical Data ("Input: 20 steps, 2 db per step", "Output: 20 steps,
//    1 db per step", "Minimum -24 dbm / Maximum +14 dbm at limiting verge"),
//  - "Front-Panel Controls" and "Meter Indications" (meter switch functions).
// The same tables drive the DSP, the GUI pointer angles, the printed legends
// (assets/generate_panel.py reads them through Tests/print_tables.cpp) and the
// regression tests. Angles are degrees clockwise from twelve o'clock.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace ba6a::hw {

// ---------------------------------------------------------------- INPUT ---
// OFF + 20 contacts. The dial plate is marked in dBm of input signal needed
// to reach the verge of limiting: +14 (most attenuation) ... -24 (none).
constexpr int inputPositions = 21;               // index 0 = OFF
constexpr int inputDefaultIndex = 8;             // "0" dBm
constexpr int inputLabelDbm(int index) noexcept { return 16 - 2 * index; } // 1->+14, 20->-24
constexpr float inputAttenuationDb(int index) noexcept {   // R1 dual pot, 2 dB/step
    return 2.0f * static_cast<float>(20 - index);           // 1 -> 38 dB ... 20 -> 0 dB
}

// --------------------------------------------------------------- OUTPUT ---
// OFF + 20 contacts of the R36 bridged-T pad, 1 dB per step. The dial plate is
// marked with the approximate output level in dBm at the verge of limiting.
constexpr int outputPositions = 21;              // index 0 = OFF
constexpr int outputDefaultIndex = 8;            // "18" dBm
constexpr int outputLabelDbm(int index) noexcept { return 10 + index; }     // 1->11, 20->30
constexpr float outputAttenuationDb(int index) noexcept {
    return static_cast<float>(20 - index);                   // 20 -> 0 dB ... 1 -> 19 dB
}

// ------------------------------------------------------------ BAL-LIMIT ---
// S1, five positions, clockwise: A, B, OFF, SINGLE, DUAL.
enum class BalLimit : int { A = 0, B, Off, Single, Dual };
constexpr int balLimitPositions = 5;
constexpr int balLimitDefaultIndex = 2;          // OFF

// ---------------------------------------------------------------- METER ---
// S2: GR, V1..V6, FIL (heater), COARSE BAL, FINE BAL and OFF. The real switch
// turns through 360 degrees and has OFF on both sides of the bottom; the
// plugin exposes the eleven distinct functions.
enum class MeterSwitch : int { Off = 0, GR, V1, V2, V3, V4, V5, V6, Fil, CoarseBal, FineBal };
constexpr int meterPositions = 11;
constexpr int meterDefaultIndex = 1;             // GR

// ---------------------------------------------------- calibration to DAW ---
// The hardware works at broadcast line level (+29.5 dBm out at the verge).
// The plugin maps the dBm legends to digital levels with fixed references:
//   input : 0 dBm sine  <->  -18 dBFS  (EBU R68 alignment)
//   output: +36 dBm sine <->  0 dBFS   (so OUTPUT "18" gives the same level as
//           an INPUT "0" verge signal: unity gain at the verge).
constexpr float inputDbfsAtZeroDbm = -18.0f;
constexpr float outputDbmAtZeroDbfs = 36.0f;
// Fig. 4: gain below verge 53.8 dB; the unlimited (straight-line) output for an
// input equal to the INPUT legend is 0.15 dB under the OUTPUT legend.
constexpr float linearVergeOffsetDb = -0.15f;
// R35 FINE OUTPUT: 100-ohm rheostat in series with the 600-ohm line
// (600-ohm source into 600-ohm load): 20*log10(1200/1300) at full rotation.
constexpr float fineOutputMaxLossDb = -0.695f;

} // namespace ba6a::hw

namespace ba6a::panel {

// INPUT and OUTPUT dial plates: 21 contacts, 15 degrees apart, OFF at
// -150 deg (about 7 o'clock), last contact at +150 deg (5 o'clock).
// Measured on the Fig. 1 photograph: -4 dBm (INPUT) and 20 dBm (OUTPUT) at
// twelve o'clock, labels every 30 degrees.
constexpr float stepAngle(int index) noexcept { return -150.0f + 15.0f * static_cast<float>(index); }

// BAL-LIMIT: A, B, OFF (twelve o'clock), SINGLE, DUAL, 30 degrees apart.
constexpr std::array<float, 5> balLimitDegrees{{-60.f, -30.f, 0.f, 30.f, 60.f}};

// METER: OFF (bottom), GR, V1..V6, FIL, COARSE BAL, FINE BAL, 30 degrees apart.
constexpr std::array<float, 11> meterDegrees{{
    -165.f, -135.f, -105.f, -75.f, -45.f, -15.f, 15.f, 45.f, 75.f, 105.f, 135.f}};

inline float angleForIndex(int knob, int index) noexcept {
    switch (knob) {
        case 0: return stepAngle(std::clamp(index, 0, 20));                       // INPUT
        case 1: return meterDegrees[static_cast<std::size_t>(std::clamp(index, 0, 10))];
        case 2: return balLimitDegrees[static_cast<std::size_t>(std::clamp(index, 0, 4))];
        case 3: return stepAngle(std::clamp(index, 0, 20));                       // OUTPUT
        default: return 0.0f;
    }
}

constexpr int positionsForKnob(int knob) noexcept {
    return knob == 1 ? 11 : knob == 2 ? 5 : 21;
}

// ------------------------------------------------- DB GAIN REDUCTION meter --
// Needle deflection is proportional to the 6SK7 cathode current; RCA printed
// the scale in dB of gain reduction. Positions measured on Fig. 1 agree with
// the variable-mu model used in the DSP: fraction = 10^(-GR * 0.91 / 20).
constexpr float meterArcStartDeg = -40.5f;   // mechanical rest (no deflection)
constexpr float meterArcEndDeg = 41.0f;      // full scale = 0 dB gain reduction
constexpr float meterCurrentExponent = 0.91f;
inline float meterFractionForGr(float grDb) noexcept {
    return std::pow(10.0f, -std::max(0.0f, grDb) * meterCurrentExponent / 20.0f);
}
inline float meterAngleForFraction(float f) noexcept {
    return meterArcStartDeg + (meterArcEndDeg - meterArcStartDeg) * f;
}
constexpr float meterCheckBandLow = 0.483f;  // "7" mark
constexpr float meterCheckBandHigh = 0.587f; // "5" mark
constexpr float meterHeaterMark = 0.800f;    // HEATER mark (6.5 V d.c.)

} // namespace ba6a::panel
