// Regression tests of the DSP core against the RCA BA-6A instruction book
// (IB-18-2035-2): Technical Data, Fig. 4 (limiter characteristic), Fig. 5
// (frequency response), Figs. 6/7 (single/dual recovery) and the Meter
// Indications table. Run at 96 kHz, the oversampled rate used by the plugin.
#include "DSP/BA6AEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>
#include <vector>

using namespace ba6a;
static int failures = 0;
static constexpr double fs = 96000.0;
static constexpr double pi = 3.14159265358979323846;

static void check(bool ok, const char* what, double value, double lo, double hi) {
    std::printf("%s %-58s %9.3f  [%g .. %g]\n", ok ? "PASS" : "FAIL", what, value, lo, hi);
    if (!ok) ++failures;
}
static void range(const char* what, double v, double lo, double hi) { check(v >= lo && v <= hi, what, v, lo, hi); }

static float dbmToDbfsPeakIn(double dbm) { return static_cast<float>(std::pow(10.0, (dbm + hw::inputDbfsAtZeroDbm) / 20.0)); }
static double peakToDbmOut(double peak) { return 20.0 * std::log10(std::max(peak, 1e-12)) + hw::outputDbmAtZeroDbfs; }

struct Tone { double amplitude = 0, thd = 0; };

// Feed a sine for `seconds`, analyse the last `analyse` seconds (integer cycles).
static Tone runTone(BA6AEngine& e, double freq, float peak, double seconds, double analyse = 0.2,
                    bool stereo = false, float rightPeak = 0.0f) {
    const auto total = static_cast<std::size_t>(seconds * fs);
    const double cycles = std::max(1.0, std::floor(analyse * freq));
    const auto nAn = static_cast<std::size_t>(std::llround(cycles / freq * fs));
    std::vector<float> l(512), r(512), tail; tail.reserve(nAn);
    for (std::size_t start = 0; start < total; start += 512) {
        const std::size_t n = std::min<std::size_t>(512, total - start);
        for (std::size_t i = 0; i < n; ++i) {
            const double ph = 2.0 * pi * freq * static_cast<double>(start + i) / fs;
            l[i] = static_cast<float>(peak * std::sin(ph));
            r[i] = static_cast<float>(rightPeak * std::sin(ph));
        }
        e.process(l.data(), stereo ? r.data() : nullptr, n);
        for (std::size_t i = 0; i < n; ++i) if (start + i >= total - nAn) tail.push_back(l[i]);
    }
    auto harmonic = [&](int h) {
        double c = 0, s = 0;
        for (std::size_t i = 0; i < tail.size(); ++i) {
            const double ph = 2.0 * pi * freq * h * static_cast<double>(total - nAn + i) / fs;
            c += tail[i] * std::cos(ph); s += tail[i] * std::sin(ph);
        }
        return 2.0 * std::sqrt(c * c + s * s) / static_cast<double>(tail.size());
    };
    Tone t; t.amplitude = harmonic(1);
    double hs = 0;
    for (int h = 2; h <= 10 && freq * h < 20000.0; ++h) { const double a = harmonic(h); hs += a * a; }
    t.thd = 100.0 * std::sqrt(hs) / std::max(t.amplitude, 1e-12);
    return t;
}

static BA6AEngine makeEngine(hw::BalLimit mode, int inputIndex = 20, int outputIndex = 20) {
    BA6AEngine e; e.prepare(fs);
    Controls c; c.inputIndex = inputIndex; c.outputIndex = outputIndex; c.balLimit = mode;
    c.residualNoise = false;
    e.setControls(c); e.reset();
    return e;
}

// Input dBm giving `gr` dB of gain reduction at 1 kHz (bisection).
static double inputForGr(double gr, hw::BalLimit mode = hw::BalLimit::Single) {
    double lo = -30, hi = 10;
    for (int it = 0; it < 22; ++it) {
        const double m = 0.5 * (lo + hi);
        auto e = makeEngine(mode);
        runTone(e, 1000, dbmToDbfsPeakIn(m), 0.6);
        (e.gainReductionDb() > gr ? hi : lo) = m;
    }
    return 0.5 * (lo + hi);
}

