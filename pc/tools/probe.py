#!/usr/bin/env python3
"""Quick probe: render GM programs with fluidsynth and print analysis features."""

import os, subprocess, tempfile, math
import numpy as np
import soundfile as sf
import mido
import librosa
from scipy.signal import resample_poly

SF2 = "/usr/share/sounds/sf2/default-GM.sf2"
SR = 22050
OUT = "/home/dima/work/midi1/tmp"


def make_midi(path, program, notes, dur=1.2, gap=0.4, channel=0, velocities=None):
    mid = mido.MidiFile(type=0, ticks_per_beat=480)
    tr = mido.MidiTrack()
    mid.tracks.append(tr)
    tempo = mido.bpm2tempo(120)
    tr.append(mido.MetaMessage("set_tempo", tempo=tempo, time=0))
    if channel != 9:
        tr.append(
            mido.Message("program_change", program=program, channel=channel, time=0)
        )
    tps = 480 * 120 / 60.0  # ticks per second at 120bpm
    starts = []
    now = 0.0
    for i, n in enumerate(notes):
        v = 100 if velocities is None else velocities[i]
        starts.append(now)
        dticks = int(round(dur * tps))
        gticks = int(round(gap * tps))
        tr.append(mido.Message("note_on", note=n, velocity=v, channel=channel, time=0))
        tr.append(
            mido.Message("note_off", note=n, velocity=0, channel=channel, time=dticks)
        )
        now += dur + gap
        # next event delta handled by mido absolute? no: mido uses delta. We already put note_off delta.
        # add a dummy meta to advance gap
        tr.append(mido.MetaMessage("text", text="", time=gticks))
    mid.save(path)
    return starts


def render(program, notes, tag, channel=0, velocities=None):
    midp = os.path.join(OUT, f"{tag}.mid")
    wavp = os.path.join(OUT, f"{tag}.wav")
    starts = make_midi(midp, program, notes, channel=channel, velocities=velocities)
    cmd = [
        "fluidsynth",
        "-ni",
        "-F",
        wavp,
        "-r",
        str(SR),
        "-g",
        "0.7",
        "-o",
        "synth.reverb.active=0",
        "-o",
        "synth.chorus.active=0",
        "-o",
        "synth.polyphony=64",
        SF2,
        midp,
    ]
    subprocess.run(
        cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )
    x, sr = sf.read(wavp, always_2d=True)
    x = x.mean(axis=1)
    return x, sr, starts


def seg(x, sr, t, dur):
    a = int(t * sr)
    b = min(len(x), int((t + dur) * sr))
    return x[a:b]


def features(x, sr):
    if len(x) < 256:
        return {}
    # onset/peak
    env = np.abs(x)
    win = max(1, int(sr * 0.005))
    env = np.convolve(env, np.ones(win) / win, mode="same")
    peak = env.max()
    if peak < 1e-9:
        return {"silent": True}
    # attack time to 90%
    idx90 = np.argmax(env >= 0.9 * peak)
    attack = idx90 / sr
    # spectral flatness over middle
    a = int(0.1 * sr)
    b = min(len(x), int(0.5 * sr))
    y = x[a:b]
    if len(y) < 512:
        y = x
    S = np.abs(librosa.stft(y, n_fft=2048, hop_length=512)) + 1e-12
    flat = np.exp(np.mean(np.log(S), axis=0)) / np.mean(S, axis=0)
    flatness = float(np.mean(flat))
    # f0
    try:
        f0, voiced, _ = librosa.pyin(x, fmin=40, fmax=3000, sr=sr, frame_length=2048)
        f0v = f0[~np.isnan(f0)]
        f0med = float(np.median(f0v)) if len(f0v) else 0.0
        f0std = float(np.std(f0v) / max(f0med, 1e-9)) if len(f0v) else 1.0
    except Exception:
        f0med, f0std = 0.0, 1.0
    # harmonic ratio
    if f0med > 0:
        S1 = np.abs(np.fft.rfft(y * np.hanning(len(y))))
        freqs = np.fft.rfftfreq(len(y), 1 / sr)
        tot = np.sum(S1**2) + 1e-12
        harm = 0.0
        for k in range(1, 40):
            f = k * f0med
            if f >= sr / 2:
                break
            m = (freqs > f - 1.5 * f0med * 0.15) & (freqs < f + 1.5 * f0med * 0.15)
            harm += np.sum(S1[m] ** 2)
        hratio = float(harm / tot)
    else:
        hratio = 0.0
    # sustain ratio
    rms = librosa.feature.rms(y=x, frame_length=1024, hop_length=256)[0]
    if len(rms):
        peak_r = rms.max()
        tail = rms[int(len(rms) * 0.7) :]
        sus = float(np.median(tail) / max(peak_r, 1e-9))
    else:
        sus = 0.0
    return dict(
        attack=attack,
        flatness=flatness,
        f0=f0med,
        f0std=f0std,
        hratio=hratio,
        sustain=sus,
        peak=float(peak),
        rms=float(np.sqrt(np.mean(x * x))),
    )


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for prog, name in [
        (0, "piano"),
        (73, "flute"),
        (56, "trumpet"),
        (33, "bass"),
        (4, "epiano"),
        (48, "strings"),
        (80, "lead"),
    ]:
        x, sr, starts = render(prog, [36, 48, 60, 72, 84], f"p{prog:03d}")
        print(f"\n{prog:3d} {name:8s} rendered {len(x) / sr:.1f}s")
        for n, t in zip([36, 48, 60, 72, 84], starts):
            s = seg(x, sr, t, 1.2)
            f = features(s, sr)
            print(
                f"  note {n:3d} t={t:5.2f} "
                + " ".join(f"{k}={v:.3f}" for k, v in f.items() if isinstance(v, float))
            )
    # drums
    x, sr, starts = render(0, [35, 36, 38, 42, 46, 49, 51, 57], "drums", channel=9)
    print("\ndrums")
    for n, t in zip([35, 36, 38, 42, 46, 49, 51, 57], starts):
        s = seg(x, sr, t, 0.6)
        f = features(s, sr)
        print(
            f"  note {n:3d} t={t:5.2f} "
            + " ".join(f"{k}={v:.3f}" for k, v in f.items() if isinstance(v, float))
        )
