#include <gccore.h>
#include <ogc/cache.h>
#include <aesndlib.h>
#include <mad.h>
#include <limits.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "swiss.h"
#include "menuaudio.h"
#include "menu_music_mp3.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SR                  32000
#define VOL_UNITY           256
#define PCM_INITIAL_SAMPLES (1u << 20)
#define MIN_LOOP_FRAMES     (SR / 10)

static bool inited = false;
static bool musicPlaying = false;
static bool suspended = false;
static bool resumeMusic = false;
static AESNDPB *musicVoice = NULL;
static AESNDPB *sfxVoice = NULL;
static s16 *musicBuf = NULL;
static u32 musicBytes = 0;
static u32 musicFormat = VOICE_STEREO16;
static int musicRate = SR;
static s16 *blipBuf = NULL;
static u32 blipBytes = 0;
static s16 *selBuf = NULL;
static u32 selBytes = 0;

static s16 clamp16(double v) {
	if(v > 32767.0) return 32767;
	if(v < -32768.0) return -32768;
	return (s16)v;
}

static s16 mf2s16(mad_fixed_t f) {
	f += (1L << (MAD_F_FRACBITS - 16));
	if(f >= MAD_F_ONE) f = MAD_F_ONE - 1;
	else if(f < -MAD_F_ONE) f = -MAD_F_ONE;
	return (s16)(f >> (MAD_F_FRACBITS + 1 - 16));
}

static bool grow_pcm(s16 **buf, u32 *capacity, u32 used, u32 needed) {
	u32 nextCapacity = *capacity ? *capacity : PCM_INITIAL_SAMPLES;
	while(nextCapacity < needed) {
		if(nextCapacity > UINT_MAX / 2) {
			nextCapacity = needed;
			break;
		}
		nextCapacity *= 2;
	}
	if(nextCapacity > UINT_MAX / sizeof(**buf)) return false;

	s16 *next = memalign(32, (size_t)nextCapacity * sizeof(*next));
	if(!next) return false;
	if(*buf && used) memcpy(next, *buf, (size_t)used * sizeof(*next));
	free(*buf);
	*buf = next;
	*capacity = nextCapacity;
	return true;
}

// Decode the bundled MP3 into an aligned, interleaved s16 buffer. libmad may
// read MAD_BUFFER_GUARD bytes past the final frame, so decode from a padded
// copy rather than directly from the compiled-in array.
static s16 *decode_mp3(const unsigned char *mp3, u32 mp3len, u32 *outFrames, int *outCh, int *outRate) {
	struct mad_stream stream;
	struct mad_frame frame;
	struct mad_synth synth;
	s16 *out = NULL;
	unsigned char *guarded = NULL;
	u32 capacity = 0;
	u32 count = 0;
	int channels = 0;
	int rate = 0;
	bool failed = false;

	*outFrames = 0;
	*outCh = 0;
	*outRate = 0;
	if(!mp3 || !mp3len || mp3len > UINT_MAX - MAD_BUFFER_GUARD) return NULL;

	guarded = malloc((size_t)mp3len + MAD_BUFFER_GUARD);
	if(!guarded) return NULL;
	memcpy(guarded, mp3, mp3len);
	memset(guarded + mp3len, 0, MAD_BUFFER_GUARD);

	mad_stream_init(&stream);
	mad_frame_init(&frame);
	mad_synth_init(&synth);
	mad_stream_buffer(&stream, guarded, mp3len + MAD_BUFFER_GUARD);

	for(;;) {
		if(mad_frame_decode(&frame, &stream)) {
			if(stream.error == MAD_ERROR_BUFLEN) break;
			if(MAD_RECOVERABLE(stream.error)) continue;
			failed = true;
			break;
		}

		mad_synth_frame(&synth, &frame);
		int frameChannels = synth.pcm.channels;
		int frameRate = synth.pcm.samplerate;
		u32 frameSamples = synth.pcm.length;
		if((frameChannels != 1 && frameChannels != 2) || frameRate <= 0 ||
		   (channels && (channels != frameChannels || rate != frameRate)) ||
		   frameSamples > (UINT_MAX - count) / frameChannels) {
			failed = true;
			break;
		}
		channels = frameChannels;
		rate = frameRate;

		u32 needed = count + frameSamples * frameChannels;
		if(needed > capacity && !grow_pcm(&out, &capacity, count, needed)) {
			failed = true;
			break;
		}
		for(u32 i = 0; i < frameSamples; i++) {
			out[count++] = mf2s16(synth.pcm.samples[0][i]);
			if(frameChannels == 2) out[count++] = mf2s16(synth.pcm.samples[1][i]);
		}
	}

	mad_synth_finish(&synth);
	mad_frame_finish(&frame);
	mad_stream_finish(&stream);
	free(guarded);

	if(failed || !out || !channels || !count) {
		free(out);
		return NULL;
	}

	*outFrames = count / channels;
	*outCh = channels;
	*outRate = rate;
	return out;
}

