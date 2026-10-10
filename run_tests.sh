#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
cmake -S . -B build-core -DBUILD_PLUGINS=OFF -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-core --config Debug --parallel 2
if [ "$#" -eq 0 ]; then
    ctest --test-dir build-core -C Debug --output-on-failure
else
    executable="build-core/Tests/CheapSynth01Tests"
    if [ -f "build-core/Tests/Debug/CheapSynth01Tests.exe" ]; then
        executable="build-core/Tests/Debug/CheapSynth01Tests.exe"
    fi
    "$executable" "$@"
fi
