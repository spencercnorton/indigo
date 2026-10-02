/* aesnd.c -- AESND stopped hundreds of times, as a launch stops the menu's audio.
 *
 * Each round starts AESND with a looping voice, plays it a few audio periods
 * and a varying part of one more, stops the voice and calls AESND_Reset, as
 * menuaudio_shutdown does. It links the AESND the Swiss build made
 * (cube/swiss/build/aesndlib.o: libogc2's, with cube/swiss/aesnd's patches).
 * A reset that waits on the DSP for ever stops the rounds: the emulator test
 * (aesnd_test.py) reads them off the debug UART, which Dolphin logs. With
 * libogc2's own AESND_Reset this froze within 50 rounds. */
#include <gccore.h>
#include <aesndlib.h>
#include <ogc/lwp_watchdog.h>
#include <stdio.h>
#include <stdlib.h>

#define ROUNDS 300

/* Text on the debug UART the IPL chip carries (EXI channel 0, device 1),
 * as the probe writes it. */
#define EXI0 ((vu32 *)0xCC006800)

static void exi_imm_write(u32 data, int len)
{
	EXI0[4] = data;
	EXI0[3] = 1 | (1 << 2) | ((len - 1) << 4);
	while (EXI0[3] & 1)
		;
}

static void uart(const char *text)
{
	EXI0[0] = (1 << 8) | (3 << 4);	/* device 1, 8 MHz */
	exi_imm_write(0xA0010000, 4);
	for (int i = 0; text[i]; i += 4) {
		u32 word = 0;
		int n = 0;
		for (; n < 4 && text[i + n]; n++)
			word |= (u32)(u8)text[i + n] << (24 - 8 * n);
		exi_imm_write(word, n);
		if (n < 4)
			break;
	}
	EXI0[0] = 0;
}

static s16 tone[4096] ATTRIBUTE_ALIGN(32);

int main(void)
{
	char line[40];

	VIDEO_Init();
	GXRModeObj *mode = VIDEO_GetPreferredMode(NULL);
	void *xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
	VIDEO_Configure(mode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	for (int i = 0; i < 4096; i++)
		tone[i] = (i & 32) ? 4000 : -4000;
	DCFlushRange(tone, sizeof(tone));
	srand(1);
	uart("aesnd: start\r");
	for (int round = 1; round <= ROUNDS; round++) {
		AESND_Init();
		AESNDPB *voice = AESND_AllocateVoice(NULL);
		AESND_PlayVoice(voice, VOICE_MONO16, tone, sizeof(tone), 32000, 0, true);
		u64 start = gettime();
		u32 wait = 20000 + (u32)(rand() % 6000);	/* microseconds */
		while (diff_usec(start, gettime()) < wait)
			;
		AESND_SetVoiceStop(voice, true);
		AESND_Reset();
		if (round % 50 == 0) {
			snprintf(line, sizeof(line), "aesnd: round %d\r", round);
			uart(line);
		}
	}
	uart("aesnd: done\r");
	for (;;)
		VIDEO_WaitVSync();
	return 0;
}
