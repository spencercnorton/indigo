#include <gctypes.h>
#include <ogc/n64.h>
#include <ogc/pad.h>
#include <ogc/si.h>
#include <ogc/si_steering.h>
#include <ogc/system.h>
#include <stdlib.h>

#include "input.h"

static u32 resetBits;
/* PAD_ScanPads returns low channel bits (1 << channel), unlike PAD_CHAN_BIT.
 * The atomic snapshot starts disconnected until the first post-retrace scan
 * publishes a result. */
static u32 menuInputValidMask;

static void resetCallback(void)
{
	u32 recalibrateBits = 0;

	for (s32 chan = PAD_CHAN0; chan < PAD_CHANMAX; chan++) {
		u32 type;

		if (PAD_GetType(chan, &type)) {
			recalibrateBits |= PAD_CHAN_BIT(chan);
		} else {
			switch (SI_DecodeType(type)) {
				case SI_N64_CONTROLLER:
					N64_ResetAsync(chan, NULL, NULL);
					break;
				case SI_GC_STEERING:
					resetBits |= SI_CHAN_BIT(chan);
					break;
			}
		}
	}

	PAD_Recalibrate(recalibrateBits);
}

static void samplingCallback(void)
{
	SISteeringStatus steering;

	for (s32 chan = SI_CHAN0; chan < SI_MAX_CHAN; chan++) {
		if (resetBits & SI_CHAN_BIT(chan)) {
			switch (SI_ReadSteering(chan, &steering)) {
				case SI_STEERING_ERR_READY:
					SI_ControlSteering(chan, SI_STEERING_CONTROL_DRIVE, -steering.steering * 16);
					if (steering.steering || SYS_ResetButtonDown()) break;
				case SI_STEERING_ERR_NO_CONTROLLER:
					resetBits &= ~SI_CHAN_BIT(chan);
					break;
			}
		}
	}
}

void padsInit()
{
	PAD_Init();
	SI_InitSteering();
	SI_SetSteeringSamplingCallback(samplingCallback);
	SYS_SetResetCallback(resetCallback);
}

void padsScan(void)
{
	u32 validMask = PAD_ScanPads() &
		((1u << UI_MENU_INPUT_CHANNEL_COUNT) - 1u);
	__atomic_store_n(&menuInputValidMask, validMask, __ATOMIC_RELAXED);
}

s8 __chooseMaxMagnitiude(s8 p0, s8 p1, s8 p2, s8 p3)
{
	/* Keep magnitudes wider than s8: abs(-128) is 128 and must not wrap back
	 * to -128 before the comparison. Equal magnitudes preserve port order. */
	int p0a = abs((int)p0);
	int p1a = abs((int)p1);
	int p2a = abs((int)p2);
	int p3a = abs((int)p3);

	s8 res = p0;
	int maxa = p0a;

	if (p1a > maxa) {
		res = p1;
		maxa = p1a;
	}
	if (p2a > maxa) {
		res = p2;
		maxa = p2a;
	}
	if (p3a > maxa) {
		res = p3;
		maxa = p3a;
	}

	return res;
}

u32 padsButtonsHeld() {
	return (
		PAD_ButtonsHeld(PAD_CHAN0) |
		PAD_ButtonsHeld(PAD_CHAN1) |
		PAD_ButtonsHeld(PAD_CHAN2) |
		PAD_ButtonsHeld(PAD_CHAN3)
	);
}

s8 padsStickX() {
	return __chooseMaxMagnitiude(
		PAD_StickX(PAD_CHAN0),
		PAD_StickX(PAD_CHAN1),
		PAD_StickX(PAD_CHAN2),
		PAD_StickX(PAD_CHAN3)
	);
}

s8 padsStickY() {
	return __chooseMaxMagnitiude(
		PAD_StickY(PAD_CHAN0),
		PAD_StickY(PAD_CHAN1),
		PAD_StickY(PAD_CHAN2),
		PAD_StickY(PAD_CHAN3)
	);
}

uiMenuInputDirection_t padsMenuInputPoll(uiMenuInputState_t *state,
	u32 elapsedMicroseconds, u32 policy, bool inhibited)
{
	u32 validMask = __atomic_load_n(&menuInputValidMask, __ATOMIC_RELAXED);
	uiMenuInputSample_t samples[UI_MENU_INPUT_CHANNEL_COUNT] = {
		{PAD_StickX(PAD_CHAN0), PAD_StickY(PAD_CHAN0),
			(validMask & (1u << PAD_CHAN0)) != 0u},
		{PAD_StickX(PAD_CHAN1), PAD_StickY(PAD_CHAN1),
			(validMask & (1u << PAD_CHAN1)) != 0u},
		{PAD_StickX(PAD_CHAN2), PAD_StickY(PAD_CHAN2),
			(validMask & (1u << PAD_CHAN2)) != 0u},
		{PAD_StickX(PAD_CHAN3), PAD_StickY(PAD_CHAN3),
			(validMask & (1u << PAD_CHAN3)) != 0u}
	};

	return UIMenuInput_Update(state, samples, elapsedMicroseconds,
		policy, inhibited);
}
