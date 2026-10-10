#!/usr/bin/env bash
# Fetch SDKs for the pinned iPlug2 checkout. No plugin installation occurs here.
set -euo pipefail
cd "$(dirname "$0")/.."
git submodule update --init libs/iPlug2
sdk_dir="$PWD/libs/iPlug2/Dependencies/IPlug"
fetch_sdk() {
    local repo="$1" revision="$2" destination="$3"
    if [ -d "$destination/.git" ]; then
        local actual
        actual=$(git -C "$destination" rev-parse HEAD)
        local expected
        expected=$(git -C "$destination" rev-parse "$revision^{commit}")
        if [ "$actual" = "$expected" ]; then return; fi
        echo "Existing SDK at $destination differs from the pinned revision; choose a clean checkout." >&2
        exit 1
    fi
    if [ -d "$destination" ]; then
        # The pinned iPlug2 tree ships only an informational placeholder here.
        local unexpected
        unexpected=$(find "$destination" -mindepth 1 -maxdepth 1 ! -name README.md ! -name readme.txt -print -quit)
        if [ -n "$unexpected" ]; then echo "SDK directory is already populated: $destination" >&2;exit 1;fi
        rmdir "$destination" 2>/dev/null || {
            rm -f "$destination/README.md" "$destination/readme.txt"
            rmdir "$destination"
        }
    fi
    git init "$destination"
    git -C "$destination" remote add origin "$repo"
    git -C "$destination" fetch --depth 1 origin "$revision"
    git -C "$destination" checkout --detach FETCH_HEAD
    # Restore iPlug2's tracked placeholder notice after the SDK is populated.
    local component
    component=$(basename "$destination")
    for notice in README.md readme.txt; do
        if git -C libs/iPlug2 cat-file -e "HEAD:Dependencies/IPlug/$component/$notice" 2>/dev/null; then
            git -C libs/iPlug2 show "HEAD:Dependencies/IPlug/$component/$notice" > "$destination/$notice"
        fi
    done
}
fetch_sdk https://github.com/steinbergmedia/vst3sdk.git 9fad9770f2ae8542ab1a548a68c1ad1ac690abe0 "$sdk_dir/VST3_SDK"
git -C "$sdk_dir/VST3_SDK" submodule update --init base pluginterfaces public.sdk cmake
fetch_sdk https://github.com/free-audio/clap.git a47f6badb49d948fd009998f28309cdab78979c9 "$sdk_dir/CLAP_SDK"
fetch_sdk https://github.com/free-audio/clap-helpers.git 29389e43489b1af44b654ef5a7b4ee0af4bbc13b "$sdk_dir/CLAP_HELPERS"
