# Menu music

`cube/swiss/source/gui/menu_music_mp3.h` is generated from these files. They
are the source of the music: change the recipe here and generate it again,
never edit the header.

## Where it comes from

"Up in the Sky" by Memoraphile, from
[OpenGameArt](https://opengameart.org/content/up-in-the-sky), released under
[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) (also offered
under CC-BY 4.0 and OGA-BY 3.0). The file used:

| File | SHA-256 |
| --- | --- |
| `Memoraphile - Up in the Sky.wav` (48 kHz stereo, 197.6 s) | `d216756ca636f91c57cf9ee3148c8912c4368c0271153fbae43c6646dfc304f5` |

## What the arrangement adds

- **Structure:** the first 30 s (the track's quiet intro) play once, then 32
  bars from 30.0 s to 114.7 s loop. The last 0.35 s of the loop is
  crossfaded into what precedes its start, so the wrap is seamless.
- **Glass chimes:** FM bells composed on the track's own beat grid (90.66 BPM)
  and each bar's chord tones (`grid.py` finds both), with a ping-pong echo and
  a plate reverb. A single shimmer every other bar during the intro.
- **The console's sound:** 32 kHz, the GameCube's native rate, and a round
  trip through DSP-ADPCM (`dspadpcm.c`), the 4-bit compression GameCube games
  stream their music in.
- `mix.py` also makes two other versions: `light` (fewer, quieter chimes) and
  `memory` (slowed 8 % with a hall reverb).

## Making it again

Needs Python 3 with NumPy, SciPy and soundfile, FFmpeg with libmp3lame, and a
C compiler.

```bash
cd buildtools/audio
cc -O2 -o dspadpcm dspadpcm.c -lm
python3 grid.py "Memoraphile - Up in the Sky.wav" grid.json   # optional: grid.json is committed
python3 mix.py "Memoraphile - Up in the Sky.wav" grid.json crystal menu.wav
python3 make_header.py menu.wav menu.wav.json
```

`mix.py` is deterministic: the same source makes the same `menu.wav`.
`make_header.py` encodes it at 96 kbps constant bitrate with no tag, checks
that every frame is the same size, measures the encoder and decoder delay
(1105 samples), and writes the loop points in decoded samples. The console
loops by seeking to a frame, so those three properties are what
`buildtools/ui/tests/test_menu_music.py` checks.
