#ifndef UI_ANIM_H
#define UI_ANIM_H

/*
 * One clock for the retained-mode UI. UIAnim_BeginFrame() is called exactly
 * once by the video thread; draw events only read the resulting frame time.
 */

/* UIAnim_Seconds() wraps to 0 here. Every ambient angular rate is an integer
 * multiple of 0.001 rad/s, so wrapping at 1000 * 2pi preserves every phase
 * instead of producing a rare long-soak jump when the float clock is
 * reduced; any other period on the clock divides it too. */
#define UI_ANIM_TIME_WRAP_SECONDS 6283.18530718f
void UIAnim_Reset(void);
void UIAnim_BeginFrame(void);
float UIAnim_Seconds(void);
float UIAnim_Delta(void);
float UIAnim_Approach(float current, float target, float response);

#endif
