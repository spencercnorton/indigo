/* -----------------------------------------------------------
      IPLFontWrite.h - Font blitter for IPL ROM fonts
	      - by emu_kidid
	   
      Version 1.0 11/11/2009
        - Initial Code
   ----------------------------------------------------------- */

#ifndef IPLFontWrite_H
#define IPLFontWrite_H

#include "FrameBufferMagic.h"
#include "input.h"

#define wait_press_A() ({while((padsButtonsHeld() & BUTTON_A)){VIDEO_WaitVSync();} while(!(padsButtonsHeld() & BUTTON_A)){VIDEO_WaitVSync();}})

#define ALIGN_LEFT 0
#define ALIGN_CENTER 1
#define ALIGN_RIGHT 2

extern GXColor defaultColor;
extern GXColor disabledColor;
extern GXColor deSelectedColor;
extern char txtbuffer[2048];

void init_font(void);
/* The font as the CPU reads it, for text drawn into a picture (Apps'
 * posters of names): a line's height in pixels (0 without the font), and
 * character c's width and coverage, 0 to 255, stride bytes a row. False
 * without the font, for a glyph wider than maxWidth or a sheet format it
 * doesn't read. */
int fontCellHeight(void);
bool fontGlyph(unsigned char c, u8 *coverage, int stride, int maxWidth,
	int *width);
void drawString(int x, int y, const char *string, float scale, int align, GXColor fontColor);
void drawStringMedium(int x, int y, const char *string, float scale, int align, GXColor fontColor);
void drawStringMediumUntinted(int x, int y, const char *string, float scale, int align, GXColor fontColor);
void drawStringWithCaret(int x, int y, const char *string, float scale, int align, GXColor fontColor, int caretPosition, GXColor caretColor);
int GetFontHeight(float scale);
int GetTextSizeInPixels(const char *string);
float GetTextScaleToFitInWidth(const char *string, int width);
float GetTextScaleToFitInWidthWithMax(const char *string, int width, float max);

#endif
