#ifndef UI_FOLDER_H
#define UI_FOLDER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define UI_FOLDER_PATH_SIZE 1024u
#define UI_FOLDER_COLOR_ENTRIES 32u
#define UI_FOLDER_COLOR_COUNT 9u /* Default, then Menu Color's eight colors */
#define UI_FOLDER_PATH_LINES 128u
#define UI_FOLDER_LINE_SIZE 128u
#define UI_FOLDER_VISIBLE_LINES 6u

typedef struct {
	char path[UI_FOLDER_PATH_SIZE];
	uint8_t color;
} uiFolderColorEntry_t;

typedef struct {
	uiFolderColorEntry_t entries[UI_FOLDER_COLOR_ENTRIES];
} uiFolderColors_t;

typedef struct {
	char lines[UI_FOLDER_PATH_LINES][UI_FOLDER_LINE_SIZE];
	uint32_t lineCount;
	uint32_t firstLine;
	uint8_t color;
	char summary[96];
	char contents[2][96];
	char status[80];
} uiFolderSnapshot_t;

typedef enum {
	UI_FOLDER_INPUT_SAVE = 1u << 0,
	UI_FOLDER_INPUT_CANCEL = 1u << 1,
	UI_FOLDER_INPUT_RESET = 1u << 2,
	UI_FOLDER_INPUT_LEFT = 1u << 3,
	UI_FOLDER_INPUT_RIGHT = 1u << 4,
	UI_FOLDER_INPUT_UP = 1u << 5,
	UI_FOLDER_INPUT_DOWN = 1u << 6
} uiFolderInput_t;

typedef enum {
	UI_FOLDER_ACTION_NONE = 0,
	UI_FOLDER_ACTION_SAVE,
	UI_FOLDER_ACTION_CANCEL
} uiFolderAction_t;

/* Prepared Memory Cards folder controls. Cancel wins; Save never changes
 * the preview in its own input frame. Persistent storage belongs to caller. */
uiFolderAction_t UIFolder_Input(uiFolderSnapshot_t *snapshot, uint32_t input);

/* Identity is the whole device-prefixed path, with trailing slashes removed.
 * The map stays in menu-thread memory; a draw never reads a settings file. */
uint8_t UIFolder_GetColor(const uiFolderColors_t *colors, const char *path);
bool UIFolder_SetColor(uiFolderColors_t *colors, const char *path, uint8_t color);
void UIFolder_ParseColors(uiFolderColors_t *colors, const char *text);
void UIFolder_WriteColors(const uiFolderColors_t *colors, FILE *file);
const char *UIFolder_ColorName(uint8_t color);
void UIFolder_ColorRGB(uint8_t color, uint8_t *r, uint8_t *g, uint8_t *b);
/* Preserve every path byte, wrapping rather than ellipsizing. The caller
 * pages the prepared lines six at a time. measure returns unscaled width. */
bool UIFolder_PreparePath(uiFolderSnapshot_t *snapshot, const char *path,
	int (*measure)(const char *text));

#endif
