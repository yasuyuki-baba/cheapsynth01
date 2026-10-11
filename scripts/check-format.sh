#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

formatter="${CLANG_FORMAT:-clang-format}"
required_version="21.1.7"
version="$("$formatter" --version)"
if [[ ! "$version" =~ version\ ${required_version//./\.}([[:space:]]|$) ]]; then
    echo "Expected clang-format $required_version, got: $version" >&2
    exit 1
fi

options=(--dry-run --Werror --style=file --fail-on-incomplete-format)
if [[ "${1:-}" == "--fix" && "$#" == 1 ]]; then
    options=(-i --style=file --fail-on-incomplete-format)
elif [[ "$#" != 0 ]]; then
    echo "Usage: bash scripts/check-format.sh [--fix]" >&2
    exit 1
fi

list_source_files() {
    if command -v rg >/dev/null 2>&1; then
        rg --files -0 Source CheapSynth01 Tests
    else
        # GitHub's macOS runners do not necessarily have ripgrep installed.
        git ls-files -z -- Source CheapSynth01 Tests
    fi
}

files=()
while IFS= read -r -d '' file; do
    case "$file" in
        Tests/legacy-juce/*|CheapSynth01/resources/*) continue ;;
        *.cpp|*.h) files+=("$file") ;;
    esac
done < <(list_source_files)

if [[ "${#files[@]}" == 0 ]]; then
    echo "No tracked C++ files found." >&2
    exit 1
fi

echo "Checking ${#files[@]} C++ files with $version"
"$formatter" "${options[@]}" "${files[@]}"
