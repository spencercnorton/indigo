#include "ui_folder.h"

#include <string.h>

uiFolderAction_t UIFolder_Input(uiFolderSnapshot_t *snapshot, uint32_t input)
{
	if(snapshot == NULL) return UI_FOLDER_ACTION_NONE;
	if(input & UI_FOLDER_INPUT_CANCEL) return UI_FOLDER_ACTION_CANCEL;
	if(input & UI_FOLDER_INPUT_SAVE) return UI_FOLDER_ACTION_SAVE;
	if(input & UI_FOLDER_INPUT_RESET) snapshot->color = 0u;
	else if(input & UI_FOLDER_INPUT_LEFT) snapshot->color =
		(uint8_t)((snapshot->color + UI_FOLDER_COLOR_COUNT - 1u) % UI_FOLDER_COLOR_COUNT);
	else if(input & UI_FOLDER_INPUT_RIGHT) snapshot->color =
		(uint8_t)((snapshot->color + 1u) % UI_FOLDER_COLOR_COUNT);
	else if(input & UI_FOLDER_INPUT_UP) {
		if(snapshot->firstLine > 0u) --snapshot->firstLine;
	}
	else if(input & UI_FOLDER_INPUT_DOWN) {
		if(snapshot->firstLine + UI_FOLDER_VISIBLE_LINES < snapshot->lineCount)
			++snapshot->firstLine;
	}
	return UI_FOLDER_ACTION_NONE;
}

static size_t pathLength(const char *path)
{
	size_t length = 0u;
	if(path == NULL) return 0u;
	while(length < UI_FOLDER_PATH_SIZE && path[length]) ++length;
	if(length == UI_FOLDER_PATH_SIZE) return 0u;
	while(length > 0u && path[length - 1u] == '/') --length;
	return length;
}

static bool matches(const uiFolderColorEntry_t *entry, const char *path,
	size_t length)
{
	return strlen(entry->path) == length &&
		memcmp(entry->path, path, length) == 0;
}

uint8_t UIFolder_GetColor(const uiFolderColors_t *colors, const char *path)
{
	size_t length = pathLength(path);
	if(colors == NULL || length == 0u) return 0u;
	for(size_t i = 0u; i < UI_FOLDER_COLOR_ENTRIES; ++i) {
		if(matches(&colors->entries[i], path, length)) {
			return colors->entries[i].color;
		}
	}
	return 0u;
}

bool UIFolder_SetColor(uiFolderColors_t *colors, const char *path, uint8_t color)
{
	size_t length = pathLength(path);
	uiFolderColorEntry_t *empty = NULL;
	if(colors == NULL || length == 0u || color >= UI_FOLDER_COLOR_COUNT) return false;
	for(size_t i = 0u; i < UI_FOLDER_COLOR_ENTRIES; ++i) {
		uiFolderColorEntry_t *entry = &colors->entries[i];
		if(matches(entry, path, length)) {
			if(color == 0u) memset(entry, 0, sizeof(*entry));
			else entry->color = color;
			return true;
		}
		if(empty == NULL && entry->path[0] == '\0') empty = entry;
	}
	if(color == 0u) return true;
	if(empty == NULL) return false;
	memcpy(empty->path, path, length);
	empty->path[length] = '\0';
	empty->color = color;
	return true;
}

static int hexValue(unsigned char c)
{
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'A' && c <= 'F') return c - 'A' + 10;
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

void UIFolder_ParseColors(uiFolderColors_t *colors, const char *text)
{
	const char *cursor = text;
	if(colors == NULL) return;
	memset(colors, 0, sizeof(*colors));
	if(cursor == NULL) return;
	while(*cursor) {
		char path[UI_FOLDER_PATH_SIZE];
		size_t length = 0u;
		bool valid = true;
		const char *end = strchr(cursor, ';');
		const char *separator;
		if(end == NULL) end = cursor + strlen(cursor);
		separator = memchr(cursor, '~', (size_t)(end - cursor));
		if(separator == NULL || end - separator != 2 ||
			separator[1] < '1' || separator[1] > '8') valid = false;
		while(valid && cursor < separator) {
			unsigned char c = (unsigned char)*cursor++;
			if(c == '%') {
				int high, low;
				if(separator - cursor < 2) { valid = false; break; }
				high = hexValue((unsigned char)cursor[0]);
				low = hexValue((unsigned char)cursor[1]);
				if(high < 0 || low < 0) { valid = false; break; }
				c = (unsigned char)(high * 16 + low);
				cursor += 2;
			}
			if(c == 0u || length + 1u >= sizeof(path)) { valid = false; break; }
			path[length++] = (char)c;
		}
		path[length] = '\0';
		if(valid) (void)UIFolder_SetColor(colors, path, (uint8_t)(separator[1] - '0'));
		cursor = *end == ';' ? end + 1 : end;
	}
}

