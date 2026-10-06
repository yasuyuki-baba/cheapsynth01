#!/usr/bin/env python3
"""Comparative behavioral probes for legacy and experimental Original VCF cores.

This dependency-free harness transcribes the C++ core equations. It is a
reproducible model probe, not a plugin render or a hardware measurement.
"""
import argparse
import json
import math
import os
import platform
import time


def legacy_coeff(fs, cutoff, resonance):
    f = min(max(cutoff, 20.0), fs * .45) / fs
    w = 2 * math.pi * f
    sw, cw = math.sin(w), math.cos(w)
    q = .5 + resonance * 4.5
    alpha = sw / (2 * q)
    norm = 1 / (1 + alpha)
    b0 = math.sin(w * .5) ** 2 * norm
    return b0, 2 * b0, b0, -2 * cw * norm, (1 - alpha) * norm


class Legacy:
    def __init__(self, fs, cutoff, resonance):
        self.fs, self.cutoff, self.resonance = fs, cutoff, resonance
        self.z1 = self.z2 = 0.0

    def tick(self, x, cutoff=None):
        # Match IG02610's input safety clamp and empirical final output bound.
        x = min(max(x, -1.0), 1.0)
        c = self.cutoff if cutoff is None else cutoff
        b0, b1, b2, a1, a2 = legacy_coeff(self.fs, c, self.resonance)
        y = b0 * x + self.z1
        self.z1 = b1 * x - a1 * y + self.z2
        self.z2 = b2 * x - a2 * y
        # Phase 2 documents the legacy cubic coloration as an empirical
        # harmonic proxy. Apply its low-resonance branch for core comparisons.
        amount = min(max(self.resonance / .4, 0.0), 1.0)
        distortion = y ** 3 * .05 * amount * .3
        blend = self.resonance * .6
        y = y * (1 - blend) + distortion * blend
        return min(max(y, -1.5), 1.5)


class Experimental:
    def __init__(self, fs, cutoff, resonance):
        self.fs, self.cutoff, self.resonance = fs, cutoff, resonance
        self.ic1 = self.ic2 = 0.0

    def tick(self, x, cutoff=None):
        c = self.cutoff if cutoff is None else cutoff
        c = min(max(c, 20.0), self.fs * .45)
        g = math.tan(math.pi * c / self.fs)
        damping = 1.25 + (0.12 - 1.25) * min(max(self.resonance, 0), 1)
        feedback = damping * math.tanh(self.ic2)
        v1 = (self.ic1 + g * (x - feedback - self.ic2)) / (1 + g * (g + damping))
        v2 = self.ic2 + g * v1
        self.ic1, self.ic2 = 2 * v1 - self.ic1, 2 * v2 - self.ic2
        return min(max(v2, -1.5), 1.5)


def measure_sine(core_type, fs, cutoff, resonance, hz, amplitude=.005, count=8192):
    core = core_type(fs, cutoff, resonance)
    start = count // 2
    sin_sum = cos_sum = input_sin = input_cos = 0.0
    for i in range(count):
        phase = 2 * math.pi * hz * i / fs
        x = amplitude * math.sin(phase)
        y = core.tick(x)
        if i >= start:
            sin_sum += y * math.sin(phase)
            cos_sum += y * math.cos(phase)
            input_sin += x * math.sin(phase)
            input_cos += x * math.cos(phase)
    out_amp = 2 * math.hypot(sin_sum, cos_sum) / (count - start)
    in_amp = 2 * math.hypot(input_sin, input_cos) / (count - start)
    return 20 * math.log10(max(out_amp / max(in_amp, 1e-30), 1e-15))


def harmonics(core_type, fs, cutoff, resonance, amplitude=.7, fundamental=440, count=32768):
    core = core_type(fs, cutoff, resonance)
    sums = [[0.0, 0.0] for _ in range(8)]
    begin = count // 2
    for i in range(count):
        phase = 2 * math.pi * fundamental * i / fs
        y = core.tick(amplitude * math.sin(phase))
        if i >= begin:
            for h, pair in enumerate(sums, 1):
                pair[0] += y * math.sin(h * phase)
                pair[1] += y * math.cos(h * phase)
    n = count - begin
    amps = [2 * math.hypot(s, c) / n for s, c in sums]
    fundamental_amp = max(amps[0], 1e-30)
    return [20 * math.log10(max(a / fundamental_amp, 1e-15)) for a in amps]


