#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
group=""
if [ "${1:-}" = "--group" ]; then
    group="${2:-}"
    case "$group" in
        audio|dsp|core|numerical|midi|presets|integration|observation) ;;
        ui) echo "Native editor tests have not been migrated yet; no UI coverage is claimed." >&2; exit 2 ;;
        *) echo "Unknown test group: $group" >&2; exit 2 ;;
    esac
    shift 2
    if [ "$#" -ne 0 ]; then
        echo "Use --group on its own, or pass ordinary Google Test arguments." >&2
        exit 2
    fi
fi
cmake -S . -B build-core -DBUILD_PLUGINS=OFF -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-core --config Debug --parallel 2
if [ -n "$group" ]; then
    ctest --test-dir build-core -C Debug --output-on-failure -L "^${group}$" --output-junit test-results.xml
elif [ "$#" -eq 0 ]; then
    ctest --test-dir build-core -C Debug --output-on-failure --output-junit test-results.xml
else
    executable="build-core/Tests/CheapSynth01Tests"
    if [ -f "build-core/Tests/Debug/CheapSynth01Tests.exe" ]; then
        executable="build-core/Tests/Debug/CheapSynth01Tests.exe"
    fi
    "$executable" "$@"
fi
