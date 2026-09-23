#ifndef MENUAUDIO_H
#define MENUAUDIO_H

#include <stdbool.h>

// Original menu sound pack: a bundled composition plus procedural UI tones.
// AESND mixes from the DSP interrupt, so playback continues through blocking
// menu input loops. Everything is config-gated (disableMenuMusic/disableMenuSFX).
void menuaudio_init(void);
void menuaudio_apply_settings(void);
void menuaudio_suspend(void);
void menuaudio_resume(void);
bool menuaudio_shutdown(void);
void menuaudio_blip(void);
void menuaudio_select(void);

#endif
