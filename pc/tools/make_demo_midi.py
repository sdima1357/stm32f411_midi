#!/usr/bin/env python3
"""Create a short multi-instrument GM demo MIDI for testing the synth."""

import os
import mido

BPM = 100
PPQ = 480
TPS = PPQ * BPM / 60.0

events = []


def add(t, msg):
    events.append((t, msg))


def note(ch, n, t, dur, vel=100):
    add(t, mido.Message("note_on", channel=ch, note=n, velocity=vel))
    add(t + dur, mido.Message("note_off", channel=ch, note=n, velocity=0))


# Programs
add(0.0, mido.Message("program_change", channel=0, program=0))  # piano
add(0.0, mido.Message("program_change", channel=1, program=33))  # finger bass
add(0.0, mido.Message("program_change", channel=2, program=48))  # strings
add(0.0, mido.Message("program_change", channel=3, program=56))  # trumpet
add(0.0, mido.Message("program_change", channel=4, program=73))  # flute
add(0.0, mido.Message("program_change", channel=5, program=16))  # drawbar organ

# Piano melody
mel = [60, 62, 64, 65, 67, 69, 71, 72, 71, 69, 67, 65, 64, 62, 60]
for i, n in enumerate(mel):
    note(0, n, 0.25 + i * 0.30, 0.28, 95)

# Bass line
for i, n in enumerate([36, 36, 43, 43, 41, 41, 38, 38]):
    note(1, n, i * 0.60, 0.55, 105)

# String chords
chords = [[48, 52, 55], [53, 57, 60], [50, 53, 57], [48, 52, 55]]
for i, ch in enumerate(chords):
    for n in ch:
        note(2, n, i * 1.2, 1.15, 80)

# Trumpet counter melody
for i, n in enumerate([67, 69, 71, 72, 74, 72, 71, 69]):
    note(3, n, 0.6 + i * 0.6, 0.45, 90)

# Flute ornaments
for i, n in enumerate([84, 86, 88, 86, 84, 81, 79, 81]):
    note(4, n, 1.0 + i * 0.5, 0.35, 85)

# Organ pad
for i, n in enumerate([48, 55, 60, 55]):
    note(5, n, i * 1.2, 1.1, 70)

# Drums (channel 9)
for i in range(16):
    t = i * 0.30
    if i % 4 == 0 or i % 4 == 2:
        note(9, 36, t, 0.10, 110)  # kick
    if i % 4 == 1 or i % 4 == 3:
        note(9, 38, t, 0.10, 100)  # snare
    note(9, 42, t, 0.05, 70)  # closed hi-hat
for i in range(4):
    note(9, 49, 4.8 + i * 0.6, 0.20, 80)  # crash

events.sort(key=lambda x: x[0])

mid = mido.MidiFile(type=0, ticks_per_beat=PPQ)
tr = mido.MidiTrack()
mid.tracks.append(tr)
tr.append(mido.MetaMessage("set_tempo", tempo=mido.bpm2tempo(BPM), time=0))

last_tick = 0
for t, msg in events:
    tick = int(round(t * TPS))
    msg.time = max(0, tick - last_tick)
    last_tick = tick
    tr.append(msg)
tr.append(mido.MetaMessage("end_of_track", time=0))

out = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build", "demo.mid"
)
os.makedirs(os.path.dirname(out), exist_ok=True)
mid.save(out)
print("wrote", out, "duration", events[-1][0])