void UIFolder_WriteColors(const uiFolderColors_t *colors, FILE *file)
{
	bool first = true;
	if(colors == NULL || file == NULL) return;
	for(size_t i = 0u; i < UI_FOLDER_COLOR_ENTRIES; ++i) {
		const uiFolderColorEntry_t *entry = &colors->entries[i];
		if(entry->path[0] == '\0' || entry->color == 0u) continue;
		if(!first) fputc(';', file);
		first = false;
		for(const unsigned char *p = (const unsigned char*)entry->path; *p; ++p) {
			if(*p <= ' ' || *p == '%' || *p == ';' || *p == '~' || *p == '=') {
				fprintf(file, "%%%02X", (unsigned)*p);
			}
			else fputc(*p, file);
		}
		fprintf(file, "~%u", (unsigned)entry->color);
	}
}

const char *UIFolder_ColorName(uint8_t color)
{
	static const char *const names[UI_FOLDER_COLOR_COUNT] = {
		"Default", "Indigo", "Azure", "Emerald", "Gold", "Spice", "Crimson", "Rose", "Jet Black"
	};
	return names[color < UI_FOLDER_COLOR_COUNT ? color : 0u];
}

void UIFolder_ColorRGB(uint8_t color, uint8_t *r, uint8_t *g, uint8_t *b)
{
	static const uint8_t palette[UI_FOLDER_COLOR_COUNT][3] = {
		{135, 120, 207}, {135, 120, 207}, {68, 170, 230}, {55, 185, 112},
		{238, 192, 62}, {237, 136, 55}, {222, 74, 79}, {226, 128, 172}, {80, 80, 88}
	};
	const uint8_t *rgb = palette[color < UI_FOLDER_COLOR_COUNT ? color : 0u];
	*r = rgb[0]; *g = rgb[1]; *b = rgb[2];
}

bool UIFolder_PreparePath(uiFolderSnapshot_t *snapshot, const char *path,
	int (*measure)(const char *text))
{
	char printable[UI_FOLDER_PATH_SIZE * 4u];
	static const char hex[] = "0123456789ABCDEF";
	size_t length = 0u, cursor = 0u, displayed = 0u;
	if(snapshot == NULL || path == NULL || measure == NULL) return false;
	while(length < UI_FOLDER_PATH_SIZE && path[length]) ++length;
	if(length == UI_FOLDER_PATH_SIZE || length == 0u) return false;
	for(size_t i = 0u; i < length; ++i) {
		unsigned char c = (unsigned char)path[i];
		if(c < 32u || c == 127u) {
			printable[displayed++] = '\\';
			printable[displayed++] = 'x';
			printable[displayed++] = hex[c >> 4];
			printable[displayed++] = hex[c & 15u];
		}
		else printable[displayed++] = (char)c;
	}
	printable[displayed] = '\0';
	memset(snapshot, 0, sizeof(*snapshot));
	while(cursor < displayed && snapshot->lineCount < UI_FOLDER_PATH_LINES) {
		char *line = snapshot->lines[snapshot->lineCount];
		size_t count = 0u;
		while(cursor + count < displayed && count + 1u < UI_FOLDER_LINE_SIZE) {
			line[count] = printable[cursor + count];
			line[count + 1u] = '\0';
			if(count > 0u && (float)measure(line) * 0.46f > 532.0f) {
				line[count] = '\0';
				break;
			}
			++count;
		}
		cursor += count;
		++snapshot->lineCount;
	}
	return cursor == displayed;
}
