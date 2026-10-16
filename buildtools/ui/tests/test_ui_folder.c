#define _GNU_SOURCE
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "ui_folder.h"

static int measure(const char *text) { return (int)strlen(text) * 20; }

static char *saved(const uiFolderColors_t *map)
{
	char *text = NULL;
	size_t size = 0u;
	FILE *file = open_memstream(&text, &size);
	assert(file != NULL);
	UIFolder_WriteColors(map, file);
	assert(fclose(file) == 0);
	return text;
}

int main(void)
{
	uiFolderColors_t map = {0}, roundtrip = {0};
	const char *path = "sda:/games/Spaces #;~%=\n\t\r/Folder";
	assert(UIFolder_SetColor(&map, path, 3u));
	assert(UIFolder_GetColor(&map, path) == 3u);
	assert(UIFolder_GetColor(&map, "sdb:/games/Spaces #;~%=\n\t\r/Folder") == 0u);
	char *text = saved(&map);
	assert(strstr(text, "%20#%3B%7E%25%3D%0A%09%0D") != NULL);
	UIFolder_ParseColors(&roundtrip, text);
	assert(memcmp(&map, &roundtrip, sizeof(map)) == 0);
	free(text);
	UIFolder_ParseColors(&roundtrip, "bad%~3;zero%00path~1;good~2;good~8;too~9;x%Q0~1");
	assert(UIFolder_GetColor(&roundtrip, "good/") == 8u);
	assert(UIFolder_GetColor(&roundtrip, "bad") == 0u);
	assert(UIFolder_GetColor(&roundtrip, "zero") == 0u);
	assert(UIFolder_GetColor(&roundtrip, "too") == 0u);
	assert(UIFolder_SetColor(&roundtrip, "good///", 0u));
	assert(UIFolder_GetColor(&roundtrip, "good") == 0u);
	char maximum[UI_FOLDER_PATH_SIZE];
	memset(maximum, '%', sizeof(maximum) - 1u);
	maximum[sizeof(maximum) - 1u] = '\0';
	assert(UIFolder_SetColor(&map, maximum, 8u));
	text = saved(&map);
	UIFolder_ParseColors(&roundtrip, text);
	assert(UIFolder_GetColor(&roundtrip, maximum) == 8u);
	free(text);
	memset(&map, 0, sizeof(map));
	for(unsigned i = 0u; i < UI_FOLDER_COLOR_ENTRIES; ++i) {
		char name[32];
		snprintf(name, sizeof(name), "sda:/games/Folder%u", i);
		assert(UIFolder_SetColor(&map, name, 1u));
	}
	assert(!UIFolder_SetColor(&map, "sda:/games/new", 2u));
	assert(UIFolder_SetColor(&map, "sda:/games/Folder4", 6u));
	assert(UIFolder_GetColor(&map, "sda:/games/Folder4") == 6u);
	assert(UIFolder_SetColor(&map, "sda:/games/Folder4", 0u));
	assert(UIFolder_SetColor(&map, "sda:/games/new", 2u));
	assert(!UIFolder_SetColor(&map, "sda:/games/new", 9u));
	uiFolderSnapshot_t snapshot;
	assert(UIFolder_PreparePath(&snapshot, path, measure));
	char joined[UI_FOLDER_PATH_SIZE * 4u] = "";
	for(unsigned i = 0; i < snapshot.lineCount; ++i) strcat(joined, snapshot.lines[i]);
	assert(strcmp(joined, "sda:/games/Spaces #;~%=\\x0A\\x09\\x0D/Folder") == 0);
	assert(UIFolder_PreparePath(&snapshot, maximum, measure));
	assert(snapshot.lineCount > UI_FOLDER_VISIBLE_LINES);
	joined[0] = '\0';
	for(unsigned i = 0; i < snapshot.lineCount; ++i) strcat(joined, snapshot.lines[i]);
	assert(strcmp(joined, maximum) == 0);
	memset(maximum, 1, sizeof(maximum) - 1u);
	assert(UIFolder_PreparePath(&snapshot, maximum, measure));
	joined[0] = '\0';
	for(unsigned i = 0; i < snapshot.lineCount; ++i) strcat(joined, snapshot.lines[i]);
	assert(strlen(joined) == (UI_FOLDER_PATH_SIZE - 1u) * 4u);
	assert(snapshot.lineCount > UI_FOLDER_VISIBLE_LINES);
	assert(strcmp(UIFolder_ColorName(0u), "Default") == 0);
	/* The production Memory Cards reducer owns local previews only. Saving
	 * and cancelling have priority over simultaneous navigation presses. */
	snapshot.color = 0u;
	assert(UIFolder_Input(&snapshot, UI_FOLDER_INPUT_LEFT) == UI_FOLDER_ACTION_NONE);
	assert(snapshot.color == 8u);
	UIFolder_Input(&snapshot, UI_FOLDER_INPUT_RIGHT);
	assert(snapshot.color == 0u);
	UIFolder_Input(&snapshot, UI_FOLDER_INPUT_RIGHT);
	assert(snapshot.color == 1u);
	assert(UIFolder_Input(&snapshot, UI_FOLDER_INPUT_SAVE | UI_FOLDER_INPUT_RIGHT) == UI_FOLDER_ACTION_SAVE);
	assert(snapshot.color == 1u);
	assert(UIFolder_Input(&snapshot, UI_FOLDER_INPUT_CANCEL | UI_FOLDER_INPUT_SAVE) == UI_FOLDER_ACTION_CANCEL);
	assert(snapshot.color == 1u);
	UIFolder_Input(&snapshot, UI_FOLDER_INPUT_RESET);
	assert(snapshot.color == 0u);
	snapshot.firstLine = 0u;
	UIFolder_Input(&snapshot, UI_FOLDER_INPUT_UP);
	assert(snapshot.firstLine == 0u);
	for(unsigned i = 0u; i < snapshot.lineCount + 2u; ++i)
		UIFolder_Input(&snapshot, UI_FOLDER_INPUT_DOWN);
	assert(snapshot.firstLine + UI_FOLDER_VISIBLE_LINES == snapshot.lineCount);
	UIFolder_Input(&snapshot, UI_FOLDER_INPUT_UP);
	assert(snapshot.firstLine + UI_FOLDER_VISIBLE_LINES + 1u == snapshot.lineCount);
	puts("folder identities, settings roundtrip, bounded map and full path paging: PASS");
	return 0;
}
