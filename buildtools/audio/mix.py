"""Up in the Sky (Memoraphile, CC0) -> Indigo menu mixes with GameCube-era character.

Original additions only: FM glass chimes composed on the track's own beat grid and
per-bar chord tones, ping-pong echo, plate/hall reverb, console-native 32 kHz and a
GameCube DSP-ADPCM round trip. Layout: intro [0, A) plays once, body [A, B) loops.
usage: mix.py source.wav grid.json <light|crystal|memory> out.wav  (build dspadpcm.c first:
       cc -O2 -o dspadpcm dspadpcm.c -lm, next to this script)
"""
import os
import json
import subprocess
import sys

import numpy as np
import soundfile as sf
from scipy import signal

SOURCE, GRID, VARIANT, OUT = sys.argv[1:5]
DSPADPCM = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dspadpcm")
SR = 48000
rng = np.random.default_rng(7)
x, sr = sf.read(SOURCE)
assert sr == SR
g = json.load(open(GRID))
beat = g["beat"]
bars = np.array(g["bars"])
barlen = 4 * beat
A = 29.99
B_nom = A + 32 * barlen

# Refine B so the waveform after B lines up with the waveform after A (+-40 ms).
mono = x.mean(axis=1)
w = int(0.2 * SR)
a0 = int(A * SR)
ref = mono[a0:a0 + w]
best = (-2, 0)
for d in range(-int(0.04 * SR), int(0.04 * SR), 4):
    b0 = int(B_nom * SR) + d
    seg = mono[b0:b0 + w]
    c = float(ref @ seg / np.sqrt((ref @ ref) * (seg @ seg)))
    if c > best[0]:
        best = (c, d)
B = B_nom + best[1] / SR
print(f"A={A:.3f} B={B:.4f} (nominal {B_nom:.4f}) seam corr {best[0]:.3f}")

a_i, b_i = int(round(A * SR)), int(round(B * SR))
L = b_i  # file = [0, B)
base = x[:L].copy()
# Pre-bake the seam: the body's last X fades from the original [B-X, B) into [A-X, A).
X = int(0.35 * SR)
fade = 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, X))[:, None]
base[L - X:L] = x[L - X:L] * (1 - fade) + x[a_i - X:a_i] * fade

# ---- chimes -----------------------------------------------------------------
def bell(freq, dur, vel):
    t = np.arange(int(dur * SR)) / SR
    idx = 2.4 * np.exp(-t / 0.16) + 0.3
    tine = np.sin(2 * np.pi * freq * t + idx * np.sin(2 * np.pi * freq * t))
    sparkle = 0.22 * np.sin(2 * np.pi * freq * 3.5 * t + 1.2 * np.exp(-t / 0.05) * np.sin(2 * np.pi * freq * 7 * t))
    shimmer = 0.18 * np.sin(2 * np.pi * freq * 2.0 * t) * np.exp(-t / 0.6)
    env = (1 - np.exp(-t / 0.004)) * np.exp(-t / 1.7)
    return vel * env * (0.75 * tine + sparkle * np.exp(-t / 0.35) + shimmer)


def midi(n):
    return 440.0 * 2 ** ((n - 69) / 12)


chord_at = {round(c["t"], 2): c["pcs"] for c in g["chords"]}


def tones(bar_t):
    pcs = chord_at.get(round(bar_t, 2)) or [6, 10, 1, 4]  # F# A# C# E
    notes = sorted({78 + ((pc - 78) % 12) for pc in pcs})  # F#5..F6 register
    return notes


chimes = np.zeros((L + 8 * SR, 2))


def place(t, freq, vel, pan):
    s = bell(freq, 3.2, vel)
    i = int(t * SR)
    chimes[i:i + len(s), 0] += s * np.cos(pan * np.pi / 2)
    chimes[i:i + len(s), 1] += s * np.sin(pan * np.pi / 2)


