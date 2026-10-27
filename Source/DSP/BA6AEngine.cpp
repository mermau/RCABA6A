#include "BA6AEngine.h"
#include <algorithm>

namespace ba6a {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDbPerNeper = 8.685889638065035;

// ---- non-trimmable residual mismatch of a "non-selected" 6SK7 pair --------
// After the A/B balancing procedure the two tubes are matched at the two
// balance bias points only; a small difference in remote-cutoff curvature
// remains. Sized so that the default unit shows the published signal-to-thump
// ratio (21 dB minimum, 26 dB average, for 5-10 dB steps above verge).
constexpr double kResidualEtaMismatch = 0.22;
constexpr double kTrimRangeA = 0.06;   // BALANCE A: cathode (grid-to-cathode offset), V0 units
constexpr double kTrimRangeB = 0.10;   // BALANCE B: screen (bias-proportional offset)
constexpr double kTestAmpA = 0.12;     // internal 60 c/s balancing voltage, position A
constexpr double kTestAmpB = 0.24;     // position B "increases the 60-cycle voltage"

// 6SK7 grid conduction: grid current starts once the grid approaches the
// cathode (5.15 V of cathode bias, ~0.69 V0).
constexpr double kGridKnee = 0.35, kGridLimit = 0.69;

// 6J7 + 6V6 fixed-gain stages with negative feedback: low distortion until
// the push-pull 6V6 pair saturates ~9 dB above the verge output
// (38.5 dBm maximum vs 29.5 dBm at verge).
constexpr double kOutSat = 3.0, kOutH3 = 0.008, kOutH2 = 0.0015;

// T3 core: low-frequency flux-dependent loss (THD < 1.2 % at 30 c/s, verge).
constexpr double kCoreLoss = 0.028;
// T1 sits ahead of the INPUT attenuator, so its core sees the absolute input
// level: 0.5 % THD at 30 c/s for the maximum rated input (+14 dBm).
constexpr double kInputCoreLoss = 0.02;
constexpr double kInputCoreRefPeak = 0.6309573;   // +14 dBm sine = -4 dBFS peak
// 5R4GY + 4.5 H choke-input supply: the 6V6 pair's class-AB current pulls B+
// down on sustained heavy output, lowering the clipping point (up to 5 %).
constexpr double kSagDepth = 0.05;
// Residual noise: hiss -56 dBm and hum from the a.c.-heated 6J7/6V6 -59 dBm,
// together -54 dBm (Technical Data: maximum noise -54 dBm, controls at maximum).
constexpr double kHumAmplitude = 3.27e-5;

// Frequency shaping (Fig. 5, see docs/CIRCUIT_ANALYSIS.md).
constexpr double kT2Hp = 4.4, kT3Hp = 4.4, kDetHp = 10.5;
constexpr double kDetShelfFc = 30000.0, kDetShelfGain = 0.65;
constexpr double kOutLp = 21500.0;

// Published S/N: 83 dB at verge of limiting (noise -54 dBm, verge +29.5 dBm).
constexpr double kNoiseRelVergeDb = -85.5;

inline double softplus(double x) noexcept {
    return x > 30.0 ? x : (x < -30.0 ? std::exp(x) : std::log1p(std::exp(x)));
}
inline double sigmoid(double x) noexcept { return 1.0 / (1.0 + std::exp(-x)); }

} // namespace

double BA6AEngine::outputStage(double s, double saturation) noexcept {
    const double t = (s / saturation) * (s / saturation);
    const double q = s / std::pow(1.0 + t * t * t, 1.0 / 6.0);
    return q - kOutH3 * q * q * q + kOutH2 * q * q;
}

