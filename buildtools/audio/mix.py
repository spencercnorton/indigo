"""Up in the Sky (Memoraphile, CC0) -> Indigo's menu music.

The track with the console's sound and nothing added: the GameCube's 32 kHz,
a gentle top end, and a round trip through DSP-ADPCM, the compression
GameCube games stream their music in. Layout: the intro [0, A) plays once,
then [A, B) loops; B is 32 bars after A on grid.json's beat.

usage: mix.py source.wav grid.json out.wav   (build dspadpcm.c first:
       cc -O2 -o dspadpcm dspadpcm.c -lm, next to this script)
"""
import json
import os
import subprocess
import sys

import numpy as np
import soundfile as sf
from scipy import signal

SOURCE, GRID, OUT = sys.argv[1:4]
DSPADPCM = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dspadpcm")
SR, OSR = 48000, 32000
x, sr = sf.read(SOURCE)
assert sr == SR
barlen = 4 * json.load(open(GRID))["beat"]
A = 29.99  # the bar where the track picks up after its intro
B_nom = A + 32 * barlen

# Refine B so the waveform after B lines up with the waveform after A (+-40 ms).
mono = x.mean(axis=1)
w, a0 = int(0.2 * SR), int(A * SR)
ref = mono[a0:a0 + w]
best = (-2.0, 0)
for d in range(-int(0.04 * SR), int(0.04 * SR), 4):
    seg = mono[int(B_nom * SR) + d:int(B_nom * SR) + d + w]
    c = float(ref @ seg / np.sqrt((ref @ ref) * (seg @ seg)))
    if c > best[0]:
        best = (c, d)
B = B_nom + best[1] / SR

# Both loop points sit on the resampler's grid (48 kHz -> 32 kHz is 2/3), so
# each lands on a whole output sample.
a_i, L = 3 * round(A * SR / 3), 3 * round(B * SR / 3)
mix = x[:L].copy()
# The body's last 0.35 s fades from the original [B-X, B) into [A-X, A).
X = int(0.35 * SR)
fade = 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, X))[:, None]
mix[L - X:L] = x[L - X:L] * (1 - fade) + x[a_i - X:a_i] * fade

# The resampler and filter see the loop's start past its end, as a looping
# player does, then the file is cut back to the loop's end.
out_len = L * 2 // 3
mix = np.concatenate([mix, mix[a_i:a_i + SR]])
mix = signal.resample_poly(mix, 2, 3, axis=0)
mix = signal.sosfilt(signal.butter(4, 13000, fs=OSR, output="sos"), mix, axis=0)[:out_len]
mix *= 0.89 / np.max(np.abs(mix))

# GameCube DSP-ADPCM round trip per channel.
out = np.zeros_like(mix)
for c in range(2):
    (mix[:, c] * 32767).astype("<i2").tofile("ch.raw")
    subprocess.run([DSPADPCM, "ch.raw", "ch2.raw"], check=True)
    out[:, c] = np.fromfile("ch2.raw", "<i2").astype(float) / 32767
os.remove("ch.raw")
os.remove("ch2.raw")

# Last, in the file's own samples: the loop's final 50 ms blend into what comes
# just before its start, so the wrap is continuous whatever came before.
ls = a_i * 2 // 3
X2 = int(0.05 * OSR)
f2 = (0.5 - 0.5 * np.cos(np.linspace(0, np.pi, X2)))[:, None]
out[-X2:] = out[-X2:] * (1 - f2) + out[ls - X2:ls] * f2
# One second of the loop's start follows its end in the file: the player
# jumps before it plays, but the MP3 encoder shapes the loop's last frames
# knowing what comes next.
end = len(out)
sf.write(OUT, np.concatenate([out, out[ls:ls + OSR]]), OSR, subtype="PCM_16")
json.dump({"rate": OSR, "loop_start": ls, "loop_end": end}, open(OUT + ".json", "w"))
print(f"{OUT}: loop {ls / OSR:.2f}..{end / OSR:.2f} s, then 1 s of the loop's start")