eighth = beat / 2
patterns = [
    [(0, 0), (3, 1), (4, 2), (6, 3)],
    [(0, 3), (2, 2), (5, 1)],
    [(1, 1), (4, 2), (7, 4)],
    [(0, 4), (3, 2)],
]
light = VARIANT == "light"
for k, t0 in enumerate(bars):
    if t0 >= B - 0.05:
        break
    nt = tones(t0)
    ext = nt + [n + 12 for n in nt]
    if t0 < A - 0.05:  # intro: a single shimmer every other bar
        if k % 2 == 0:
            place(t0, midi(ext[min(2, len(ext) - 1)] + 12), 0.30, 0.5 + 0.3 * (-1) ** (k // 2))
        continue
    phrase = int(round((t0 - A) / barlen)) % 4
    if light and phrase in (1, 3):
        continue
    for slot, deg in patterns[phrase]:
        if light and slot % 2:
            continue
        n = ext[min(deg, len(ext) - 1)]
        vel = (0.55 if light else 0.62) * (1.0 if slot == 0 else 0.8) * rng.uniform(0.85, 1.0)
        place(t0 + slot * eighth, midi(n), vel, 0.5 + 0.35 * np.sin(1.7 * k + slot))

# Ping-pong echo, dotted eighth, damped.
d = int(0.75 * beat * SR)
echo = np.zeros_like(chimes)
fb, wet = 0.38, 0.30
lp = signal.butter(1, 5000, fs=SR, output="sos")
tap = chimes.copy()
for n in range(1, 6):
    tap = signal.sosfilt(lp, tap, axis=0)
    shifted = np.zeros_like(tap)
    shifted[d * n:] = tap[:len(tap) - d * n]
    ch = n % 2
    echo[:, ch] += shifted[:, 0 if ch else 1] * (fb ** (n - 1)) * wet
chimes = chimes + echo


def reverb(sig, t60, predelay, wet, bright=True, seed=1):
    r = np.random.default_rng(seed)
    n = int(t60 * SR)
    t = np.arange(n) / SR
    ir = r.standard_normal((n, 2)) * np.exp(-6.9 * t / t60)[:, None]
    hp = signal.butter(2, 250 if bright else 120, "highpass", fs=SR, output="sos")
    lpf = signal.butter(2, 9000 if bright else 6000, fs=SR, output="sos")
    ir = signal.sosfilt(lpf, signal.sosfilt(hp, ir, axis=0), axis=0)
    ir /= np.sqrt((ir ** 2).sum(axis=0))
    pd = int(predelay * SR)
    out = np.stack([signal.fftconvolve(sig[:, c], ir[:, c])[:len(sig)] for c in range(2)], axis=1)
    out = np.concatenate([np.zeros((pd, 2)), out])[:len(sig)]
    return sig * (1 - wet * 0.5) + out * wet * 2.2


chimes = reverb(chimes, 2.6, 0.012, 0.42, seed=3)

# Loop the chime layer: tails past B wrap onto the body's start.
tail = chimes[L:].copy()
chimes = chimes[:L]
chimes[a_i:a_i + len(tail)] += tail[:max(0, L - a_i)]

# Level: chimes sit well under the track in the body.
def rms(v):
    return np.sqrt(np.mean(v ** 2))


track_rms = rms(base[a_i:L])
target = track_rms * (10 ** ((-19 if light else -14) / 20))
chimes *= target / (rms(chimes[a_i:L]) + 1e-12)
mix = base + chimes

if VARIANT == "memory":  # slowed + reverb: tape-slow 8%, lush hall
    mix = reverb(mix, 3.8, 0.028, 0.24, bright=False, seed=9)
    rate_num, rate_den = 50, 69  # 48k -> 32k and slowed to 0.92x
    A_out, B_out = A / 0.92, B / 0.92
else:
    rate_num, rate_den = 2, 3
    A_out, B_out = A, B
mix = signal.resample_poly(mix, rate_num, rate_den, axis=0)
OSR = 32000
lpf = signal.butter(4, 11000 if VARIANT == "memory" else 13000, fs=OSR, output="sos")
mix = signal.sosfilt(lpf, mix, axis=0)
if VARIANT == "memory":
    mix = np.tanh(mix * 1.15) / np.tanh(1.15)  # a little tape warmth
mix *= 0.89 / np.max(np.abs(mix))

# GameCube DSP-ADPCM round trip per channel.
out = np.zeros_like(mix)
for c in range(2):
    (mix[:, c] * 32767).astype("<i2").tofile("ch.raw")
    subprocess.run([DSPADPCM, "ch.raw", "ch2.raw"], check=True)
    out[:, c] = np.fromfile("ch2.raw", "<i2").astype(float) / 32767
os.remove("ch.raw")
os.remove("ch2.raw")
sf.write(OUT, out, OSR, subtype="PCM_16")
json.dump({"variant": VARIANT, "rate": OSR, "loop_start": round(A_out * OSR), "loop_end": len(out)},
          open(OUT + ".json", "w"))
print(f"{OUT}: {len(out) / OSR:.1f} s, loop {A_out:.2f}..{len(out) / OSR:.2f} s")
