#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" == 0 ]]; then
    echo "Usage: bash run-linux-gui-tests.sh <test executable> [arguments...]" >&2
    exit 1
fi

# Run inside xvfb-run so the window manager and tests share the same display.
openbox --sm-disable > openbox.log 2>&1 &
wm_pid=$!
cleanup() {
    kill "$wm_pid" 2>/dev/null || true
    wait "$wm_pid" 2>/dev/null || true
}
trap cleanup EXIT

ready=false
for ((attempt = 0; attempt < 100; ++attempt)); do
    if ! kill -0 "$wm_pid" 2>/dev/null; then
        echo "Openbox exited before becoming ready." >&2
        cat openbox.log >&2
        exit 1
    fi
    if xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -Eq 'window id # 0x[1-9a-fA-F][0-9a-fA-F]*'; then
        ready=true
        break
    fi
    sleep 0.1
done

if [[ "$ready" != true ]]; then
    echo "Timed out waiting for Openbox to initialize." >&2
    cat openbox.log >&2
    exit 1
fi

"$@"