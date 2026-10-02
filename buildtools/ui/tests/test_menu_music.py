#!/usr/bin/env python3
"""The bundled menu music is a stream the console can loop.

gui/menuaudio.c streams menu_music.mp3 (menu_music_mp3.h embeds it) and loops
it by seeking straight to
an MP3 frame: frame n must start at n * MENU_MUSIC_FRAME_BYTES, so every
frame is MPEG-1 Layer III at the header's rate and bitrate, unpadded, with no
tag or Xing frame in front. The loop must fit inside the decoded stream, and
libmad must find MAD_BUFFER_GUARD zero bytes past the last frame.
"""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HEADER = (ROOT / "cube/swiss/source/gui/menu_music_mp3.h").read_text()
MP3 = (ROOT / "cube/swiss/source/gui/menu_music.mp3").read_bytes()
AUDIO = (ROOT / "cube/swiss/source/gui/menuaudio.c").read_text()
FRAME_SAMPLES = 1152
GUARD = 8
BITRATES = [0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320]
RATES = [44100, 48000, 32000]


def define(name: str) -> int:
    return int(re.search(rf"#define {name}\s+(\d+)", HEADER).group(1))


def stream() -> bytes:
    """What the array holds: the file, then the zeros #embed's suffix adds."""
    embed = re.search(r'#embed "menu_music.mp3" limit\(MENU_MUSIC_MP3_LEN \+ 1\) suffix\(([^)]*)\)', HEADER)
    zeros = [int(v) for v in embed.group(1).split(",") if v.strip()]
    return MP3[:define("MENU_MUSIC_MP3_LEN") + 1] + bytes(zeros)


def frame_sizes(data: bytes) -> list[int]:
    sizes, at = [], 0
    while at < len(data):
        h = int.from_bytes(data[at:at + 4], "big")
        if (h >> 21) & 0x7FF != 0x7FF or (h >> 19) & 3 != 3 or (h >> 17) & 3 != 1:
            raise AssertionError(f"no MPEG-1 Layer III frame at byte {at}")
        size = 144 * BITRATES[(h >> 12) & 15] * 1000 // RATES[(h >> 10) & 3] + ((h >> 9) & 1)
        if RATES[(h >> 10) & 3] != define("MENU_MUSIC_RATE"):
            raise AssertionError(f"frame at byte {at} is not at MENU_MUSIC_RATE")
        sizes.append(size)
        at += size
    return sizes


class MenuMusic(unittest.TestCase):
    def test_stream_is_seekable_by_frame(self):
        data = stream()
        length = define("MENU_MUSIC_MP3_LEN")
        self.assertEqual(data[length:], bytes(GUARD), "MAD_BUFFER_GUARD zeros must follow the stream")
        sizes = frame_sizes(data[:length])
        self.assertEqual(set(sizes), {define("MENU_MUSIC_FRAME_BYTES")}, "every frame the same size")
        self.assertEqual(sum(sizes), length, "the stream ends on a frame")

    def test_loop_fits_the_decoded_stream(self):
        frames = define("MENU_MUSIC_MP3_LEN") // define("MENU_MUSIC_FRAME_BYTES")
        start, end = define("MENU_MUSIC_LOOP_START"), define("MENU_MUSIC_LOOP_END")
        self.assertLess(0, start)
        self.assertLess(start, end)
        self.assertLessEqual(end, frames * FRAME_SAMPLES)
        # The seek lands MUSIC_PRIME_FRAMES early; there must be frames to prime from.
        prime = int(re.search(r"#define MUSIC_PRIME_FRAMES\s+(\d+)", AUDIO).group(1))
        self.assertGreaterEqual(start // FRAME_SAMPLES, prime)

    def test_blocks_are_whole_aesnd_chunks(self):
        # AESND pads a short last DSP_STREAMBUFFER_SIZE (1152-byte) chunk with silence.
        self.assertIn("#define MUSIC_BLOCK_BYTES   (MUSIC_FRAME_SAMPLES * 2 * sizeof(s16))", AUDIO)

    def test_only_the_stream_is_bundled(self):
        # The old player decoded the whole piece into RAM; the stream player must not.
        self.assertNotIn("decode_mp3", AUDIO)
        self.assertNotIn("PCM_INITIAL_SAMPLES", AUDIO)


if __name__ == "__main__":
    unittest.main()