int main() {
    std::printf("---- panel tables ----\n");
    range("INPUT index 1 legend (dBm)", hw::inputLabelDbm(1), 14, 14);
    range("INPUT index 20 legend (dBm)", hw::inputLabelDbm(20), -24, -24);
    range("INPUT 2 dB per step", hw::inputAttenuationDb(10) - hw::inputAttenuationDb(11), 2, 2);
    range("OUTPUT index 1 legend (dBm)", hw::outputLabelDbm(1), 11, 11);
    range("OUTPUT index 20 legend (dBm)", hw::outputLabelDbm(20), 30, 30);
    range("OUTPUT 1 dB per step", hw::outputAttenuationDb(10) - hw::outputAttenuationDb(11), 1, 1);
    range("INPUT -4 dBm contact at twelve o'clock", panel::angleForIndex(0, 10), 0, 0);
    range("OUTPUT 20 dBm contact at twelve o'clock", panel::angleForIndex(3, 10), 0, 0);
    range("BAL-LIMIT OFF at twelve o'clock", panel::angleForIndex(2, 2), 0, 0);
    range("meter scale 20 dB position (Fig. 1: 0.123)", panel::meterFractionForGr(20), 0.11, 0.135);
    range("meter scale 10 dB position (Fig. 1: 0.362)", panel::meterFractionForGr(10), 0.34, 0.375);

    std::printf("---- Fig. 4 limiter characteristic (INPUT -24, OUTPUT 30, SINGLE) ----\n");
    const double fig4[][2] = {{-30, 23.85}, {-26, 27.85}, {-24.5, 29.28}, {-23.5, 29.62}, {-22.5, 29.74},
                              {-21, 29.86}, {-17, 30.21}, {-14.5, 30.43}, {-11, 30.77}, {-9.5, 30.92},
                              {-4, 31.48}, {-1, 31.78}};
    double worst = 0;
    for (const auto& p : fig4) {
        auto e = makeEngine(hw::BalLimit::Single);
        const auto t = runTone(e, 1000, dbmToDbfsPeakIn(p[0]), 0.8);
        const double out = peakToDbmOut(t.amplitude);
        std::printf("     in %6.1f dBm -> out %6.2f dBm (RCA %5.2f)  GR %5.2f dB\n", p[0], out, p[1], e.gainReductionDb());
        worst = std::max(worst, std::abs(out - p[1]));
    }
    range("worst deviation from Fig. 4 (dB)", worst, 0, 0.35);
    {
        auto lo = makeEngine(hw::BalLimit::Single), hi = makeEngine(hw::BalLimit::Single);
        const double a = peakToDbmOut(runTone(lo, 1000, dbmToDbfsPeakIn(-24), 0.8).amplitude);
        const double b = peakToDbmOut(runTone(hi, 1000, dbmToDbfsPeakIn(-4), 0.8).amplitude);
        range("compression: 20 dB above verge into (dB)", b - a, 1.6, 2.4);
        auto g = makeEngine(hw::BalLimit::Single);
        const double below = peakToDbmOut(runTone(g, 1000, dbmToDbfsPeakIn(-34), 0.5).amplitude) + 34;
        range("gain below verge (dB, RCA 53.8 / 54 +-1)", below, 53.3, 54.3);
        range("output at verge (dBm, RCA 29.5 +-0.5)", a, 29.0, 30.0);
    }

    std::printf("---- calibration of the dial legends (any INPUT/OUTPUT) ----\n");
    for (int in : {1, 8, 14}) for (int out : {1, 8, 20}) {
        auto e = makeEngine(hw::BalLimit::Single, in, out);
        const double label = hw::inputLabelDbm(in);
        const double o = peakToDbmOut(runTone(e, 1000, dbmToDbfsPeakIn(label), 0.8).amplitude);
        char what[96]; std::snprintf(what, sizeof what, "INPUT %+d / OUTPUT %d: output at verge re legend", (int) label, hw::outputLabelDbm(out));
        range(what, o - hw::outputLabelDbm(out), -0.9, 0.1);
    }

    std::printf("---- distortion (Technical Data) ----\n");
    for (double f : {50.0, 1000.0, 7500.0}) {
        auto e = makeEngine(hw::BalLimit::Single);
        const auto t = runTone(e, f, dbmToDbfsPeakIn(-25), 1.2);
        char what[64]; std::snprintf(what, sizeof what, "THD below verge %.0f c/s (%%, RCA < 0.6)", f);
        range(what, t.thd, 0.05, 0.6);
    }
    {
        auto e = makeEngine(hw::BalLimit::Single);
        const double low = runTone(e, 30, dbmToDbfsPeakIn(-25), 1.5, 0.5).thd;
        range("THD below verge 30 c/s, INPUT -24 (%, RCA < 1.2)", low, 0.3, 1.2);
        // INPUT at +14: the input transformer T1 (ahead of the attenuator) now
        // carries +13 dBm, and its core adds low-frequency distortion.
        auto h = makeEngine(hw::BalLimit::Single, 1, 20);
        const double hot = runTone(h, 30, dbmToDbfsPeakIn(13), 1.5, 0.5).thd;
        range("THD below verge 30 c/s, INPUT +14 (%, RCA < 1.2)", hot, low + 0.2, 1.2);
        auto k = makeEngine(hw::BalLimit::Single, 1, 20);
        range("THD below verge 1 kc/s, INPUT +14 (%, RCA < 0.6)", runTone(k, 1000, dbmToDbfsPeakIn(13), 1.0).thd, 0.05, 0.6);
    }
    const double in10 = inputForGr(10), in15 = inputForGr(15);
    std::printf("     input for 10 dB GR: %.2f dBm, for 15 dB GR: %.2f dBm\n", in10, in15);
    for (double f : {100.0, 1000.0, 5000.0}) {
        auto e = makeEngine(hw::BalLimit::Single);
        char what[64]; std::snprintf(what, sizeof what, "THD at 10 dB GR, %.0f c/s (%%, RCA < 0.75)", f);
        range(what, runTone(e, f, dbmToDbfsPeakIn(in10), 1.5).thd, 0.05, 0.75);
        auto e2 = makeEngine(hw::BalLimit::Single);
        std::snprintf(what, sizeof what, "THD at 15 dB GR, %.0f c/s (%%, RCA < 1.0)", f);
        range(what, runTone(e2, f, dbmToDbfsPeakIn(in15), 1.5).thd, 0.05, 1.0);
    }
    {
        auto e = makeEngine(hw::BalLimit::Off);
        const auto t = runTone(e, 1000, dbmToDbfsPeakIn(-24 + 9), 0.5);
        const double out = peakToDbmOut(t.amplitude);
        std::printf("     limiting OFF, +9 dB over verge: %.2f dBm, THD %.2f %%\n", out, t.thd);
        range("maximum output limiting OFF (dBm, RCA 38.5)", out, 37.5, 39.0);
        range("THD at maximum output (%)", t.thd, 0.5, 5.0);
        // B+ sag of the choke-input supply on sustained full output.
        auto f = makeEngine(hw::BalLimit::Off);
        const double fresh = peakToDbmOut(runTone(f, 1000, dbmToDbfsPeakIn(-24 + 12), 0.06, 0.02).amplitude);
        const double held = peakToDbmOut(runTone(f, 1000, dbmToDbfsPeakIn(-24 + 12), 2.0, 0.1).amplitude);
        range("supply sag: overload level after 2 s re first 60 ms (dB)", held - fresh, -0.8, -0.1);
    }

    std::printf("---- Fig. 5 frequency response ----\n");
    auto response = [](double inDbm, double f) {
        auto e = makeEngine(hw::BalLimit::Single);
        auto r = makeEngine(hw::BalLimit::Single);
        const double ref = runTone(r, 1000, dbmToDbfsPeakIn(inDbm), 1.5).amplitude;
        const double a = runTone(e, f, dbmToDbfsPeakIn(inDbm), f < 50 ? 3.0 : 1.5, f < 50 ? 0.5 : 0.2).amplitude;
        return 20.0 * std::log10(a / ref);
    };
    const double nl[][3] = {{20, -0.65, -0.15}, {30, -0.4, -0.05}, {100, -0.1, 0.05}, {10000, -0.25, 0.1},
                            {15000, -0.6, 0.0}, {20000, -2.6, -1.4}};
    for (const auto& p : nl) {
        char what[64]; std::snprintf(what, sizeof what, "no limiting, %.0f c/s (dB)", p[0]);
        range(what, response(-34, p[0]), p[1], p[2]);
    }
    const double gr10[][3] = {{20, 0.25, 1.2}, {30, 0.0, 0.7}, {100, -0.15, 0.25}, {10000, -0.9, -0.2},
                              {15000, -1.8, -0.8}};
    for (const auto& p : gr10) {
        char what[64]; std::snprintf(what, sizeof what, "10 dB gain reduction, %.0f c/s (dB)", p[0]);
        range(what, response(in10, p[0]), p[1], p[2]);
    }

    std::printf("---- time constants ----\n");
    {
        // Step from 10 dB below verge to 10 dB above; GR sampled every 0.1 ms.
        auto e = makeEngine(hw::BalLimit::Single);
        runTone(e, 1000, dbmToDbfsPeakIn(-34), 0.3);
        std::vector<float> buf(12);
        const double stepMs = 1000.0 * static_cast<double>(buf.size()) / fs;
        std::vector<double> gr;
        double ph = 0;
        for (int k = 0; k < 2000; ++k) {
            for (auto& v : buf) { v = dbmToDbfsPeakIn(in10) * static_cast<float>(std::sin(ph)); ph += 2 * pi * 1000 / fs; }
            e.process(buf.data(), nullptr, buf.size());
            gr.push_back(e.gainReductionDb());
        }
        const double final = gr.back();
        double t90 = 0;
        for (std::size_t k = 0; k < gr.size(); ++k) if (gr[k] >= 0.9 * final) { t90 = stepMs * (double) (k + 1); break; }
        range("attack: time to 90 % of final GR (ms)", t90, 0.2, 4.0);
        // Release: remove the signal.
        std::fill(buf.begin(), buf.end(), 0.0f);
        const double g0 = e.gainReductionDb();
        double tau = 0;
        for (int k = 0; k < 40000; ++k) {
            std::fill(buf.begin(), buf.end(), 0.0f);
            e.process(buf.data(), nullptr, buf.size());
            if (e.gainReductionDb() <= g0 / std::exp(1.0)) { tau = stepMs * k; break; }
        }
        range("release time constant SINGLE (ms, RCA 330)", tau, 270, 400);
    }
    auto recovery = [](hw::BalLimit mode, double burst) {
        auto e = makeEngine(mode);
        runTone(e, 1000, dbmToDbfsPeakIn(-14), burst, 0.005);
        std::vector<float> buf(96, 0.0f);
        for (int k = 0; k < 20000; ++k) {
            std::fill(buf.begin(), buf.end(), 0.0f);
            e.process(buf.data(), nullptr, buf.size());
            if (e.gainReductionDb() < 1.0f) return k * 0.001;
        }
        return 20.0;
    };
    const double sLong = recovery(hw::BalLimit::Single, 3.0), dLong = recovery(hw::BalLimit::Dual, 3.0);
    const double sShort = recovery(hw::BalLimit::Single, 0.02), dShort = recovery(hw::BalLimit::Dual, 0.02);
    std::printf("     recovery to 1 dB: single %.2fs/%.2fs, dual %.2fs/%.2fs (3 s tone / 20 ms burst)\n", sLong, sShort, dLong, dShort);
    range("DUAL after sustained peaks: recovery / SINGLE recovery", dLong / sLong, 2.5, 10.0);
    range("DUAL after a single short peak: recovery / SINGLE (Fig. 7)", dShort / sShort, 0.8, 1.35);

    std::printf("---- noise and thump ----\n");
    {
        BA6AEngine e; e.prepare(fs);
        Controls c; c.inputIndex = 0; c.outputIndex = 20; c.balLimit = hw::BalLimit::Single;
        e.setControls(c); e.reset();
        std::vector<float> buf(9600, 0.0f);
        double sum = 0; std::size_t count = 0;
        for (int k = 0; k < 20; ++k) {
            std::fill(buf.begin(), buf.end(), 0.0f);
            e.process(buf.data(), nullptr, buf.size());
            if (k >= 5) for (float v : buf) { sum += v * v; ++count; }
        }
        const double noiseDbm = 20 * std::log10(std::sqrt(sum / count) * std::sqrt(2.0)) + hw::outputDbmAtZeroDbfs;
        range("output noise, controls at maximum (dBm, RCA -54)", noiseDbm, -56, -52);
    }
    auto thumpRatio = [](float balA, float balB, double stepDb) {
        BA6AEngine e; e.prepare(fs);
        Controls c; c.inputIndex = 20; c.outputIndex = 20; c.balLimit = hw::BalLimit::Single;
        c.residualNoise = false; c.balanceA = balA; c.balanceB = balB;
        e.setControls(c); e.reset();
        // 400 ms below verge, then a step to `stepDb` above verge. A 4-pole
        // 150 c/s low-pass separates the low-frequency thump from the tone.
        std::vector<float> buf(96);
        double ph = 0, lp[4] = {0, 0, 0, 0}, thump = 0, sig = 0;
        const double a = 1.0 - std::exp(-2 * pi * 150 / fs);
        for (int k = 0; k < 15000; ++k) {
            const double t = k * 0.001;
            const float amp = dbmToDbfsPeakIn(t < 0.4 ? -30 : -24 + stepDb);
            for (auto& v : buf) { v = amp * static_cast<float>(std::sin(ph)); ph += 2 * pi * 3000 / fs; }
            e.process(buf.data(), nullptr, buf.size());
            for (float v : buf) {
                double x = v; for (double& s : lp) { s += a * (x - s); x = s; }
                if (t > 0.4) thump = std::max(thump, std::abs(x));
                if (t > 1.0) sig = std::max(sig, static_cast<double>(std::abs(v)));
            }
        }
        return 20 * std::log10(sig / std::max(thump, 1e-12));
    };
    const double th5 = thumpRatio(0, 0, 5), th10 = thumpRatio(0, 0, 10);
    range("signal-to-thump, step 5 dB above verge (dB, RCA 21 min / 26 avg)", th5, 21, 40);
    range("signal-to-thump, step 10 dB above verge (dB, RCA 21 min / 26 avg)", th10, 21, 34);
    range("signal-to-thump with BALANCE A misadjusted (dB)", thumpRatio(0.6f, 0, 10), 0, th10 - 8);
    range("signal-to-thump with BALANCE B misadjusted (dB)", thumpRatio(0, 0.6f, 10), 0, th10 - 6);

    std::printf("---- meter and balancing procedure ----\n");
    auto meterAfter = [](Controls c, double seconds, float inputPeak = 0.0f) {
        BA6AEngine e; e.prepare(fs); c.residualNoise = false; e.setControls(c); e.reset();
        runTone(e, 1000, inputPeak, seconds, 0.05);
        return std::pair<double, double>(e.meterTarget(), e.gainReductionDb());
    };
    Controls m; m.meter = hw::MeterSwitch::GR; m.balLimit = hw::BalLimit::Off;
    range("GR, BAL-LIM OFF: full scale", meterAfter(m, 0.3).first, 0.98, 1.02);
    m.balLimit = hw::BalLimit::A;
    range("GR, BAL-LIM at A: ~5 dB (meter fraction)", meterAfter(m, 0.3).first, panel::meterFractionForGr(6), panel::meterFractionForGr(4));
    m.balLimit = hw::BalLimit::B;
    range("GR, BAL-LIM at B: ~10 dB (meter fraction)", meterAfter(m, 0.3).first, panel::meterFractionForGr(11), panel::meterFractionForGr(9));
    m.balLimit = hw::BalLimit::Off;
    for (auto sw : {hw::MeterSwitch::V1, hw::MeterSwitch::V2, hw::MeterSwitch::V3, hw::MeterSwitch::V4, hw::MeterSwitch::V5, hw::MeterSwitch::V6}) {
        m.meter = sw;
        const double f = meterAfter(m, 0.3).first;
        check(f > panel::meterCheckBandLow && f < panel::meterCheckBandHigh, "V1..V6 within CHECK band", f, panel::meterCheckBandLow, panel::meterCheckBandHigh);
    }
    m.meter = hw::MeterSwitch::Fil;
    range("FIL at HEATER mark", meterAfter(m, 0.1).first, panel::meterHeaterMark - 0.01, panel::meterHeaterMark + 0.01);
    m.meter = hw::MeterSwitch::GR; m.zeroAdj = 0.5;
    range("ZERO ADJ moves full-scale reading", meterAfter(m, 0.2).first, 1.03, 1.07);
    m.zeroAdj = 0;
    m.inputIndex = 0;
    for (auto pos : {hw::BalLimit::A, hw::BalLimit::B}) {
        m.balLimit = pos; m.meter = hw::MeterSwitch::FineBal;
        range(pos == hw::BalLimit::A ? "FINE BAL at A, aligned: no deflection" : "FINE BAL at B, aligned: no deflection",
              meterAfter(m, 0.5).first, 0, 0.08);
        m.balanceA = 0.4;
        range(pos == hw::BalLimit::A ? "COARSE BAL at A, BALANCE A off: deflection" : "COARSE BAL at B, BALANCE A off: deflection",
              (m.meter = hw::MeterSwitch::CoarseBal, meterAfter(m, 0.5).first), 0.2, 1.06);
        m.balanceA = 0; m.balanceB = 0.4;
        range(pos == hw::BalLimit::A ? "COARSE BAL at A, BALANCE B off: deflection" : "COARSE BAL at B, BALANCE B off: deflection",
              meterAfter(m, 0.5).first, pos == hw::BalLimit::A ? 0.02 : 0.2, 1.06);
        m.balanceB = 0;
    }
    {
        // The manual's iterative procedure (steps 5-8) must converge from a
        // misaligned pair: minimise FINE BAL with BALANCE A at position A,
        // then with BALANCE B at position B, and repeat.
        Controls p; p.inputIndex = 0; p.meter = hw::MeterSwitch::FineBal;
        p.balanceA = 0.55f; p.balanceB = -0.45f;
        // The technician reads COARSE BAL until the needle comes off the end
        // stop on FINE BAL (FINE is ten times as sensitive).
        auto reading = [&](Controls c) {
            c.meter = hw::MeterSwitch::FineBal;
            const double fine = meterAfter(c, 0.35).first;
            if (fine < 1.0) return 0.1 * fine;
            c.meter = hw::MeterSwitch::CoarseBal;
            return meterAfter(c, 0.35).first;
        };
        for (int pass = 0; pass < 5; ++pass) {
            for (int which = 0; which < 2; ++which) {
                p.balLimit = which == 0 ? hw::BalLimit::A : hw::BalLimit::B;
                float& trim = which == 0 ? p.balanceA : p.balanceB;
                double lo = -1, hi = 1;
                for (int it = 0; it < 18; ++it) {
                    const double m1 = lo + (hi - lo) * 0.382, m2 = lo + (hi - lo) * 0.618;
                    trim = static_cast<float>(m1); const double r1 = reading(p);
                    trim = static_cast<float>(m2); const double r2 = reading(p);
                    (r1 < r2 ? hi : lo) = (r1 < r2 ? m2 : m1);
                }
                trim = static_cast<float>(0.5 * (lo + hi));
            }
        }
        std::printf("     procedure result: BALANCE A %.3f, BALANCE B %.3f\n", p.balanceA, p.balanceB);
        range("balancing procedure converges: |BALANCE A| residual", std::abs(p.balanceA), 0, 0.05);
        range("balancing procedure converges: |BALANCE B| residual", std::abs(p.balanceB), 0, 0.05);
    }
    {
        // Positions A/B disconnect the line: no program and no 60 c/s at the output.
        auto e = makeEngine(hw::BalLimit::A);
        range("BAL-LIM A: output disconnected (peak)", runTone(e, 1000, 0.1f, 0.3).amplitude, 0, 1e-5);
    }

    std::printf("---- power, OFF positions, link, robustness ----\n");
    {
        auto e = makeEngine(hw::BalLimit::Single, 0, 20);
        range("INPUT OFF: silence (peak)", runTone(e, 1000, 0.5f, 0.3).amplitude, 0, 1e-5);
        auto o = makeEngine(hw::BalLimit::Single, 20, 0);
        range("OUTPUT OFF: silence (peak)", runTone(o, 1000, 0.5f, 0.3).amplitude, 0, 1e-6);
        BA6AEngine p; p.prepare(fs);
        Controls c; c.residualNoise = false; c.balLimit = hw::BalLimit::Single;
        p.setControls(c); p.reset();
        const double on = runTone(p, 1000, 0.05f, 0.3).amplitude;
        c.power = false; p.setControls(c);
        range("power OFF: output after 2 s (dB re on)", 20 * std::log10(runTone(p, 1000, 0.05f, 2.0).amplitude / on + 1e-12), -300, -60);
        runTone(p, 1000, 0.05f, 40.0, 0.01);   // let the heaters cool down
        c.power = true; p.setControls(c);
        const double early = runTone(p, 1000, 0.05f, 1.0).amplitude;
        range("warm-up: level 1 s after power ON (dB re warm)", 20 * std::log10(early / on + 1e-12), -300, -30);
        const double later = runTone(p, 1000, 0.05f, 14.0).amplitude;
        range("warm-up: level 15 s after power ON (dB re warm)", 20 * std::log10(later / on), -0.3, 0.3);
    }
    for (bool link : {true, false}) {
        BA6AEngine e; e.prepare(fs);
        Controls c; c.balLimit = hw::BalLimit::Single; c.inputIndex = 20; c.outputIndex = 20;
        c.residualNoise = false; c.stereoLink = link;
        e.setControls(c); e.reset();
        runTone(e, 1000, dbmToDbfsPeakIn(-34), 0.5, 0.2, true, dbmToDbfsPeakIn(-34));
        // Right channel 10 dB below verge, left 14 dB above: compare right gain.
        BA6AEngine q; q.prepare(fs); q.setControls(c); q.reset();
        std::vector<float> l(48000), r(48000);
        for (std::size_t i = 0; i < l.size(); ++i) {
            const double ph = 2 * pi * 1000 * static_cast<double>(i) / fs;
            l[i] = dbmToDbfsPeakIn(-10) * static_cast<float>(std::sin(ph));
            r[i] = dbmToDbfsPeakIn(-34) * static_cast<float>(std::sin(ph));
        }
        q.process(l.data(), r.data(), l.size());
        double pk = 0; for (std::size_t i = 24000; i < r.size(); ++i) pk = std::max(pk, static_cast<double>(std::abs(r[i])));
        const double gain = peakToDbmOut(pk) + 34;
        if (link) range("stereo link: quiet channel follows loud channel GR (dB gain)", gain, 30, 45);
        else range("dual mono: quiet channel keeps full gain (dB gain)", gain, 53.3, 54.3);
    }
    {
        BA6AEngine e; e.prepare(fs);
        Controls c; c.balLimit = hw::BalLimit::Dual; c.inputIndex = 20; c.outputIndex = 20;
        e.setControls(c); e.reset();
        std::vector<float> l(4096), r(4096);
        std::uint32_t s = 1; bool finite = true; double mx = 0;
        for (int k = 0; k < 200; ++k) {
            for (std::size_t i = 0; i < l.size(); ++i) {
                s = s * 1664525u + 1013904223u;
                const float v = (static_cast<float>(s >> 8) / 8388608.0f - 1.0f) * (k % 3 == 0 ? 8.0f : 0.5f);
                l[i] = v; r[i] = -v;
            }
            if (k == 50) l[7] = 1e30f;
            if (k == 60) r[9] = std::numeric_limits<float>::quiet_NaN();
            e.process(l.data(), r.data(), l.size());
            for (std::size_t i = 0; i < l.size(); ++i) {
                finite = finite && std::isfinite(l[i]) && std::isfinite(r[i]);
                mx = std::max(mx, static_cast<double>(std::abs(l[i])));
            }
        }
        check(finite, "extreme input: output always finite", finite ? 1 : 0, 1, 1);
        range("extreme input: output bounded (peak dBFS)", 20 * std::log10(mx), -60, 12);
    }

    std::printf(failures ? "\n%d FAILURE(S)\n" : "\nALL TESTS PASSED\n", failures);
    return failures ? 1 : 0;
}
