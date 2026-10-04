#!/usr/bin/env python3
"""Exercise the real manual MP3 player with immediate and delayed card reads."""

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def extract_function(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


HARNESS = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define DEVICE_HANDLER_SEEK_SET 0
#define PLAYER_PAUSE 0
#define PLAYER_STOP 1
#define PLAYER_NEXT 2
#define PLAYER_PREV 3
enum { BUTTON_B = 1, BUTTON_START = 2, BUTTON_X = 4, BUTTON_Y = 8,
	BUTTON_R = 16, BUTTON_L = 32, BUTTON_RIGHT = 64, BUTTON_LEFT = 128,
	BUTTON_Z = 256 };
#define ALIGN_CENTER 1
#define usleep(value) ((void)0)

typedef struct { int unused; } uiDrawObj_t;
static uiDrawObj_t object;
static struct { int fbWidth; } mode = {640};
#define getVideoMode() (&mode)
static int defaultColor;
static char txtbuffer[2048];
static char progressBar[31];
static int useShuffle;
static int volume = 192;

static uiDrawObj_t *DrawEmptyBox(int left, int top, int right, int bottom)
{ return &object; }
static uiDrawObj_t *DrawStyledLabel(int x, int y, const char *text,
	float scale, int align, int color)
{
	if(y == 210) {
		assert(strlen(text) == 30u);
		memcpy(progressBar, text, 31u);
		unsigned stars = 0u;
		for(unsigned i = 0u; i < 30u; ++i) {
			assert(text[i] == '-' || text[i] == '*');
			stars += text[i] == '*';
		}
		assert(stars == 1u);
	}
	return &object;
}
static uiDrawObj_t *DrawHintLabel(int x, int y, const char *text,
	float scale, int align, int color)
{ return &object; }
static void DrawAddChild(uiDrawObj_t *parent, uiDrawObj_t *child) {}
static uiDrawObj_t *DrawRepublish(uiDrawObj_t *old, uiDrawObj_t *next)
{ return next; }
static void DrawDispose(uiDrawObj_t *draw) {}
static float GetTextScaleToFitInWidth(const char *text, int width)
{ return 1.0f; }
static const char *getRelativeName(const char *name) { return name; }
static void VIDEO_WaitVSync(void) {}

typedef struct file_handle file_handle;
typedef struct {
	s32 (*seekFile)(file_handle *, u32, int);
	s32 (*readFile)(file_handle *, void *, s32);
	s32 (*closeFile)(file_handle *);
} device_t;
struct file_handle { char name[64]; u32 offset, size; device_t *device; };
static bool clipReads;
static s32 seekFile(file_handle *file, u32 offset, int origin)
{
	file->offset = offset;
	return 0;
}
static s32 readFile(file_handle *file, void *dst, s32 count)
{
	if(file->offset > file->size) {
		if(!clipReads) return -1;
		file->offset = file->size;
	}
	u32 bytes = MIN((u32)count, file->size - file->offset);
	file->offset += bytes;
	return (s32)bytes;
}
static s32 closeFile(file_handle *file) { return 0; }
static device_t device = {seekFile, readFile, closeFile};

/* A decoder reads 32 KiB ahead. A FAT seek may sleep before that read;
 * a failed disc read may leave the requested offset unchanged. */
static bool delayedRead, playing, pendingRead, firstReadDone;
static u32 initialOffset;
static void *readerData;
static s32 (*reader)(void *, void *, s32);
static void firstRead(void)
{
	char buffer[32768];
	if(!firstReadDone) {
		((file_handle *)readerData)->offset = initialOffset;
		firstReadDone = true;
	}
	if(reader(readerData, buffer, (s32)sizeof(buffer)) <= 0) playing = false;
}
static void MP3Player_PlayFile(void *data,
	s32 (*callback)(void *, void *, s32), void *filter)
{
	readerData = data;
	reader = callback;
	playing = true;
	if(delayedRead) pendingRead = true;
	else firstRead();
}
static bool MP3Player_IsPlaying(void)
{
	if(pendingRead) {
		pendingRead = false;
		firstRead();
	}
	return playing;
}
static void MP3Player_Stop(void) { playing = pendingRead = false; }
static void MP3Player_Volume(int value) {}

static u32 buttons[256];
static unsigned buttonCount, buttonIndex;
static u32 padsButtonsHeld(void)
{
	assert(buttonIndex < buttonCount);
	return buttons[buttonIndex++];
}

/* PLAYER_FUNCTIONS */

static void testRewind(bool clipping, bool delayed, unsigned forward,
	unsigned rewind, u32 initialReadOffset)
{
	file_handle file = {"track.mp3", 0u, 4u << 20, &device};
	clipReads = clipping;
	delayedRead = delayed;
	playing = pendingRead = false;
	firstReadDone = false;
	initialOffset = initialReadOffset;
	buttonCount = buttonIndex = 0u;
	for(unsigned i = 0u; i < forward; ++i) buttons[buttonCount++] = BUTTON_RIGHT;
	for(unsigned i = 0u; i < rewind; ++i) buttons[buttonCount++] = BUTTON_LEFT;
	buttons[buttonCount++] = BUTTON_B;
	buttons[buttonCount++] = 0u;
	assert(play_mp3(&file, 1, 1) == PLAYER_STOP);
	assert(buttonIndex == buttonCount);
	assert(file.offset <= file.size);
	if(forward == 0u && rewind == 1u) {
		u32 before = initialReadOffset + 32768u;
		u32 expected = (before > 65536u ? before - 65536u : 0u) + 32768u;
		assert(file.offset == expected);
	}
}

static void testForwardNearLimit(void)
{
	file_handle file = {"track.mp3", 0u, UINT32_MAX, &device};
	clipReads = true;
	delayedRead = false;
	playing = pendingRead = firstReadDone = false;
	initialOffset = UINT32_MAX - 32768u - 16384u;
	buttonIndex = 0u;
	buttonCount = 3u;
	buttons[0] = BUTTON_RIGHT;
	buttons[1] = BUTTON_B;
	buttons[2] = 0u;
	assert(play_mp3(&file, 1, 1) == PLAYER_NEXT);
	assert(buttonIndex == 1u);
	assert(file.offset == UINT32_MAX - 16384u);
}

int main(void)
{
	testForwardNearLimit();
	for(unsigned clipping = 0u; clipping < 2u; ++clipping) {
		for(unsigned delayed = 0u; delayed < 2u; ++delayed) {
			testRewind(clipping != 0u, delayed != 0u, 0u, 1u, 0u);
			testRewind(clipping != 0u, delayed != 0u, 0u, 1u, 32767u);
			testRewind(clipping != 0u, delayed != 0u, 0u, 1u, 32768u);
			testRewind(clipping != 0u, delayed != 0u, 0u, 1u, 98304u);
			testRewind(clipping != 0u, delayed != 0u, 30u, 100u, 0u);
		}
	}
	const u32 sizes[] = {0u, 1u, 32768u, 4u << 20, UINT32_MAX};
	const u32 offsets[] = {0u, 1u, 32768u, 65535u, 65536u,
		0xffff8000u, UINT32_MAX};
	file_handle file = {"track.mp3", 0u, 0u, &device};
	for(unsigned s = 0u; s < sizeof(sizes) / sizeof(sizes[0]); ++s) {
		file.size = sizes[s];
		for(unsigned o = 0u; o < sizeof(offsets) / sizeof(offsets[0]); ++o) {
			file.offset = offsets[o];
			updatescreen_mp3(&file, PLAYER_NEXT, 1, 1);
			unsigned expected = file.size ?
				(unsigned)((u64)MIN(file.offset, file.size) * 29u / file.size) : 0u;
			assert(progressBar[expected] == '*');
		}
	}
	puts("manual MP3 player tests passed");
	return 0;
}
'''


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    source = (ROOT / "cube/swiss/source/mp3.c").read_text()
    functions = "\n\n".join(extract_function(source, signature) for signature in (
        "s32 mp3Reader(", "uiDrawObj_t* updatescreen_mp3(", "int play_mp3("))
    with tempfile.TemporaryDirectory() as folder:
        path = Path(folder)
        harness = path / "player.c"
        harness.write_text(HARNESS.replace("/* PLAYER_FUNCTIONS */", functions))
        command = [os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra",
                   "-Werror", "-Wno-unused-parameter", "-Wno-deprecated-declarations",
                   "-O1", str(harness),
                   "-o", str(path / "player")]
        if args.sanitize:
            command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
            if sys.platform.startswith("linux"):
                command += ["-fno-pie", "-no-pie"]
        subprocess.run(command, check=True)
        subprocess.run([str(path / "player")], check=True)


if __name__ == "__main__":
    main()
