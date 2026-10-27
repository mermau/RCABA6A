#pragma once
// RCA BA-6A limiting amplifier — circuit-informed DSP core.
//
// Pure C++17, no JUCE, allocation-free in process(). The engine is meant to run
// at an oversampled rate (>= 88.2 kHz); the plugin wrapper oversamples 2x when
// the host runs at 44.1/48 kHz.
//
// Signal graph (RCA IB-18-2035-2, Figs. 3 and 8):
//   INPUT pad R1 (2 dB steps, in the 6SK7 grid circuit)
//   -> V1/V2 6SK7 remote-cutoff pentodes, push-pull, gain set by the d.c.
//      control voltage on both grids (variable-mu)
//   -> T2 interstage -> V3/V4 6J7 -> V5/V6 6V6 (fixed gain, negative feedback)
//   -> T3 -> OUTPUT bridged-T pad R36 (1 dB steps) + FINE OUTPUT R35
//   Side chain: V5/V6 plates -> isolating network -> V7 6H6 full-wave rectifier
//   with fixed positive cathode bias (verge of limiting) -> C1 0.1 uF || R11
//   3.3 M (SINGLE); in DUAL the series R37 3.3 M + C18 0.27 uF is added in
//   parallel -> 6SK7 grids.
// See docs/CIRCUIT_ANALYSIS.md for the derivation of every constant.
#include "ControlCalibration.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ba6a {

struct Controls {
    int inputIndex = hw::inputDefaultIndex;      // 0 = OFF, 1..20 = +14..-24 dBm
    int outputIndex = hw::outputDefaultIndex;    // 0 = OFF, 1..20 = 11..30 dBm
    hw::BalLimit balLimit = hw::BalLimit::Off;
    hw::MeterSwitch meter = hw::MeterSwitch::GR;
    bool power = true;
    bool stereoLink = true;      // plugin extension: two units with tied control grids
    bool residualNoise = true;   // hiss + heater hum within the published noise (-54 dBm)
    bool mains50Hz = false;      // the US original runs on 60 c/s (balancing tone, hum)
    float balanceA = 0.0f;       // R3 cathode balance, -1..+1 (0 = aligned)
    float balanceB = 0.0f;       // R8 screen balance,  -1..+1 (0 = aligned)
    float zeroAdj = 0.0f;        // R47 meter zero, -1..+1 (0 = aligned)
    float fineOutput = 0.0f;     // R35 100-ohm rheostat, 0..1 of its travel
    float heaterAdj = 0.0f;      // R64, -1..+1 -> 5.9..7.1 V d.c. on the 6SK7 heaters
};

class BA6AEngine {
public:
    // Constants of the model (public for tests and documentation).
    static constexpr double eta = 0.099;          // remote-cutoff curvature (fits meter scale + Fig. 4)
    static constexpr double driveAtVerge = 0.165; // 6SK7 grid swing at verge, units of V0
    static constexpr double detectorGain = 8.6;   // 6H6 bias (66 V) / 6SK7 V0 (~7.5 V), fitted to Fig. 4
    static constexpr double detectorThreshold = 0.9638; // verge, -0.32 dB re straight-line output
    static constexpr double diodeKnee = 0.06;     // 6H6 conduction knee, units of V0
    static constexpr double rSource = 6.0e3;      // 0.6 ms attack with C1
    static constexpr double c1 = 0.1e-6, r11 = 3.3e6;   // SINGLE: 0.6 ms / 0.33 s
    static constexpr double c18 = 0.27e-6, r37 = 3.3e6; // DUAL: 0.9 s / ~2 s
    static constexpr double biasBalanceA = 0.5385; // control voltage giving 5 dB GR (meter table)
    static constexpr double biasBalanceB = 1.1040; // control voltage giving 10 dB GR

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void setControls(const Controls&) noexcept;
    // Non-interleaved. right == nullptr -> mono.
    void process(float* left, float* right, std::size_t frames) noexcept;

    float gainReductionDb() const noexcept { return grDb; }
    float meterTarget() const noexcept { return needleTarget; }   // 0 = rest, 1 = full scale
    const Controls& controls() const noexcept { return ctl; }

    static double grForControl(double c) noexcept;      // dB of gain reduction
    static double controlForGr(double grDb) noexcept;
    static float inputGain(int index) noexcept;          // dBFS sample -> internal units
    static float outputGain(int index, float fine) noexcept; // internal -> dBFS sample
    static double outputStage(double s, double saturation) noexcept;

private:
    struct OnePoleHP {
        double b0 = 1, a1 = 0, x1 = 0, y1 = 0;
        void design(double fc, double fs) noexcept;
        double run(double x) noexcept { const double y = b0 * (x - x1) - a1 * y1; x1 = x; y1 = y; return y; }
        void prime(double x) noexcept { x1 = x; y1 = 0; }
    };
    struct Biquad {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void lowpass(double fc, double q, double fs) noexcept;
        double run(double x) noexcept {
            const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y;
        }
        void clear() noexcept { z1 = z2 = 0; }
    };
    struct Channel {
        OnePoleHP t2, t3, detHp, detShelf;
        Biquad lpA, lpB;
        double flux = 0, iA = 1, iB = 1, outRect = 0;
        double inputFlux = 0, supplyLoad = 0, sag = 0;
        std::uint32_t rng = 0x9e3779b9u;
    };
    struct Detector { double v1 = 0, v18 = 0, grid = 0, drive = 0, conductance = 0; };

    double tube(double u, double etaTube) const noexcept;
    double tubeSlope(double u, double etaTube) const noexcept;
    void solveBalance() noexcept;
    void stepDetector(Detector&, bool dual) noexcept;
    double noise(Channel&) noexcept;

    Controls ctl;
    double fs = 96000.0, dt = 1.0 / 96000.0;
    std::array<Channel, 2> ch {};
    std::array<Detector, 2> det {};
    double inGain = 0, inTarget = 0, outGain = 0, outTarget = 0, rampCoeff = 0;
    double heaterTemp = 1, bPlus = 1, heaterCoeffOn = 0, heaterCoeffOff = 0;
    double bPlusOn = 0, bPlusOff = 0, gridCoeff = 0, meterCoeff = 0, rectCoeff = 0;
    double fluxStep = 0, fluxLeak = 0, testPhase = 0, testStep60 = 0, testStep50 = 0;
    double supplyCoeff = 0, sagCoeff = 0;
    double dU0 = 0, dK0 = 0, noiseSigma = 0;
    float grDb = 0, needleTarget = 1;
};

} // namespace ba6a
