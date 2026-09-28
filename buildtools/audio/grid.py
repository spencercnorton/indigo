"""Beat grid and per-bar chord tones of the source track (they place mix.py's chimes).

usage: grid.py source.wav grid.json
"""
import json
import sys
import numpy as np
import soundfile as sf
from scipy import signal

x, sr = sf.read(sys.argv[1])
mono = x.mean(axis=1)
hop = 512
f, t, Z = signal.stft(mono, sr, nperseg=2048, noverlap=2048 - hop)
S = np.abs(Z)
fps = sr / hop

# Onset envelope: positive spectral flux, 100 Hz - 8 kHz.
band = (f > 100) & (f < 8000)
L = np.log1p(S[band] * 100)
env = np.maximum(0, np.diff(L, axis=1)).sum(0)
env = np.concatenate([[0], env])
env = (env - env.mean()) / (env.std() + 1e-9)

# Tempo + phase: score a pulse train over the steady section (after 30 s).
start = int(30 * fps)
best = None
for bpm in np.arange(85, 96.01, 0.02):
    period = 60 / bpm * fps
    for phase in np.arange(0, period, 0.5):
        idx = np.round(np.arange(start + phase, len(env) - 1, period)).astype(int)
        score = env[idx].mean()
        if best is None or score > best[0]:
            best = (score, bpm, phase)
score, bpm, phase = best
beat0 = (start + phase) / fps
beat = 60 / bpm
# Walk the grid back to the first beat >= 0.
first = beat0 - np.floor(beat0 / beat) * beat
print(f"tempo {bpm:.2f} bpm, beat {beat:.4f} s, first beat {first:.3f} s, score {score:.2f}")

# Downbeat: which of the 4 beat phases carries the most low-frequency energy onsets.
lowband = (f > 30) & (f < 180)
low = np.maximum(0, np.diff(np.log1p(S[lowband] * 100), axis=1)).sum(0)
low = np.concatenate([[0], low])
beats = np.arange(first, len(mono) / sr, beat)
bi = np.clip(np.round(beats * fps).astype(int), 0, len(low) - 1)
phase_score = [low[bi[k::4]][8:].mean() for k in range(4)]
down = int(np.argmax(phase_score))
bars = beats[down::4]
print("downbeat phase", down, [round(s, 2) for s in phase_score], "bars", len(bars), "first bar", round(bars[0], 3))

# Per-bar chroma -> top pitch classes (chord tones).
pitch = 12 * np.log2(np.maximum(f, 1) / 440) + 69
pc = np.round(pitch).astype(int) % 12
names = "C C# D D# E F F# G G# A A# B".split()
harm = (f > 80) & (f < 1500)
chords = []
for b0, b1 in zip(bars[:-1], bars[1:]):
    a, z = int(b0 * fps), int(b1 * fps)
    seg = S[:, a:z]
    ch = np.array([seg[harm & (pc == k)].sum() for k in range(12)])
    ch = ch / (ch.max() + 1e-9)
    top = [int(k) for k in np.argsort(ch)[::-1][:4] if ch[k] > 0.45]
    chords.append({"t": round(float(b0), 4), "pcs": top, "names": [names[k] for k in top]})
json.dump({"bpm": bpm, "beat": beat, "first_beat": first, "bars": [float(b) for b in bars], "chords": chords},
          open(sys.argv[2], "w"), indent=1)