// Trim MP3 encoder delay/padding, then overlap the tail into the head so the
// voice wraps without a click.
static u32 make_seamless(s16 *buf, u32 frames, int channels) {
	const int threshold = 120;
	u32 head = 0;
	u32 tail = frames;
	while(head < frames) {
		int left = abs(buf[head * channels]);
		int right = channels > 1 ? abs(buf[head * channels + 1]) : left;
		if(left > threshold || right > threshold) break;
		head++;
	}
	while(tail > head) {
		u32 i = tail - 1;
		int left = abs(buf[i * channels]);
		int right = channels > 1 ? abs(buf[i * channels + 1]) : left;
		if(left > threshold || right > threshold) break;
		tail--;
	}

	u32 length = tail - head;
	if(head && length) memmove(buf, buf + (size_t)head * channels, (size_t)length * channels * sizeof(*buf));
	u32 crossfade = 1600;
	if(crossfade > length / 4) crossfade = length / 4;
	for(u32 i = 0; i < crossfade; i++) {
		float weight = (float)i / crossfade;
		for(int channel = 0; channel < channels; channel++) {
			float headSample = buf[(size_t)i * channels + channel];
			float tailSample = buf[((size_t)length - crossfade + i) * channels + channel];
			buf[(size_t)i * channels + channel] = (s16)(headSample * weight + tailSample * (1.0f - weight));
		}
	}
	return length - crossfade;
}

static s16 *synth_tone(double freq, double duration, double decay, double amp, u32 *outBytes) {
	u32 samples = (u32)(SR * duration);
	s16 *buf = memalign(32, samples * sizeof(*buf));
	if(!buf) {
		*outBytes = 0;
		return NULL;
	}
	double attack = SR * 0.004;
	for(u32 n = 0; n < samples; n++) {
		double envelope = exp(-(double)n / (SR * decay));
		if(n < attack) envelope *= (double)n / attack;
		double sample = 0.85 * sin(2.0 * M_PI * freq * (double)n / SR) +
		                0.15 * sin(2.0 * M_PI * 2 * freq * (double)n / SR);
		buf[n] = clamp16(amp * 32767.0 * envelope * sample);
	}
	*outBytes = samples * sizeof(*buf);
	return buf;
}

static s16 *synth_chime(u32 *outBytes) {
	const double duration = 0.18;
	const double decay = 0.09;
	const double amp = 0.30;
	const double f1 = 659.25;
	const double f2 = 988.0;
	u32 samples = (u32)(SR * duration);
	s16 *buf = memalign(32, samples * sizeof(*buf));
	if(!buf) {
		*outBytes = 0;
		return NULL;
	}
	double attack = SR * 0.004;
	for(u32 n = 0; n < samples; n++) {
		double envelope = exp(-(double)n / (SR * decay));
		if(n < attack) envelope *= (double)n / attack;
		double sample = 0.6 * sin(2.0 * M_PI * f1 * (double)n / SR) +
		                0.4 * sin(2.0 * M_PI * f2 * (double)n / SR);
		buf[n] = clamp16(amp * 32767.0 * envelope * sample);
	}
	*outBytes = samples * sizeof(*buf);
	return buf;
}

