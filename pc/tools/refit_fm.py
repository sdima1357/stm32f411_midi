#!/usr/bin/env python3
"""Refit FM parameters and clamp envelope attacks in the generated table.

This is a fast post-processing pass used while tuning the soundbank.  It reads
the cached fluidsynth renders, re-fits the 2-op FM parameters with musically
sensible ratio choices, and rewrites src/data/instruments.c in place.
"""

import os
import re
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_soundbank as B

CAPS = {
    "piano": (1, 60),
    "chromatic": (1, 60),
    "organ": (5, 40),
    "guitar": (1, 50),
    "bass": (2, 60),
    "strings": (30, 600),
    "ensemble": (30, 600),
    "brass": (10, 150),
    "reed": (10, 150),
    "pipe": (10, 150),
    "synth_lead": (2, 80),
    "synth_pad": (50, 1000),
    "synth_fx": (5, 500),
    "ethnic": (2, 200),
    "percussive": (1, 5),
    "sfx": (1, 5),
}

RATIOS = {
    "bass": [1],
    "brass": [1],
    "reed": [1],
    "pipe": [1],
    "synth_lead": [1, 2, 3, 4],
    "organ": [1, 2, 3, 4],
    "chromatic": [1, 2, 3, 4],
    "synth_fx": [1, 2, 3, 4],
}

LINE_RE = re.compile(r"/\*\s*(\d+)\s+(\w+)\s+(\w+)\s*\*/\s*\{\s*(.*?)\s*\},?$")


def fit_with_ratios(x, sr, f0, ratios):
    target = B.harmonic_amplitudes(x, sr, f0, kmax=24)
    if target.max() <= 1e-12:
        return None
    target = target / target.max()
    even = float(np.sum(target[1::2] ** 2))
    odd = float(np.sum(target[0::2] ** 2))
    if 2 in ratios and even < 0.05 * (odd + 1e-12):
        ratios = [2]
    best = (1e9, 1, 0.0)
    for ratio in ratios:
        for idx in np.arange(0.0, B.FM_INDEX_MAX + 1e-9, B.FM_INDEX_STEP):
            amps = B.fm_harmonics(ratio, float(idx), kmax=24)
            w = target + 0.02
            w[0] *= 4.0
            err = float(
                np.sum(w * (amps - target) ** 2) / (np.sum(w * target**2) + 1e-12)
            )
            if err < best[0]:
                best = (err, ratio, float(idx))
    return best


def main():
    path = os.path.join(B.ROOT, "src", "data", "instruments.c")
    lines = open(path).read().splitlines()
    out = []
    changed = 0
    for line in lines:
        m = LINE_RE.search(line)
        if not m:
            out.append(line)
            continue
        program = int(m.group(1))
        family = m.group(2)
        method = m.group(3)
        fields = [f.strip() for f in m.group(4).split(",")]
        if len(fields) != 16:
            out.append(line)
            continue

        attack = int(fields[7].rstrip("u"))
        lo, hi = CAPS.get(family, (1, 1000))
        new_attack = max(lo, min(hi, attack))
        fields[7] = str(new_attack)

        if method == "FM":
            x, starts = B.render_program(program)
            # C4 is the third rendered note.
            seg = B.segment(x, B.SR, starts[2], 0.5)
            f0, _ = B.estimate_f0(seg, B.SR)
            if f0 <= 0:
                f0 = 261.625565
            best = fit_with_ratios(seg, B.SR, f0, RATIOS.get(family, [1]))
            if best is not None:
                err, ratio, index = best
                fields[12] = str(int(round(ratio * 16)))
                fields[13] = str(int(round(index * 16)))
                changed += 1
                print(
                    f"prog {program:3d} {family:10s} ratio={ratio} idx={index:.2f} err={err:.3f}"
                )

        # Preserve the u suffix on the two 32-bit offset fields.
        fields[5] = fields[5] if fields[5].endswith("u") else fields[5] + "u"
        fields[6] = fields[6] if fields[6].endswith("u") else fields[6] + "u"
        prefix = line[: m.start(4)]
        out.append(prefix + ", ".join(fields) + " },")

    open(path, "w").write("\n".join(out) + "\n")
    print(f"updated {changed} FM instruments")


if __name__ == "__main__":
    main()