def bench(core_type, fs, cutoff, resonance, modulated, seconds):
    core = core_type(fs, cutoff, resonance)
    count = max(1, int(fs * seconds))
    start = time.perf_counter()
    for i in range(count):
        c = cutoff * (1 + .25 * math.sin(i * 2 * math.pi / 4096)) if modulated else cutoff
        core.tick(.25 * math.sin(i * 2 * math.pi * 220 / fs), c)
    elapsed = time.perf_counter() - start
    return {"samples": count, "elapsed_seconds": elapsed,
            "ns_per_sample": elapsed * 1e9 / count,
            "realtime_factor": count / fs / elapsed}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seconds", type=float, default=.05)
    parser.add_argument("--output", default="artifacts/dsp/original_vcf_ab.json")
    args = parser.parse_args()
    rates = (44100, 48000, 96000)
    models = {"legacy_biquad": Legacy, "experimental_tpt_nonlinear_feedback": Experimental}
    result = {
        "format": "original-vcf-ab-characterization-v1",
        "environment": {"platform": platform.platform(), "python": platform.python_version()},
        "scope": "Python transcription of filter core equations; not plugin execution or hardware data",
        "benchmark_seconds_requested": args.seconds,
        "models": {},
    }
    for name, core in models.items():
        responses, distortion, benchmarks, modulation = [], [], [], []
        for host_rate in rates:
            # Phase 2 documented host-rate probes; also probe the production 4x core rate.
            for fs, oversampling in ((host_rate, 1), (host_rate * 4, 4)):
                for cutoff in (80, 1000, 10000):
                    for resonance in (.1, .8):
                        for hz in (20, 40, 80, 100, 440, 1000, 5000, 10000, 18000):
                            if hz < fs * .45:
                                responses.append({"host_rate": host_rate, "core_rate": fs,
                                                  "oversampling": oversampling, "cutoff_hz": cutoff,
                                                  "resonance": resonance, "frequency_hz": hz,
                                                  "gain_db": measure_sine(core, fs, cutoff, resonance, hz)})
                distortion.append({"host_rate": host_rate, "core_rate": fs, "cutoff_hz": 1000,
                                   "resonance": .8, "input_peak": .7, "fundamental_hz": 440,
                                   "harmonic_db_relative_to_fundamental": harmonics(core, fs, 1000, .8)})
                for resonance in (.1, .8):
                    for modulated in (False, True):
                        item = bench(core, fs, 1000, resonance, modulated, args.seconds)
                        item.update({"host_rate": host_rate, "core_rate": fs,
                                     "oversampling": oversampling, "resonance": resonance,
                                     "cutoff_mode": "modulated" if modulated else "static"})
                        benchmarks.append(item)
                # Exercise rapid full-range cutoff jumps, including endpoint clamps.
                probe = core(fs, 1000, .8)
                peak = 0.0
                finite = True
                for i in range(20000):
                    try:
                        y = probe.tick(4 * math.sin(i * .17), 20 if i & 1 else 20000)
                    except (OverflowError, ValueError):
                        finite = False
                        peak = None
                        break
                    finite &= math.isfinite(y)
                    if not finite:
                        peak = None
                        break
                    peak = max(peak, abs(y))
                modulation.append({"host_rate": host_rate, "core_rate": fs,
                                   "oversampling": oversampling, "fast_cutoff_sweep_finite": finite,
                                   "peak_output": peak})
        result["models"][name] = {"frequency_response": responses, "harmonics": distortion,
                                  "cpu_microbenchmark": benchmarks,
                                  "cutoff_modulation_stress": modulation}
    out = os.path.abspath(args.output)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        json.dump(result, f, indent=2, sort_keys=True)
        f.write("\n")
    print("Wrote", out)


if __name__ == "__main__":
    main()
