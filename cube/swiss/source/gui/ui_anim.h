#ifndef UI_ANIM_H
#define UI_ANIM_H

/*
 * One clock for the retained-mode UI. UIAnim_BeginFrame() is called exactly
 * once by the video thread; draw events only read the resulting frame time.
 */
void UIAnim_Reset(void);
void UIAnim_BeginFrame(void);
float UIAnim_Seconds(void);
float UIAnim_Delta(void);
float UIAnim_Approach(float current, float target, float response);

#endif
