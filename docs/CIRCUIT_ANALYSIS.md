# RCA BA-6A — circuit analysis and modelling notes

**Status: structural model, NOT component-by-component circuit solver; no measured null-test against a BA-6A.**
Source documents: RCA *Type BA-6A Limiting Amplifier* technical manual (uploaded
`ba6a.pdf`), especially printed pp. 1–5 and figures 3–8; RCA sales sheet
(`RCA BA_6A.pdf`), full corrected 2004 schematic (`BA6A schematic corrected.jpeg`),
and simplified circuit (`rca_ba-6a_limiting-amp_sch.pdf`).

## Signal graph from RCA documents

```
600 / 150 Ω transformer T1 -> R1 input 20×2 dB stepped pad
   -> balanced, push-pull pair V1/V2 (6SK7 remote-cutoff tubes; variable µ)
   -> interstage transformer T2 -> push-pull pair V3/V4 (6J7)
   -> push-pull pair V5/V6 (6V6) -> T3 output transformer
   -> R36 output 20×1 dB stepped bridge-T pad + R35 fine trim
   -> 600 Ω output

Feedback: V5/V6 output -> V7 (6H6 biased full-wave rectifier)
   -> BAL-LIMIT S1 -> R11/C1 rapid network
                 \-> R37/C18 memory network in DUAL
   -> DC grid bias of the V1/V2 pair
```

The input grid and screen/cathode balance arrangements compensate tube-pair
mismatch and reduce *thumps* on modulation changes. RCA used fixed negative
feedback around the two later amplifying sections. 6H6 cathodes have a fixed
positive bias so the detector starts conducting only above the verge of
limiting (manual printed pp. 2–3 and figs. 3/8).

## RCA calibration and exact documented measurements

| Parameter | Original published data | Software implementation |
|---|---:|---|
| First pair | 2 × 6SK7 | one nonlinear gain-controlled aggregate |
| Next pairs | 2 × 6J7; 2 × 6V6 | two simplified tube saturating stages |
| Rectifier | 6H6 full-wave | absolute peak detector |
| Original input / output | 600/150 Ω input; 600 Ω output | conventional floating-point plugin I/O |
| Below threshold gain | nominal 54 dB at 1 kHz | virtual level calibration instead of +54 dB electrical gain |
| Verge | about -24 dBm at min input attenuation | **chosen** -26 dBFS internal reference |
| Strong limiting slope | 20 dB input → ~1.9–2 dB output | feedback loop target gain ≈ 9.526:1 |
| SINGLE timing | 600 µs attack; 330 ms release | exponentially smoothed detector |
| DUAL additional network | 900 ms charge; 2 s discharge | parallel slow follower driven by the fast peak-held envelope (max fast,slow) |
| Input control | 20 steps, 2 dB/contact | OFF + 20 contacts, -38..0 dB |
| Output control | 20 steps, 1 dB/contact | 0..20 = -20..0 dB |
| THD from manual | <0.6% 50–15,000 Hz below limiting | **not yet calibrated** |

RCA's dynamic data are **time constants**, not the time to reach a specific
percentage of full attenuation under a specified step. The simplified DUAL
memory (maximum of two follower outputs) reflects the engineering description
of fast recovery after isolated peaks, slow recovery after sustained peaks;
it is not an exact nodal solution of the R37/C18 parallel RC arrangement.

### Why digital -26 dBFS is NOT the hardware threshold

The original reference is electrical **dBm into an impedance**, whereas a DAW
handles **dBFS without a fixed line-level calibration**. The code's -26 dBFS
threshold is an explicit design choice for a usable plugin at typical DAW
levels, *not* a literal conversion of -24 dBm. Likewise, plugin gain does
not impose the hardware's 54 dB voltage gain, which would clip digital mixes.
Use the input stepped pad to control how far audio drives the model.

### Variable-µ and harmonic structure

The original 6SK7 gain is controlled by a continuously variable negative grid
bias, with nonlinear tube transconductance and a true balanced push-pull
configuration. The prototype approximates this with three cascaded
`std::tanh` transfer functions and minor asymmetric offsets; that yields
program-level-dependent harmonics but **does not replicate** SPICE curves,
transformer hysteresis, operating-point drift, measured frequency-dependent
intermodulation or hardware-matched noise. The -17 Hz DC blocking/high-pass
and 19 kHz first-order low-pass are software stability/color choices, not
values derived by calculating T1/T2/T3 winding response.

### Detector feedback and stereo

The detector is driven from the output stage BEFORE the output pad, following
RCA's 6H6 feedback architecture. A negative-feedback law varies the variable
µ stage's attenuation above a detector threshold. Digital feedback slope
requires calibration of the open-loop gain: the chosen loop factor of 9.526
was selected to target the manual's 20:1.9 steady-state mapping, but test
results do **not** establish a verified 20:1.9 transfer curve across level and
frequency. The 35 dB reduction cap is a deliberate stabilizer.

One BA-6A was **mono**, not stereo. A stereo instance of the plugin models a
matched pair linked by the maximum detected peak to prevent balance wandering.
This is an extension for DAW convenience and should not be mistaken for an
original RCA stereo design.

## Front panel and internal service controls

- INPUT 20 detents of 2 dB (plus OFF); BAL-LIMIT A/B/OFF/SINGLE/DUAL; OUTPUT
  20 detents of 1 dB; METER selection V1–V6, FIL, COARSE BAL, FINE BAL, GR.
- The six service trims — BAL A, BAL B, ZERO ADJ, FINE OUTPUT, HUM ADJ and
  HEATER ADJ — are on the expanded service hatch; ON/OFF is the seventh.
- BAL A and BAL B intentionally mute plugin program output as they do in the
  original alignment procedure; no 60 Hz calibration tone is generated.
- Meter V1–V6, FIL, COARSE and FINE are displayed as selector positions; the
  vintage meter animation presently renders **gain reduction only**. The
  diagnostic needle readings have not been electrically modelled. ZERO ADJ
  is stored in the session but not mapped to exact galvanometer currents.
- HUM service adjustment is stored but currently has no audible effect, because
  simulated mains hum is intentionally not introduced. Heater adjustment is
  a small tube-shape bias, not a power supply/thermal model.
- Studio plugin hosts can bypass processing using their standard plugin bypass;
  switching power OFF instead mutes the signal like the original unit.

## Required measurements for greater hardware fidelity

1. Record steady-state 1 kHz transfer curves for stepped INPUT, output pad
   max and limiting OFF/SINGLE/DUAL, across -45 to +10 dBm equivalent.
2. Capture attack and full recovery at 1 kHz for isolated bursts, 50 ms and
   sustained 3 s tones, both modes; derive matched detector RC curves.
3. Characterize harmonic spectra (THD+N, HD2/3) from 50 Hz to 15 kHz at
   0/5/10/15 dB gain reduction and at multiple source impedances.
4. Capture transformer response and nonlinear magnetic behavior, including
   LF saturation, high-frequency phase changes, and intermodulation.
5. Fit remote-cutoff 6SK7 tube characteristic equations and matched pair
   balancing response; consider 2× or 4× oversampling with antialias filters.
6. Implement galvanometer needle ballistics and genuine V1–V6, FIL and
   balance metering from equivalent circuit state.

Only after this calibration can the software plausibly be labelled a
**high-fidelity hardware emulation** rather than an **inspired prototype**.