static bool prepare_music(void) {
	if(musicBuf && musicBytes) return true;

	u32 frames = 0;
	int channels = 0;
	int rate = 0;
	s16 *pcm = decode_mp3(menu_music_mp3, menu_music_mp3_len, &frames, &channels, &rate);
	if(!pcm || frames < MIN_LOOP_FRAMES) {
		free(pcm);
		return false;
	}
	u32 loopFrames = make_seamless(pcm, frames, channels);
	if(loopFrames < MIN_LOOP_FRAMES || loopFrames > UINT_MAX / (channels * sizeof(*pcm))) {
		free(pcm);
		return false;
	}

	musicBuf = pcm;
	musicBytes = loopFrames * channels * sizeof(*pcm);
	musicFormat = channels == 2 ? VOICE_STEREO16 : VOICE_MONO16;
	musicRate = rate;
	DCFlushRange(musicBuf, musicBytes);
	return true;
}

static void start_music(void) {
	if(!inited || suspended || musicPlaying || swissSettings.disableMenuMusic || !prepare_music()) return;
	if(!musicVoice) musicVoice = AESND_AllocateVoice(NULL);
	if(!musicVoice) return;
	AESND_SetVoiceVolume(musicVoice, (VOL_UNITY * 3) / 5, (VOL_UNITY * 3) / 5);
	AESND_PlayVoice(musicVoice, musicFormat, musicBuf, musicBytes, musicRate, 0, true);
	musicPlaying = true;
}

static void stop_music(void) {
	if(musicVoice && musicPlaying) AESND_SetVoiceStop(musicVoice, true);
	musicPlaying = false;
}

void menuaudio_init(void) {
	if(inited) {
		menuaudio_apply_settings();
		return;
	}

	AESND_Init();
	inited = true;
	blipBuf = synth_tone(880.0, 0.05, 0.03, 0.30, &blipBytes);
	selBuf = synth_chime(&selBytes);
	if(blipBuf) DCFlushRange(blipBuf, blipBytes);
	if(selBuf) DCFlushRange(selBuf, selBytes);
	sfxVoice = AESND_AllocateVoice(NULL);
	menuaudio_apply_settings();
}

void menuaudio_apply_settings(void) {
	if(!inited) return;
	if(swissSettings.disableMenuMusic) stop_music();
	else start_music();
	if(swissSettings.disableMenuSFX && sfxVoice) AESND_SetVoiceStop(sfxVoice, true);
}

void menuaudio_suspend(void) {
	if(!inited || suspended) return;
	resumeMusic = musicPlaying;
	suspended = true;
	stop_music();
	if(sfxVoice) AESND_SetVoiceStop(sfxVoice, true);
}

void menuaudio_resume(void) {
	if(!inited || !suspended) return;
	bool shouldResumeMusic = resumeMusic;
	resumeMusic = false;
	suspended = false;
	if(shouldResumeMusic && !swissSettings.disableMenuMusic) start_music();
}

bool menuaudio_shutdown(void) {
	if(!inited) return false;
	stop_music();
	if(sfxVoice) AESND_SetVoiceStop(sfxVoice, true);
	AESND_Reset();
	musicVoice = NULL;
	sfxVoice = NULL;
	musicPlaying = false;
	suspended = false;
	resumeMusic = false;
	free(musicBuf);
	free(blipBuf);
	free(selBuf);
	musicBuf = NULL;
	blipBuf = NULL;
	selBuf = NULL;
	musicBytes = 0;
	blipBytes = 0;
	selBytes = 0;
	inited = false;
	return true;
}

static void play_sfx(s16 *buf, u32 bytes) {
	if(!buf || !sfxVoice || suspended || swissSettings.disableMenuSFX) return;
	AESND_SetVoiceVolume(sfxVoice, (VOL_UNITY * 17) / 20, (VOL_UNITY * 17) / 20);
	AESND_PlayVoice(sfxVoice, VOICE_MONO16, buf, bytes, SR, 0, false);
}

void menuaudio_blip(void) {
	play_sfx(blipBuf, blipBytes);
}

void menuaudio_select(void) {
	play_sfx(selBuf, selBytes);
}
