#!/usr/bin/env python3
"""The menu music is a stream the console can loop, and a file it can check.

gui/menuaudio.c reads buildtools/audio/menu-music.mp3 from the card (the zip
puts it at MENU_MUSIC_MP3_PATH) and loops it by seeking straight to an MP3
frame: frame n must start at n * MENU_MUSIC_FRAME_BYTES, so every frame is
MPEG-1 Layer III at the header's rate and bitrate, unpadded, with no tag or
Xing frame in front. The loop must fit inside the decoded stream. The console
plays the file only when its length and CRC-32 are the header's, and gives
libmad MAD_BUFFER_GUARD zero bytes past the last frame.
"""

import re
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HEADER = (ROOT / "cube/swiss/source/gui/menu_music_mp3.h").read_text()
AUDIO = (ROOT / "cube/swiss/source/gui/menuaudio.c").read_text()
MP3 = (ROOT / "buildtools/audio/menu-music.mp3").read_bytes()
PACKAGE = (ROOT / "buildtools/sd_package.sh").read_text()
FRAME_SAMPLES = 1152
BITRATES = [0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320]
RATES = [44100, 48000, 32000]


def define(name: str) -> int:
    return int(re.search(rf"#define {name}\s+(\d+)", HEADER).group(1))




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
        length = define("MENU_MUSIC_MP3_LEN")
        self.assertEqual(len(MP3), length, "menu-music.mp3 is MENU_MUSIC_MP3_LEN bytes")
        sizes = frame_sizes(MP3)
        self.assertEqual(set(sizes), {define("MENU_MUSIC_FRAME_BYTES")}, "every frame the same size")
        self.assertEqual(sum(sizes), length, "the stream ends on a frame")

    def test_the_console_checks_the_file(self):
        crc = int(re.search(r"#define MENU_MUSIC_MP3_CRC32\s+0x([0-9A-F]{8})u", HEADER).group(1), 16)
        self.assertEqual(zlib.crc32(MP3), crc, "MENU_MUSIC_MP3_CRC32 is menu-music.mp3's")
        # Length and CRC decide whether the file plays; anything else leaves the menus silent.
        self.assertIn("MENU_MUSIC_MP3_LEN", AUDIO)
        self.assertIn("MENU_MUSIC_MP3_CRC32", AUDIO)
        self.assertIn("MAD_BUFFER_GUARD", AUDIO)
        self.assertIn("!musicData", AUDIO, "start_music gives up without the file")

    def test_the_zip_carries_the_file(self):
        path = re.search(r'#define MENU_MUSIC_MP3_PATH\s+"([^"]+)"', HEADER).group(1)
        self.assertEqual(path, "swiss/indigo/menu-music.mp3")
        self.assertIn("buildtools/audio/menu-music.mp3", PACKAGE)
        self.assertIn(path, PACKAGE)

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
