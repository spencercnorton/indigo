/* aesnd_reset.h -- stopping AESND without freezing the console.
 *
 * AESND_Reset (libogc2) mails the DSP to stop and waits, interrupts off,
 * until it takes the mail. libogc2's mixer takes no mail until the CPU has
 * read the reply it sent last, and AESND keeps the DSP replying every audio
 * period, voices or not: a reply sent just before the reset was never read,
 * and a launch froze until the console was switched off. Paused, AESND
 * starts no more of the DSP's frames, and two video frames with interrupts
 * on let the one in flight finish and its replies be read. The emulator test
 * stops AESND this way hundreds of times (buildtools/ui/emulator/aesnd/). */
#ifndef AESND_RESET_H
#define AESND_RESET_H

#include <gccore.h>
#include <aesndlib.h>

static inline void aesnd_reset_safely(void) {
	AESND_Pause(true);
	VIDEO_WaitVSync();
	VIDEO_WaitVSync();
	AESND_Reset();
}

#endif