namespace {
inline double smoothstep(double x) noexcept {
    x = std::clamp(x, 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}
} // namespace

// ------------------------------------------------------------- filters -----
void BA6AEngine::OnePoleHP::design(double fc, double sr) noexcept {
    fc = std::min(fc, 0.45 * sr);
    const double k = std::tan(kPi * fc / sr);
    b0 = 1.0 / (1.0 + k);
    a1 = (k - 1.0) / (k + 1.0);
}
void BA6AEngine::Biquad::lowpass(double fc, double q, double sr) noexcept {
    fc = std::min(fc, 0.45 * sr);
    const double k = std::tan(kPi * fc / sr), k2 = k * k;
    const double norm = 1.0 / (1.0 + k / q + k2);
    b0 = k2 * norm; b1 = 2.0 * b0; b2 = b0;
    a1 = 2.0 * (k2 - 1.0) * norm;
    a2 = (1.0 - k / q + k2) * norm;
}

// -------------------------------------------------------- tube model -------
// Remote-cutoff pentode: ln(i) = -ln(1 - eta*u)/eta, i.e. the local
// exponential slope V0 grows linearly with negative bias. Units: grid volts in
// V0 (~7.5 V), current normalised to the zero-signal plate current.
double BA6AEngine::tube(double u, double etaTube) const noexcept {
    if (u > kGridKnee)
        u = kGridKnee + (kGridLimit - kGridKnee) * std::tanh((u - kGridKnee) / (kGridLimit - kGridKnee));
    return std::exp(-std::log1p(-etaTube * u) / etaTube);
}
double BA6AEngine::tubeSlope(double u, double etaTube) const noexcept {
    return std::exp((-1.0 / etaTube - 1.0) * std::log1p(-etaTube * u));
}
double BA6AEngine::grForControl(double c) noexcept {
    return kDbPerNeper * (1.0 + 1.0 / eta) * std::log1p(eta * std::max(0.0, c));
}
double BA6AEngine::controlForGr(double gr) noexcept {
    return std::expm1(std::max(0.0, gr) / (kDbPerNeper * (1.0 + 1.0 / eta))) / eta;
}

// Factory alignment: offsets of the pair that the BALANCE A/B procedure
// cancels, leaving only the curvature mismatch.
void BA6AEngine::solveBalance() noexcept {
    const double ea = eta * (1.0 + kResidualEtaMismatch), eb = eta * (1.0 - kResidualEtaMismatch);
    auto leak = [&](double c, double du, double dk) {
        return tubeSlope(-c + du - dk * c, ea) - tubeSlope(-c - du + dk * c, eb);
    };
    double du = 0, dk = 0;
    for (int it = 0; it < 30; ++it) {
        const double fa = leak(biasBalanceA, du, dk), fb = leak(biasBalanceB, du, dk);
        const double h = 1e-6;
        const double ja1 = (leak(biasBalanceA, du + h, dk) - fa) / h;
        const double ja2 = (leak(biasBalanceA, du, dk + h) - fa) / h;
        const double jb1 = (leak(biasBalanceB, du + h, dk) - fb) / h;
        const double jb2 = (leak(biasBalanceB, du, dk + h) - fb) / h;
        const double detJ = ja1 * jb2 - ja2 * jb1;
        if (std::abs(detJ) < 1e-15) break;
        du -= (fa * jb2 - fb * ja2) / detJ;
        dk -= (ja1 * fb - jb1 * fa) / detJ;
    }
    dU0 = du; dK0 = dk;
}

// ---------------------------------------------------------- lifecycle ------
void BA6AEngine::prepare(double sampleRate) noexcept {
    fs = std::max(22050.0, sampleRate);
    dt = 1.0 / fs;
    for (auto& c : ch) {
        c.t2.design(kT2Hp, fs);
        c.t3.design(kT3Hp, fs);
        c.detHp.design(kDetHp, fs);
        c.detShelf.design(kDetShelfFc, fs);
        c.lpA.lowpass(kOutLp, 0.54119610, fs);
        c.lpB.lowpass(kOutLp, 1.30656296, fs);
    }
    auto coeff = [this](double seconds) { return 1.0 - std::exp(-dt / seconds); };
    rampCoeff = coeff(0.004);
    gridCoeff = coeff(0.00015);
    meterCoeff = coeff(0.020);
    rectCoeff = coeff(0.060);
    heaterCoeffOn = coeff(3.5);
    heaterCoeffOff = coeff(6.0);
    bPlusOn = coeff(0.5);
    bPlusOff = coeff(0.25);
    fluxStep = 2.0 * kPi * 30.0 / fs;            // flux = 1 for a unit 30 c/s sine
    fluxLeak = std::exp(-2.0 * kPi * 1.0 / fs);
    testStep60 = 2.0 * kPi * 60.0 / fs;
    testStep50 = 2.0 * kPi * 50.0 / fs;
    supplyCoeff = coeff(0.040);
    sagCoeff = coeff(0.120);
    noiseSigma = std::sqrt(0.5) * std::pow(10.0, kNoiseRelVergeDb / 20.0)
               * std::sqrt((fs * 0.5) / 20000.0);
    solveBalance();
    reset();
}

void BA6AEngine::reset() noexcept {
    std::uint32_t seed = 0x2545F491u;
    for (auto& c : ch) {
        c.t2 = OnePoleHP{c.t2.b0, c.t2.a1};
        c.t3 = OnePoleHP{c.t3.b0, c.t3.a1};
        c.detHp = OnePoleHP{c.detHp.b0, c.detHp.a1};
        c.detShelf = OnePoleHP{c.detShelf.b0, c.detShelf.a1};
        c.lpA.clear(); c.lpB.clear();
        c.flux = 0; c.iA = c.iB = 1; c.outRect = 0;
        c.inputFlux = 0; c.supplyLoad = 0; c.sag = 0;
        c.rng = (seed += 0x9E3779B9u);
    }
    det = {};
    heaterTemp = ctl.power ? 1.0 : 0.0;
    bPlus = ctl.power ? 1.0 : 0.0;
    testPhase = 0;
    setControls(ctl);
    inGain = inTarget;
    outGain = outTarget;
    // Pre-charge T2 with the zero-signal current difference of the pair so a
    // freshly loaded instance does not thump.
    const double du = dU0 + kTrimRangeA * ctl.balanceA, dk = dK0 + kTrimRangeB * ctl.balanceB;
    const double rest = (tube(du, eta * (1 + kResidualEtaMismatch))
                       - tube(-du, eta * (1 - kResidualEtaMismatch))) / driveAtVerge;
    (void) dk;
    for (auto& c : ch) c.t2.prime(rest * bPlus);
}

float BA6AEngine::inputGain(int index) noexcept {
    if (index <= 0) return 0.0f;
    const float label = static_cast<float>(hw::inputLabelDbm(std::min(index, 20)));
    return std::pow(10.0f, (-hw::inputDbfsAtZeroDbm - label) / 20.0f);
}
float BA6AEngine::outputGain(int index, float fine) noexcept {
    if (index <= 0) return 0.0f;
    const float label = static_cast<float>(hw::outputLabelDbm(std::min(index, 20)));
    const float dbm = label + hw::linearVergeOffsetDb
                    + hw::fineOutputMaxLossDb * std::clamp(fine, 0.0f, 1.0f);
    return std::pow(10.0f, (dbm - hw::outputDbmAtZeroDbfs) / 20.0f);
}

void BA6AEngine::setControls(const Controls& in) noexcept {
    ctl = in;
    ctl.inputIndex = std::clamp(ctl.inputIndex, 0, 20);
    ctl.outputIndex = std::clamp(ctl.outputIndex, 0, 20);
    ctl.balanceA = std::clamp(ctl.balanceA, -1.0f, 1.0f);
    ctl.balanceB = std::clamp(ctl.balanceB, -1.0f, 1.0f);
    ctl.zeroAdj = std::clamp(ctl.zeroAdj, -1.0f, 1.0f);
    ctl.fineOutput = std::clamp(ctl.fineOutput, 0.0f, 1.0f);
    ctl.heaterAdj = std::clamp(ctl.heaterAdj, -1.0f, 1.0f);
    inTarget = inputGain(ctl.inputIndex);
    // In A and B the output is disconnected from the line and switched to the meter.
    const bool lineConnected = ctl.balLimit != hw::BalLimit::A && ctl.balLimit != hw::BalLimit::B;
    outTarget = lineConnected ? outputGain(ctl.outputIndex, ctl.fineOutput) : 0.0;
}

double BA6AEngine::noise(Channel& c) noexcept {
    double sum = 0;
    for (int k = 0; k < 4; ++k) {
        c.rng ^= c.rng << 13; c.rng ^= c.rng >> 17; c.rng ^= c.rng << 5;
        sum += static_cast<double>(c.rng) * (2.0 / 4294967295.0) - 1.0;
    }
    return sum * 0.8660254037844386 * noiseSigma;   // unit variance * sigma
}

// Backward-Euler step of the C1/R11 (+ R37/C18) network driven by the 6H6.
void BA6AEngine::stepDetector(Detector& d, bool dual) noexcept {
    const double k1 = c1 / dt, k18 = c18 / dt, g37 = dual ? 1.0 / r37 : 0.0;
    const double a = k1 + d.conductance + 1.0 / r11 + g37;
    const double rhs = k1 * d.v1 + d.drive + d.conductance * d.v1;
    if (!dual) {
        d.v1 = rhs / a;
    } else {
        const double den = k18 + g37;
        const double x = (rhs + g37 * k18 * d.v18 / den) / (a - g37 * g37 / den);
        d.v18 = (k18 * d.v18 + g37 * x) / den;
        d.v1 = x;
    }
    d.v1 = std::max(0.0, d.v1);
    d.drive = d.conductance = 0;
}

// ------------------------------------------------------------- process -----
void BA6AEngine::process(float* left, float* right, std::size_t frames) noexcept {
    if (left == nullptr) return;
    float* io[2] = {left, right};
    const int channels = right != nullptr ? 2 : 1;
    const bool linked = ctl.stereoLink && channels == 2;
    const int detectors = linked ? 1 : channels;
    const double gd = 1.0 / rSource / (linked ? 2.0 : 1.0);
    const auto mode = ctl.balLimit;
    const bool dual = mode == hw::BalLimit::Dual;
    const bool limiting = mode == hw::BalLimit::Single || dual;
    const double testAmp = mode == hw::BalLimit::A ? kTestAmpA : mode == hw::BalLimit::B ? kTestAmpB : 0.0;
    const double fixedBias = mode == hw::BalLimit::A ? biasBalanceA : mode == hw::BalLimit::B ? biasBalanceB : 0.0;
    const double du = dU0 + kTrimRangeA * ctl.balanceA;
    const double dk = dK0 + kTrimRangeB * ctl.balanceB;
    const double etaA = eta * (1.0 + kResidualEtaMismatch), etaB = eta * (1.0 - kResidualEtaMismatch);
    // R64: 6SK7 d.c. heater voltage 6.5 V nominal; emission falls when under-heated.
    const double heaterVolts = 6.5 + 0.6 * ctl.heaterAdj;
    const double heaterGain = heaterVolts < 6.5 ? 1.0 - 0.9 * (6.5 - heaterVolts) / 6.5
                                                : 1.0 + 0.1 * (heaterVolts - 6.5) / 6.5;
    const double sigma = ctl.residualNoise ? 1.0 : 0.0;

    for (std::size_t n = 0; n < frames; ++n) {
        inGain += rampCoeff * (inTarget - inGain);
        outGain += rampCoeff * (outTarget - outGain);
        heaterTemp += (ctl.power ? heaterCoeffOn * (1.0 - heaterTemp) : -heaterCoeffOff * heaterTemp);
        bPlus += ctl.power ? bPlusOn * (1.0 - bPlus) : -bPlusOff * bPlus;
        const double emission = smoothstep((heaterTemp - 0.45) / 0.45) * heaterGain;
        const double test = testAmp * std::sin(testPhase);
        const double hum = sigma * kHumAmplitude
                         * (std::sin(testPhase) + 0.5 * std::sin(2.0 * testPhase + 0.7) + 0.25 * std::sin(3.0 * testPhase + 1.9));
        testPhase += ctl.mains50Hz ? testStep50 : testStep60;
        if (testPhase > 2.0 * kPi) testPhase -= 2.0 * kPi;

        for (int c = 0; c < channels; ++c) {
            Channel& s = ch[static_cast<std::size_t>(c)];
            Detector& d = det[static_cast<std::size_t>(linked ? 0 : c)];
            const float raw = io[c][n];
            double line = std::isfinite(raw) ? std::clamp(static_cast<double>(raw), -1.0e4, 1.0e4) : 0.0;
            // T1 input transformer (before the attenuator).
            {
                const double v = line / kInputCoreRefPeak;
                s.inputFlux = s.inputFlux * fluxLeak + fluxStep * v;
                const double f2 = s.inputFlux * s.inputFlux;
                line -= kInputCoreLoss * f2 / (1.0 + f2 / 9.0) * line;
            }
            const double x = line * inGain;
            const double vg = driveAtVerge * x;
            const double g = d.grid;
            const double uA = -g + 0.5 * vg + test + du - dk * g;
            const double uB = -g - 0.5 * vg + test - du + dk * g;
            const double iA = emission * tube(uA, etaA);
            const double iB = emission * tube(uB, etaB);
            double y = ((iA - iB) / driveAtVerge + sigma * noise(s) + hum) * bPlus;
            y = s.t2.run(y);
            const double s2 = outputStage(y, kOutSat * (1.0 - s.sag));
            s.supplyLoad += supplyCoeff * (std::abs(s2) - s.supplyLoad);
            s.sag += sagCoeff * (kSagDepth * std::clamp((s.supplyLoad - 0.9) / 1.0, 0.0, 1.0) - s.sag);
            // Side chain from the 6V6 plates through the isolating network.
            double sd = s.detHp.run(s2);
            sd += kDetShelfGain * s.detShelf.run(sd);
            const double v = detectorGain * (std::abs(sd) - detectorThreshold) - d.v1;
            d.drive += gd * diodeKnee * softplus(v / diodeKnee);
            d.conductance += gd * sigmoid(v / diodeKnee);
            // T3 output transformer.
            s.flux = s.flux * fluxLeak + fluxStep * s2;
            const double f2 = s.flux * s.flux;
            double s3 = s.t3.run(s2 - kCoreLoss * f2 / (1.0 + f2 / 9.0) * s2);
            s3 = s.lpB.run(s.lpA.run(s3));
            s.iA += meterCoeff * (iA - s.iA);
            s.iB += meterCoeff * (iB - s.iB);
            s.outRect += rectCoeff * (std::abs(s3) - s.outRect);
            const double out = s3 * outGain;
            io[c][n] = std::isfinite(out) ? static_cast<float>(out) : 0.0f;
        }

        for (int k = 0; k < detectors; ++k) {
            Detector& d = det[static_cast<std::size_t>(k)];
            if (limiting) {
                stepDetector(d, dual);
            } else {
                d.v1 = 0.0;                       // control voltage shorted to ground
                d.drive = d.conductance = 0.0;
            }
            const double target = limiting ? d.v1 : fixedBias;
            d.grid += gridCoeff * (target - d.grid);
        }
    }

    // ---- meter --------------------------------------------------------------
    double maxGrid = 0, cathode = 2, tubeA = 1, tubeB = 1, rect = 0;
    for (int c = 0; c < channels; ++c) {
        const auto& s = ch[static_cast<std::size_t>(c)];
        maxGrid = std::max(maxGrid, det[static_cast<std::size_t>(linked ? 0 : c)].grid);
        const double k = 0.5 * (s.iA + s.iB);
        if (k < cathode) { cathode = k; tubeA = s.iA; tubeB = s.iB; }
        rect = std::max(rect, s.outRect);
    }
    grDb = static_cast<float>(grForControl(maxGrid));
    const double centre = 0.5 * (panel::meterCheckBandLow + panel::meterCheckBandHigh);
    const double drive = std::min(1.5, rect / 0.6366);   // output level re verge
    const double warm = smoothstep((heaterTemp - 0.45) / 0.45) * bPlus;
    double f = 0;
    switch (ctl.meter) {
        case hw::MeterSwitch::Off: f = 0; break;
        case hw::MeterSwitch::GR:  f = cathode * bPlus * (1.0 + 0.12 * ctl.zeroAdj); break;
        case hw::MeterSwitch::V1:  f = centre * tubeA * bPlus; break;
        case hw::MeterSwitch::V2:  f = centre * tubeB * bPlus; break;
        case hw::MeterSwitch::V3:  f = centre * 1.02 * warm; break;
        case hw::MeterSwitch::V4:  f = centre * 0.985 * warm; break;
        case hw::MeterSwitch::V5:  f = centre * (0.99 + 0.05 * drive) * warm; break;
        case hw::MeterSwitch::V6:  f = centre * (1.01 + 0.05 * drive) * warm; break;
        case hw::MeterSwitch::Fil: f = ctl.power ? panel::meterHeaterMark * heaterVolts / 6.5 : 0.0; break;
        case hw::MeterSwitch::CoarseBal: f = rect * 25.0; break;    // full scale: -24 dB re verge
        case hw::MeterSwitch::FineBal:   f = rect * 250.0; break;   // full scale: -44 dB re verge
    }
    needleTarget = static_cast<float>(std::clamp(f, 0.0, 1.06));   // 1.06 = end stop
}

} // namespace ba6a
