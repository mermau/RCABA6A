# Test status — updated 2026-10-26

## Executed locally on Linux

- CMake 3.31.6 with C and C++ languages enabled.
- GCC 14.2.0 and Ninja 1.12.1: `BA6A_DSP` static library and
  `BA6A_Tests` compiled and linked.
- `ctest --test-dir build-dsp --output-on-failure`: **1/1 suites passed**;
  currently 16 in-process assertions, including dynamic limiting slope,
  long-tail DUAL memory and stereo sample processing.
- Clang with AddressSanitizer + UndefinedBehaviorSanitizer in a separate
  Debug build: **1/1 suites passed**.

## Full JUCE plugin targets — NOT compiled in this environment

`cmake -S . -B build-vst3 -G Ninja -DBA6A_BUILD_PLUGIN=ON` was attempted.
`FetchContent` failed to download `juce-framework/JUCE`, tag `8.0.15`, with
`Could not resolve host: github.com`. JUCE 8.0.15 and its Linux development
libraries are not available locally. **No VST3 or AU binary is included**;
no plugin-scan or DAW-host validation has been carried out.

`.github/workflows/linux-vst3.yml` is configured to install dependencies,
verify Ninja, check out JUCE 8.0.15, build `RCABA6A_VST3`, run `ctest` and
upload `.vst3`. This configuration has NOT been executed or verified here.
The macOS workflow similarly targets VST3 and AU, but has NOT run here.

## Remaining blockers / not yet validated

- Compile JUCE `PluginProcessor.cpp`, `PluginEditor.cpp` and binary GUI assets
  in an environment with all JUCE 8.0.15 headers/libraries installed.
- Run host scan (e.g. REAPER, pluginval), actual audio signal tests at 44.1,
  48, 96 and 192 kHz, preset recall and DAW automation.
- Verify interface functionality in the VST3/AU editor under multiple DPI
  scaling configurations (especially in Pro Tools via an appropriate format).
- Measure against real RCA BA-6A reference recordings: distortion,
  frequency response, attack/recovery, noise/hum and stereo mismatch.

## GUI update v0.2.0 (2026-10-26)

The editor was changed to render the **chosen second photo-real RCA BA-6A
faceplate**, from `assets/faceplate_master.png`, in place of the original rough
procedural panel. The four knobs are independent RGBA sprites linked to
APVTS via SliderAttachment; the gain-reduction needle is live JUCE vector
rendering; the service panel remains accessible through the UI.

The static composited `assets/preview.png` is NOT a host screenshot and does
not prove runtime rendering. The new `BA6A_GUI_AssetIntegrity` CTest checks
that all image assets exist and have the expected dimensions. The JUCE GUI
compilation, host plugin validation and macOS AU build remain outstanding
while JUCE itself is inaccessible in this environment.

## GUI perspective bug fix — v0.3.0 (2026-10-26)

- Identified the error in `assets/build_photo_assets.py`: the 91px
  circle included stationary metal bezels and some etched scale marks.
- Split source image into a stationary background with the seat/shadow and
  cap-only movable sprites, restricted to 79px radius and softly feathered.
- Updated generated `panel.jpg` and all four knob PNGs.
- Rendered two very different control positions (`preview.png` and
  `preview_alternate.png`) plus `bezel_comparison.png`.
- New `Tests/StaticBezelTests.py` checks alpha = 0 outside 82px on each
  knob and asserts pixel identity of the engraved scale (radii 84..139px)
  across the two previews. Changes in cap pixels are also required.
- Recompiled on Linux, with GCC 14.2 and Ninja: **3/3 CTest suites passed**
  (`BA6A_DSP_Regression`, `BA6A_GUI_AssetIntegrity`,
  `BA6A_StaticBezel_Regression`).

**Important:** a static composited preview and regression checks on PNG
assets are NOT the same as validation in an actual JUCE VST3 or Audio Unit
window. JUCE 8.0.15 compilation and DAW plugin-scanning remain outstanding.

## GitHub Actions package (0.4.0)

macOS arm64 VST3/AU/Standalone build and ad-hoc signed ditto bundles configured in `.github/workflows/build-macos.yml`. macOS 15 is used instead of the soon-to-be-retired macOS 14 label. This workflow must still be run remotely before any full JUCE compilation can be claimed as verified.
