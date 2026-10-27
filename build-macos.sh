#!/usr/bin/env bash
# Optional local build on a Mac with Xcode command line tools, CMake and Ninja.
# Not needed when building through GitHub Actions.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
command -v cmake >/dev/null
command -v ninja >/dev/null
cmake -S "$ROOT" -B "$ROOT/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DRCABA6A_BUILD_PLUGIN=ON -DBUILD_TESTING=ON
cmake --build "$ROOT/build" --config Release \
    --target RCABA6A_VST3 RCABA6A_AU RCABA6A_Standalone BA6A_Tests BA6A_PluginTests \
    --parallel 4
ctest --test-dir "$ROOT/build" -C Release --output-on-failure
