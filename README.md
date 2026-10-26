# RCABA6A — RCA BA-6A limiter emulation

C++17/JUCE 8 plug-in emulating the RCA BA-6A with a photorealistic, animated vintage front panel. The DSP is an engineering approximation; it has **not** been calibrated against measured original hardware.

## Compile on GitHub, not on your Mac

1. Upload every file/folder of this repository to `mermau/RCABA6A`, on the `main` branch (include the **hidden** `.github/workflows/build-macos.yml` file and the binary files in `assets/`).
2. Open **Actions → Build RCA BA-6A (Apple Silicon) → Run workflow**, or simply push to `main`.
3. After a successful run, open that workflow run and download the separate `rcaba6a-vst3-macos-arm64` and `rcaba6a-au-macos-arm64` artifacts (plus optional Standalone).
4. Each artifact contains an inner ZIP, made by macOS `ditto`, which preserves the `.vst3` / `.component` bundle structure. Extract the inner ZIP and copy its bundle.

### Install on Apple Silicon

In Finder choose **Go → Go to Folder** and paste these paths:

- VST3: `~/Library/Audio/Plug-Ins/VST3/rcaba6a.vst3`
- Audio Unit: `~/Library/Audio/Plug-Ins/Components/rcaba6a.component`

If the destination folders do not exist, create them. In Terminal, after copying:

```bash
xattr -cr "$HOME/Library/Audio/Plug-Ins/VST3/rcaba6a.vst3"
xattr -cr "$HOME/Library/Audio/Plug-Ins/Components/rcaba6a.component"
```

Rescan plug-ins or restart the DAW. This project uses ad-hoc code signing for personal testing, not Apple Developer ID notarization. Only remove quarantine from files you have built yourself and trust. macOS/AU hosts may require a reboot or AU rescan.

## Build configuration

GitHub-hosted `macos-15` runner (arm64); macOS 14 image retirement on 2026-11-02; JUCE `8.0.15` pinned via `FetchContent`, Ninja, Release, minimum deployment target macOS 11.0. Formats: VST3, AU, Standalone. Explicit `COPY_PLUGIN_AFTER_BUILD FALSE`. This workflow downloads tools **only on the GitHub runner** and leaves the local Mac untouched.

The plugin artifacts are **`rcaba6a.vst3`**, **`rcaba6a.component`** and **`rcaba6a.app`**. Host display name: **RCA BA-6A**.

## Source tree

- `Source/PluginProcessor.*` — JUCE audio processor, parameters and state.
- `Source/PluginEditor.*` — fixed-shading procedural knobs with separately animated markers, fixed bezels.
- `Source/DSP/BA6AEngine.*` — independent C++ DSP engine.
- `assets/panel.jpg`, `assets/knob_*.png` — GUI assets embedded with `juce_add_binary_data`.
- `Tests/` — regression tests (including optional Python/Pillow visual test).
- `docs/` — circuit analysis and validation notes.
- `.github/workflows/build-macos.yml` — complete Apple Silicon CI packaging/signing.

### Local build and CI limitations

Without downloading JUCE, native standalone DSP and asset tests can be built with `cmake -S . -B build -G Ninja -DRCABA6A_BUILD_PLUGIN=OFF && cmake --build build && ctest --test-dir build --output-on-failure` on a C++17 host. The **full VST3/AU build requires JUCE and a macOS runner**, and is exercised by the GitHub workflow rather than by the native DSP tests. AU runs only in Audio Unit-compatible DAWs. **AAX/Pro Tools is not included.**

## Panel control detents (0.5.0)

- INPUT: -24 to +24, every 2 units (25 steps); +24 gives maximum modeled input drive.
- OUTPUT: OFF (0) followed by 2 to 30 every 2 units (16 positions); OFF mutes.
- METER: ten indexed settings, advancing clockwise with increasing index.
- BAL-LIMIT: clockwise B -> A -> OFF -> SINGLE -> DUAL; vertical drag has reduced sensitivity.

The vintage reference artwork contains several inconsistent engraved digits on the INPUT ring (notably -12/-16); the underlying saved artwork was not altered. The software positions are quantized and anchored to the physical ticks, but fully faithful numeral alignment requires a corrected faceplate master. Old state (v0.4) is migrated on load; existing DAW automation lanes may need inspection because parameter ranges changed.

## v0.6.0 — accurate rings and app icon

INPUT starts at 0, OUTPUT at OFF, BAL-LIMIT at OFF, and METER at IN. The three photographic dial markings are overpainted with matching brushed-metal static calibrated dial graphics from `PluginEditor.cpp`; engraved label and moving pointer share `ControlCalibration.h`. The macOS runner generates a standalone icon from `assets/generate_app_icon.swift` using built-in AppKit before configuring CMake. Existing saved presets keep their settings through state migration.
