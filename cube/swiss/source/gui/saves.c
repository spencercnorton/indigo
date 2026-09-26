/* saves.c - Memory Cards (Home > System > Memory Cards)

   The saves on the memory cards in Slot A and Slot B and in folders on the
   device Indigo keeps its settings on, with Copy, Move and Delete between
   them: the IPL's Memory Card screen, plus folders. The card and FAT drivers
   do the reading and writing, as they do for the file browser's Copy. This
   file decides what to call, reads every copy back before calling it done,
   and removes a Move's original only after that. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <malloc.h>
#include <math.h>
#include <gccore.h>
#include <ogc/card.h>
#include "deviceHandler.h"
#include "filemeta.h"
#include "FrameBufferMagic.h"
#include "IPLFontWrite.h"
#include "swiss.h"
#include "main.h"
#include "config.h"
#include "files.h"
#include "util.h"
#include "input.h"
#include "ui_cheats.h"
#include "ui_menu_input.h"
#include "ui_saves.h"
#include "ui_settings_layout.h"
#include "saves.h"

#define SAVES_DEFAULT_FOLDER "swiss/saves"
#define SAVES_LIST_MAX 256
#define SAVES_TABS 3
#define SAVES_TAB_FOLDER 2
/* A card's first five blocks hold its header, directory and block map. */
#define SAVES_SYSTEM_BLOCKS 5
/* The largest save a card holds (2043 blocks) in the largest wrapper. */
#define SAVES_MAX_BYTES (0x150u + 2043u * UI_SAVES_BLOCK_SIZE)

typedef struct {
	DEVICEHANDLER_INTERFACE *device;
	file_handle dir;			/* the folder listed */
	file_handle *entries;			/* readDir's */
	int entryCount;
	file_handle *list[SAVES_LIST_MAX];	/* what the page lists, in order */
	int count;
	int selection;
	bool ready;
	bool mounted;				/* this screen mounted the card */
	int totalBlocks;
	int freeBlocks;
	u32 listing;				/* which listing this is, for the page */
	char note[2][96];			/* why nothing is listed */
} savesPlace_t;

typedef struct {
	uiMenuInputState_t menu;
	u32 previous;
	u32 lastRetrace;
	u32 repeatHeld;
	u32 repeatTime;
	bool repeated;
} savesInput_t;

static savesPlace_t places[SAVES_TABS];
static savesPlace_t chooser;
static u32 listings;
static uiSavesPageSnapshot_t shown ATTRIBUTE_ALIGN(32);

const char *saves_folder(void)
{
	return swissSettings.saveFolder[0] ? swissSettings.saveFolder :
		SAVES_DEFAULT_FOLDER;
}

static bool isCard(const DEVICEHANDLER_INTERFACE *device)
{
	return device == &__device_card_a || device == &__device_card_b;
}

static DEVICEHANDLER_INTERFACE *slotDevice(int slot)
{
	return slot ? &__device_card_b : &__device_card_a;
}

static const char *slotName(int slot)
{
	return slot ? "Slot B" : "Slot A";
}

static bool isSaveName(const char *name)
{
	size_t length = strlen(name);

	return length > 4u && (!strcasecmp(name + length - 4u, ".gci") ||
		!strcasecmp(name + length - 4u, ".gcs") ||
		!strcasecmp(name + length - 4u, ".sav"));
}

/* ------------------------------------------------------------------------
 * Input: the cheat browser's, one press at a time with held directions
 * repeating on the shared schedule.
 * --------------------------------------------------------------------- */
#define SAVES_DIRECTIONS (BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT | BUTTON_RIGHT)
#define SAVES_BUTTONS (SAVES_DIRECTIONS | BUTTON_A | BUTTON_B | BUTTON_X | \
	BUTTON_L | BUTTON_R)

static u32 inputElapsed(u32 *lastRetrace)
{
	u32 retrace = VIDEO_GetRetraceCount();
	u32 count = retrace - *lastRetrace;
	float rate = VIDEO_GetRetraceRate();
	float elapsed;

	*lastRetrace = retrace;
	if(!isfinite(rate) || rate < 1.0f) rate = 60.0f;
	elapsed = (float)count * 1000000.0f / rate;
	return elapsed >= (float)UI_MENU_INPUT_MAX_ELAPSED_US ?
		UI_MENU_INPUT_MAX_ELAPSED_US : (u32)elapsed;
}

/* Buttons already down (the A that opened this) aren't a press. */
static void inputInit(savesInput_t *input)
{
	memset(input, 0, sizeof(*input));
	UIMenuInput_Init(&input->menu);
	input->previous = padsButtonsHeld() & SAVES_BUTTONS;
	input->lastRetrace = VIDEO_GetRetraceCount();
}

static u32 inputNext(savesInput_t *input)
{
	while(1) {
		u32 held, pressed, elapsed, direction;
		uiMenuInputDirection_t analog;

		VIDEO_WaitVSync();
		held = padsButtonsHeld() & SAVES_BUTTONS;
		pressed = held & ~input->previous;
		input->previous = held;
		elapsed = inputElapsed(&input->lastRetrace);
		analog = padsMenuInputPoll(&input->menu, elapsed,
			UI_MENU_INPUT_AXIS_VERTICAL | UI_MENU_INPUT_REPEAT, held != 0u);
		direction = held & SAVES_DIRECTIONS;
		if(direction != input->repeatHeld) {
			input->repeatHeld = direction;
			input->repeatTime = 0u;
			input->repeated = false;
		}
		else if(direction != 0u && (held & ~SAVES_DIRECTIONS) == 0u) {
			input->repeatTime += elapsed;
			if(input->repeatTime >= (input->repeated ? UI_MENU_INPUT_REPEAT_US :
					UI_MENU_INPUT_INITIAL_REPEAT_US)) {
				pressed |= direction;
				input->repeatTime = 0u;
				input->repeated = true;
			}
		}
		if(analog == UI_MENU_INPUT_UP) pressed |= BUTTON_UP;
		if(analog == UI_MENU_INPUT_DOWN) pressed |= BUTTON_DOWN;
		if(pressed != 0u) {
			return pressed;
		}
	}
}

/* A message in the dialog card; its last line becomes the A prompt. */
static void savesTell(int type, const char *text)
{
	uiDrawObj_t *box = DrawPublish(DrawMessageBox(type, text));

	wait_press_A();
	DrawDispose(box);
}

/* A short list over the page, in the Settings value list's card: A chooses,
 * B cancels. Returns the index chosen, or -1. */
static int savesPick(const char *title, const char *const *items, int count,
	int focus)
{
	static uiSetListSnapshot_t list;
	uiDrawObj_t *box = NULL;
	savesInput_t input;
	int chosen = -1;
	int i;

	if(count <= 0 || count > UI_SETLAYOUT_LIST_MAX) {
		return -1;
	}
	inputInit(&input);
	while(1) {
		u32 pressed;

		memset(&list, 0, sizeof(list));
		UISetLayout_ComputeList(count, focus, &list.layout);
		list.titleScale = UI_SETLAYOUT_CARD_TITLE_SCALE;
		UICheats_Fit(list.title, sizeof(list.title), title,
			list.layout.titleMaxWidth, list.titleScale, GetTextSizeInPixels);
		for(i = 0; i < list.layout.visibleCount; i++) {
			list.valueScale[i] = UI_SETLAYOUT_LABEL_SCALE;
			UICheats_Fit(list.value[i], sizeof(list.value[i]),
				items[list.layout.first + i], list.layout.rowTextMaxWidth,
				list.valueScale[i], GetTextSizeInPixels);
		}
		list.current = -1;
		snprintf(list.hint[0], sizeof(list.hint[0]), "A  Choose");
		snprintf(list.hint[1], sizeof(list.hint[1]), "B  Cancel");
		list.hintScale[0] = list.hintScale[1] = UI_SETLAYOUT_HINT_SCALE;
		if(box == NULL) {
			if((box = DrawSettingsList(&list)) != NULL) {
				DrawPublish(box);
			}
		}
		else {
			DrawUpdateSettingsList(box, &list, -1);
		}
		pressed = inputNext(&input);
		if(pressed & BUTTON_UP) {
			focus = focus > 0 ? focus - 1 : count - 1;
		}
		else if(pressed & BUTTON_DOWN) {
			focus = focus + 1 < count ? focus + 1 : 0;
		}
		else if(pressed & (BUTTON_A | BUTTON_B)) {
			chosen = (pressed & BUTTON_A) ? focus : -1;
			break;
		}
	}
	DrawDispose(box);
	return chosen;
}

/* ------------------------------------------------------------------------
 * Places: a card, or a folder on the settings device.
 * --------------------------------------------------------------------- */

/* Lets go of what a place listed, and the banners its rows read. */
static void placeClear(savesPlace_t *place)
{
	int i;

	for(i = 0; i < place->entryCount; i++) {
		if(place->entries[i].meta != NULL) {
			meta_free(place->entries[i].meta);
			place->entries[i].meta = NULL;
		}
		if(place->device != NULL) {
			place->device->closeFile(&place->entries[i]);
		}
	}
	free(place->entries);
	place->entries = NULL;
	place->entryCount = 0;
	place->count = 0;
	place->ready = false;
	place->listing = ++listings;
	place->note[0][0] = place->note[1][0] = '\0';
}

/* A device that isn't this card but sits in its slot, like an SD adapter
 * that is the source or holds the settings, is never probed as a card. */
static const char *slotTakenBy(int slot)
{
	u32 location = slot ? LOC_MEMCARD_SLOT_B : LOC_MEMCARD_SLOT_A;
	DEVICEHANDLER_INTERFACE *users[2] = {devices[DEVICE_CUR],
		devices[DEVICE_CONFIG]};
	int i;

	for(i = 0; i < 2; i++) {
		if(users[i] != NULL && users[i] != slotDevice(slot) &&
			(users[i]->location & location)) {
			return users[i]->hwName;
		}
	}
	return NULL;
}

static void loadCard(savesPlace_t *place, int slot)
{
	DEVICEHANDLER_INTERFACE *device = slotDevice(slot);
	const char *taken = slotTakenBy(slot);
	device_info *info;
	int used = 0;
	int i;

	placeClear(place);
	place->device = device;
	if(taken != NULL) {
		snprintf(place->note[0], sizeof(place->note[0]), "%s holds the %s",
			slotName(slot), taken);
		snprintf(place->note[1], sizeof(place->note[1]),
			"It's in use, so Memory Cards leaves it alone.");
		return;
	}
	/* Mounting again finds a card put in since; the driver keeps a mount
	 * that's still good. */
	if(device->init(device->initial) != 0) {
		switch(device->initial->status) {
			case CARD_ERROR_NOCARD:
				snprintf(place->note[0], sizeof(place->note[0]),
					"No memory card in %s", slotName(slot));
				snprintf(place->note[1], sizeof(place->note[1]),
					"Put one in, then press L or R to look again.");
				break;
			case CARD_ERROR_WRONGDEVICE:
				snprintf(place->note[0], sizeof(place->note[0]),
					"%s holds something else", slotName(slot));
				snprintf(place->note[1], sizeof(place->note[1]),
					"It isn't a memory card.");
				break;
			default:
				snprintf(place->note[0], sizeof(place->note[0]),
					"The memory card in %s can't be read", slotName(slot));
				snprintf(place->note[1], sizeof(place->note[1]),
					"It may need checking on the console's own screen.");
				break;
		}
		return;
	}
	place->mounted = true;
	place->entryCount = device->readDir(device->initial, &place->entries, -1);
	if(place->entryCount < 1) {
		place->entryCount = 0;
		snprintf(place->note[0], sizeof(place->note[0]),
			"The memory card in %s can't be read", slotName(slot));
		return;
	}
	/* entries[0] is the driver's "..". The card's own order, as the IPL. */
	for(i = 1; i < place->entryCount && place->count < SAVES_LIST_MAX; i++) {
		place->list[place->count++] = &place->entries[i];
		used += (int)((place->entries[i].size + UI_SAVES_BLOCK_SIZE - 1u) /
			UI_SAVES_BLOCK_SIZE);
	}
	info = device->info(device->initial);
	place->totalBlocks = (int)(info->totalSpace / UI_SAVES_BLOCK_SIZE) -
		SAVES_SYSTEM_BLOCKS;
	place->freeBlocks = place->totalBlocks > used ?
		place->totalBlocks - used : 0;
	place->ready = true;
	if(place->count == 0) {
		snprintf(place->note[0], sizeof(place->note[0]),
			"No saves on this memory card");
	}
	if(place->selection >= place->count) {
		place->selection = place->count > 0 ? place->count - 1 : 0;
	}
}

/* The parent, then folders, then saves, each by name. */
static int placeOrder(const void *a, const void *b)
{
	const file_handle *x = *(file_handle *const *)a;
	const file_handle *y = *(file_handle *const *)b;
	int rankX = x->fileType == IS_SPECIAL ? 0 : x->fileType == IS_DIR ? 1 : 2;
	int rankY = y->fileType == IS_SPECIAL ? 0 : y->fileType == IS_DIR ? 1 : 2;

	if(rankX != rankY) {
		return rankX - rankY;
	}
	return strcasecmp(getRelativeName((char *)x->name),
		getRelativeName((char *)y->name));
}

static bool folderIsRoot(const savesPlace_t *place)
{
	return !strcmp(place->dir.name, place->device->initial->name);
}

/* place->dir names the folder; lists it. */
static void loadFolder(savesPlace_t *place, bool foldersOnly)
{
	int i;

	placeClear(place);
	place->device = devices[DEVICE_CONFIG];
	if(place->device == NULL) {
		snprintf(place->note[0], sizeof(place->note[0]),
			"No device for settings and saves");
		snprintf(place->note[1], sizeof(place->note[1]),
			"Choose one in Settings, Storage.");
		return;
	}
	place->entryCount = place->device->readDir(&place->dir, &place->entries, -1);
	if(place->entryCount < 0) {
		place->entryCount = 0;
		snprintf(place->note[0], sizeof(place->note[0]),
			"This folder can't be read");
		return;
	}
	for(i = 0; i < place->entryCount && place->count < SAVES_LIST_MAX; i++) {
		file_handle *entry = &place->entries[i];
		const char *leaf = getRelativeName(entry->name);

		if(entry->fileType == IS_SPECIAL) {
			if(!folderIsRoot(place)) {
				place->list[place->count++] = entry;
			}
		}
		/* A dot name is a system file, like macOS's ._ copies of saves. */
		else if(leaf[0] != '.' && (entry->fileType == IS_DIR ||
			(!foldersOnly && entry->fileType == IS_FILE && isSaveName(leaf)))) {
			place->list[place->count++] = entry;
		}
	}
	qsort(place->list, (size_t)place->count, sizeof(place->list[0]),
		placeOrder);
	place->ready = true;
	if(place->count == 0 || (place->count == 1 &&
		place->list[0]->fileType == IS_SPECIAL)) {
		snprintf(place->note[0], sizeof(place->note[0]), foldersOnly ?
			"No folders in here" : "No saves in this folder");
	}
	if(place->selection >= place->count) {
		place->selection = place->count > 0 ? place->count - 1 : 0;
	}
}

/* The folder under the settings device's root: "swiss/saves", "/" for the
 * root. */
static void folderSet(savesPlace_t *place, const char *path)
{
	memset(&place->dir, 0, sizeof(place->dir));
	place->device = devices[DEVICE_CONFIG];
	if(place->device != NULL) {
		concat_path(place->dir.name, place->device->initial->name, path);
	}
	place->dir.fileType = IS_DIR;
	place->dir.device = place->device;
	place->selection = 0;
}

/* The path under the device's root that folderSet takes back. */
static void folderPath(const file_handle *dir, char *out, size_t size)
{
	const char *path = getDevicePath((char *)dir->name);

	snprintf(out, size, "%s", !strcmp(path, "/") || path[0] == '\0' ? "/" :
		path[0] == '/' ? path + 1 : path);
}

/* Each folder along path, made where it's missing (the default Save Folder
 * on a card that hasn't had one yet); makeDir leaves one that's there. */
static void folderEnsure(DEVICEHANDLER_INTERFACE *device, const char *path)
{
	file_handle dir;
	const char *slash;
	size_t root;

	if(device == NULL || device->makeDir == NULL || path[0] == '\0' ||
		!strcmp(path, "/")) {
		return;
	}
	memset(&dir, 0, sizeof(dir));
	dir.fileType = IS_DIR;
	dir.device = device;
	concat_path(dir.name, device->initial->name, path);
	root = strlen(dir.name) - strlen(path);
	for(slash = strchr(path, '/'); slash != NULL; slash = strchr(slash + 1, '/')) {
		size_t end = root + (size_t)(slash - path);
		char kept = dir.name[end];

		dir.name[end] = '\0';
		device->makeDir(&dir);
		dir.name[end] = kept;
	}
	device->makeDir(&dir);
}

/* Reads banners and comments for the rows on screen and lets the rest go,
 * so a long list holds six at a time. */
static void placeWindow(savesPlace_t *place, int first)
{
	int i;

	for(i = 0; i < place->count; i++) {
		file_handle *entry = place->list[i];

		if(entry->fileType != IS_FILE) {
			continue;
		}
		if(i >= first && i < first + UI_SAVES_PAGE_ROWS) {
			if(entry->meta == NULL) {
				populate_meta(entry);
				place->device->closeFile(entry);
			}
		}
		else if(entry->meta != NULL) {
			meta_free(entry->meta);
			entry->meta = NULL;
		}
	}
}

/* ------------------------------------------------------------------------
 * What the page shows.
 * --------------------------------------------------------------------- */

/* The first line of its comment names a save the way its game does. */
static void saveTitle(char *out, size_t size, file_handle *save)
{
	const char *comment = save->meta ? save->meta->bannerDesc.description : "";
	const char *end = strchr(comment, '\n');
	int length = end ? (int)(end - comment) : (int)strlen(comment);

	if(length > 0) {
		snprintf(out, size, "%.*s", length, comment);
	}
	else if(isCard(save->device)) {
		snprintf(out, size, "%.*s", CARD_FILENAMELEN,
			((card_dir *)save->other)->filename);
	}
	else {
		snprintf(out, size, "%s", getRelativeName(save->name));
	}
}

/* The comment's second line, and what the save is on the card or in the
 * folder: its game code, or its file name. */
static void saveDetail(char *out, size_t size, file_handle *save)
{
	const char *comment = save->meta ? save->meta->bannerDesc.description : "";
	const char *second = strchr(comment, '\n');
	char what[64];

	if(isCard(save->device)) {
		card_dir *dir = (card_dir *)save->other;
		snprintf(what, sizeof(what), "%.4s%.2s", (const char *)dir->gamecode,
			(const char *)dir->company);
	}
	else {
		snprintf(what, sizeof(what), "%s", getRelativeName(save->name));
	}
	if(second != NULL && second[1] != '\0') {
		snprintf(out, size, "%s   \267   %s", second + 1, what);
	}
	else {
		snprintf(out, size, "%s", what);
	}
}

static unsigned saveBlocks(const file_handle *save)
{
	unsigned blocks = save->size / UI_SAVES_BLOCK_SIZE;

	/* A .gci's header adds 64 bytes to its whole blocks. */
	return isCard(save->device) ?
		(save->size + UI_SAVES_BLOCK_SIZE - 1u) / UI_SAVES_BLOCK_SIZE :
		(blocks > 0u ? blocks : 1u);
}

static void pageRow(uiSavesPageRow_t *row, savesPlace_t *place,
	file_handle *entry)
{
	char text[PATHNAME_MAX];

	if(entry->fileType == IS_SPECIAL) {
		char parent[PATHNAME_MAX];

		row->kind = UI_SAVES_ROW_PARENT;
		getParentPath(place->dir.name, parent);
		snprintf(text, sizeof(text), "Up to %s", getDevicePath(parent));
	}
	else if(entry->fileType == IS_DIR) {
		row->kind = UI_SAVES_ROW_FOLDER;
		snprintf(text, sizeof(text), "%s", getRelativeName(entry->name));
		snprintf(row->blocks, sizeof(row->blocks), "Folder");
	}
	else {
		unsigned blocks = saveBlocks(entry);

		row->kind = UI_SAVES_ROW_SAVE;
		saveTitle(text, sizeof(text), entry);
		snprintf(row->blocks, sizeof(row->blocks), "%u block%s", blocks,
			blocks == 1u ? "" : "s");
		if(entry->meta != NULL && entry->meta->banner != NULL) {
			if(entry->meta->bannerSize == CARD_BANNER_W * CARD_BANNER_H * 2) {
				row->bannerFormat = CARD_BANNER_RGB;
				memcpy(row->banner, entry->meta->banner,
					CARD_BANNER_W * CARD_BANNER_H * 2);
			}
			else if(entry->meta->bannerSize ==
				CARD_BANNER_W * CARD_BANNER_H + 512) {
				row->bannerFormat = CARD_BANNER_CI;
				memcpy(row->banner, entry->meta->banner,
					CARD_BANNER_W * CARD_BANNER_H + 512);
			}
		}
	}
	UICheats_Fit(row->title, sizeof(row->title), text, 330, 0.62f,
		GetTextSizeInPixels);
}

/* tabCount 0 is the folder chooser: one place, folders only. */
static void pageBuild(savesPlace_t *place, int tab, int tabCount,
	const char *warning)
{
	static const char *const tabNames[SAVES_TABS] = {
		"SLOT A", "SLOT B", "SD CARD"
	};
	uiSavesPageSnapshot_t *s = &shown;
	int first = UICheats_WindowStart(place->selection, place->count);
	file_handle *focus = place->count > 0 ? place->list[place->selection] : NULL;
	char text[PATHNAME_MAX];
	int i;

	placeWindow(place, first);
	memset(s, 0, sizeof(*s));
	snprintf(s->title, sizeof(s->title), tabCount ? "Memory Cards" :
		"Choose a Folder");
	s->tabCount = (s8)tabCount;
	s->tab = (s8)tab;
	for(i = 0; i < tabCount; i++) {
		snprintf(s->tabs[i], sizeof(s->tabs[i]), "%s", tabNames[i]);
	}
	if(isCard(place->device)) {
		if(place->ready) {
			snprintf(s->status, sizeof(s->status), "%d block%s free",
				place->freeBlocks, place->freeBlocks == 1 ? "" : "s");
			snprintf(s->section, sizeof(s->section), "MEMORY CARD %d",
				place->totalBlocks);
		}
	}
	else if(place->device != NULL) {
		snprintf(s->status, sizeof(s->status), "%s",
			DeviceDisplayName(place->device));
		UICheats_Fit(s->section, sizeof(s->section),
			getDevicePath(place->dir.name), 420, 0.42f, GetTextSizeInPixels);
	}
	if(place->count > 0) {
		snprintf(s->position, sizeof(s->position), "%d / %d",
			place->selection + 1, place->count);
	}
	for(i = 0; i < UI_SAVES_PAGE_ROWS && first + i < place->count; i++) {
		pageRow(&s->rows[i], place, place->list[first + i]);
		s->rowCount++;
	}
	s->first = first;
	s->count = place->count;
	s->list = place->listing;
	s->focusRow = (s8)(place->selection - first);
	if(s->rowCount == 0 || (place->count == 1 &&
		place->list[0]->fileType == IS_SPECIAL)) {
		snprintf(s->empty[0], sizeof(s->empty[0]), "%s", place->note[0]);
		snprintf(s->empty[1], sizeof(s->empty[1]), "%s", place->note[1]);
	}
	if(warning != NULL) {
		UICheats_Fit(s->detail, sizeof(s->detail), warning, 560, 0.48f,
			GetTextSizeInPixels);
		s->warning = 1;
	}
	else if(focus != NULL && focus->fileType == IS_FILE) {
		saveDetail(text, sizeof(text), focus);
		UICheats_Fit(s->detail, sizeof(s->detail), text, 560, 0.48f,
			GetTextSizeInPixels);
	}
	if(tabCount == 0) {
		snprintf(s->hint[0], sizeof(s->hint[0]), "%sX  Choose this folder",
			focus != NULL ? "A  Open   " : "");
		snprintf(s->hint[1], sizeof(s->hint[1]), "B  Cancel");
	}
	else {
		snprintf(s->hint[0], sizeof(s->hint[0]), "%sB  Back",
			focus == NULL ? "" : focus->fileType == IS_FILE ?
			"A  Options   " : "A  Open   ");
		snprintf(s->hint[1], sizeof(s->hint[1]), "L/R  Switch");
	}
}

static void pageShow(uiDrawObj_t **page)
{
	if(*page == NULL) {
		if((*page = DrawSavesPage(&shown)) != NULL) {
			DrawPublish(*page);
		}
	}
	else {
		DrawUpdateSavesPage(*page, &shown);
	}
}

/* ------------------------------------------------------------------------
 * Reading, writing and checking saves.
 * --------------------------------------------------------------------- */

/* The whole save as the .gci layout: a card's entry and blocks, or a file's
 * bytes as they are. The caller frees it. */
static u8 *saveRead(file_handle *save, u32 *length)
{
	bool card = isCard(save->device);
	u32 want = card ? save->size + UI_SAVES_ENTRY_SIZE : save->size;
	u8 *data;
	s32 got;

	if(want <= UI_SAVES_ENTRY_SIZE || want > SAVES_MAX_BYTES ||
		(data = memalign(32, want)) == NULL) {
		return NULL;
	}
	save->device->seekFile(save, 0, DEVICE_HANDLER_SEEK_SET);
	if(card) {
		setCopyGCIMode(true);
	}
	got = save->device->readFile(save, data, want);
	if(card) {
		setCopyGCIMode(false);
	}
	save->device->closeFile(save);
	if(got != (s32)want) {
		free(data);
		return NULL;
	}
	*length = want;
	return data;
}

/* The save on the card in slot whose game, maker and name are entry's.
 * Reads the card's list into *entries, which the caller frees. */
static file_handle *cardFind(int slot, const u8 *entry, file_handle **entries,
	int *count, int *usedBlocks)
{
	DEVICEHANDLER_INTERFACE *device = slotDevice(slot);
	file_handle *found = NULL;
	int i;

	*entries = NULL;
	*usedBlocks = 0;
	*count = device->readDir(device->initial, entries, -1);
	for(i = 1; i < *count; i++) {
		card_dir *dir = (card_dir *)(*entries)[i].other;

		*usedBlocks += (int)(((*entries)[i].size + UI_SAVES_BLOCK_SIZE - 1u) /
			UI_SAVES_BLOCK_SIZE);
		if(!memcmp(dir->gamecode, entry, 4) &&
			!memcmp(dir->company, entry + 4, 2) &&
			!strncmp(dir->filename, (const char *)entry + 8, CARD_FILENAMELEN)) {
			found = &(*entries)[i];
		}
	}
	return found;
}

static const char *cardWhy(s32 error)
{
	switch(error) {
		case CARD_ERROR_INSSPACE: return "not enough free blocks";
		case CARD_ERROR_NOENT: return "no room for another save";
		case CARD_ERROR_EXIST: return "it already has this save";
		case CARD_ERROR_NOCARD: return "the card was taken out";
		default: return "the card didn't take it";
	}
}

/* Writes a save to the card in slot and reads it back. The driver creates
 * it from entry (game, maker, name, banner and icon places) and writes
 * blocks, the save's data. */
static bool cardWrite(int slot, const u8 *entry, const u8 *blocks,
	u32 blockBytes, char *why, size_t whySize)
{
	DEVICEHANDLER_INTERFACE *device = slotDevice(slot);
	file_handle dest;
	file_handle *entries = NULL;
	file_handle *copy;
	int count, used, total;
	u8 *back;
	u32 backLength = 0;
	s32 written;
	bool has, same;

	memset(&dest, 0, sizeof(dest));
	if(device->init(device->initial) != 0) {
		snprintf(why, whySize, "%s has no memory card.", slotName(slot));
		return false;
	}
	places[slot].mounted = true;
	/* The driver would write over a save of the same name in place, whatever
	 * its size, so one already there stops the copy. */
	has = cardFind(slot, entry, &entries, &count, &used) != NULL;
	free(entries);
	total = (int)(device->info(device->initial)->totalSpace /
		UI_SAVES_BLOCK_SIZE) - SAVES_SYSTEM_BLOCKS;
	if(count < 1) {
		snprintf(why, whySize, "The memory card in %s can't be read.",
			slotName(slot));
		return false;
	}
	if(has) {
		snprintf(why, whySize, "%s already has this save.", slotName(slot));
		return false;
	}
	if(count - 1 >= CARD_MAXFILES) {
		snprintf(why, whySize, "%s has no room for another save.",
			slotName(slot));
		return false;
	}
	if((int)UISaves_Blocks(entry) > total - used) {
		snprintf(why, whySize, "%s has %d free block%s; this needs %u.",
			slotName(slot), total > used ? total - used : 0,
			total - used == 1 ? "" : "s", UISaves_Blocks(entry));
		return false;
	}
	concatf_path(dest.name, device->initial->name, "%.*s", CARD_FILENAMELEN,
		(const char *)entry + 8);
	dest.device = device;
	setGCIInfo(entry);
	written = device->writeFile(&dest, blocks, blockBytes);
	setGCIInfo(NULL);
	if(written != (s32)blockBytes) {
		/* The save wasn't there before, so what the driver left is ours. */
		if((copy = cardFind(slot, entry, &entries, &count, &used)) != NULL) {
			device->deleteFile(copy);
		}
		free(entries);
		snprintf(why, whySize, "%s: %s.", slotName(slot),
			cardWhy(written < 0 ? written : CARD_ERROR_FATAL_ERROR));
		return false;
	}
	/* Read it back from the card before calling it copied. */
	copy = cardFind(slot, entry, &entries, &count, &used);
	back = copy != NULL ? saveRead(copy, &backLength) : NULL;
	same = back != NULL && backLength == blockBytes + UI_SAVES_ENTRY_SIZE &&
		!memcmp(back + UI_SAVES_ENTRY_SIZE, blocks, blockBytes);
	free(back);
	if(!same) {
		if(copy != NULL) {
			device->deleteFile(copy);
		}
		snprintf(why, whySize, "The copy on %s didn't read back the same.",
			slotName(slot));
	}
	free(entries);
	return same;
}

/* Writes data to a new file named name in folder (on the settings device),
 * as name_2 and so on while a name is taken, and reads it back. */
static bool folderWrite(const char *folder, const char *name, const u8 *data,
	u32 length, char *why, size_t whySize)
{
	DEVICEHANDLER_INTERFACE *device = devices[DEVICE_CONFIG];
	file_handle dest;
	char leaf[PATHNAME_MAX];
	u8 *back;
	int attempt;
	bool same;

	for(attempt = 1; attempt < 100; attempt++) {
		memset(&dest, 0, sizeof(dest));
		dest.device = device;
		UISaves_NumberedName(leaf, sizeof(leaf), name, attempt);
		concat_path(dest.name, folder, leaf);
		if(device->statFile(&dest) != 0) {
			break;
		}
	}
	if(attempt == 100) {
		snprintf(why, whySize, "That folder has 99 copies of this save.");
		return false;
	}
	memset(&dest, 0, sizeof(dest));
	dest.device = device;
	concat_path(dest.name, folder, leaf);
	if(device->writeFile(&dest, data, length) != (s32)length ||
		device->closeFile(&dest) != 0) {
		device->closeFile(&dest);
		device->deleteFile(&dest);
		snprintf(why, whySize, "The SD card didn't take the save.");
		return false;
	}
	back = memalign(32, length);
	dest.offset = 0;
	same = back != NULL && device->readFile(&dest, back, length) == (s32)length &&
		!memcmp(back, data, length);
	device->closeFile(&dest);
	free(back);
	if(!same) {
		device->deleteFile(&dest);
		snprintf(why, whySize, "The copy didn't read back the same.");
		return false;
	}
	return true;
}

static bool saveDelete(file_handle *save)
{
	save->device->closeFile(save);
	return save->device->deleteFile(save) == 0;
}

/* The folder a Copy or Move goes to, as a path on the settings device. */
static bool destinationFolder(uiSavesPlace_t to, char *path, size_t size);

/* Copy or Move save, from tab, to a card or folder. */
static void saveTransfer(file_handle *save, int tab, uiSavesPlace_t to,
	bool move)
{
	char why[128] = "";
	char done[PATHNAME_MAX + 64];
	char folder[PATHNAME_MAX];
	char name[PATHNAME_MAX];
	u8 entry[UI_SAVES_ENTRY_SIZE];
	uiDrawObj_t *progress;
	u8 *data;
	u32 length = 0;
	size_t blocksAt;
	bool ok;

	if(move && isCard(save->device) && (((card_dir *)save->other)->permissions &
		(CARD_ATTRIB_NOCOPY | CARD_ATTRIB_NOMOVE))) {
		savesTell(D_WARN, "This game doesn't let its save move.\n"
			"Copy it instead.\nPress A to continue.");
		return;
	}
	if(to >= UI_SAVES_PLACE_FOLDER && !destinationFolder(to, folder,
		sizeof(folder))) {
		return;
	}
	/* A folder's save moving to another folder on the same card only needs
	 * renaming there. */
	if(move && tab == SAVES_TAB_FOLDER && to >= UI_SAVES_PLACE_FOLDER) {
		char source[PATHNAME_MAX];
		file_handle dest;
		int attempt;

		getParentPath(save->name, source);
		if(!strcmp(source, folder)) {
			savesTell(D_INFO, "It's already in that folder.\n"
				"Press A to continue.");
			return;
		}
		for(attempt = 1; attempt < 100; attempt++) {
			memset(&dest, 0, sizeof(dest));
			UISaves_NumberedName(name, sizeof(name),
				getRelativeName(save->name), attempt);
			concat_path(dest.name, folder, name);
			if(save->device->statFile(&dest) != 0) {
				break;
			}
		}
		save->device->closeFile(save);
		ok = attempt < 100 && save->device->renameFile(save, dest.name) == 0;
		savesTell(ok ? D_INFO : D_FAIL, ok ? "Moved.\nPress A to continue." :
			"It couldn't be moved.\nPress A to continue.");
		return;
	}
	progress = DrawPublish(DrawProgressBar(true, 0, move ? "Moving\205" :
		"Copying\205"));
	data = saveRead(save, &length);
	blocksAt = 0u;
	if(data != NULL && isCard(save->device)) {
		/* The card driver read it as a .gci: its entry, then its blocks. */
		memcpy(entry, data, UI_SAVES_ENTRY_SIZE);
		blocksAt = UI_SAVES_ENTRY_SIZE;
	}
	else if(data != NULL) {
		blocksAt = UISaves_FindEntry(data, length, entry);
	}
	if(data == NULL) {
		snprintf(why, sizeof(why), "The save couldn't be read.");
		ok = false;
	}
	else if(blocksAt == 0u) {
		snprintf(why, sizeof(why), "This file isn't a GameCube save.");
		ok = false;
	}
	else if(to <= UI_SAVES_PLACE_SLOT_B) {
		ok = cardWrite(to == UI_SAVES_PLACE_SLOT_B, entry, data + blocksAt,
			length - (u32)blocksAt, why, sizeof(why));
		snprintf(done, sizeof(done), "%s to %s.", move ? "Moved" : "Copied",
			slotName(to == UI_SAVES_PLACE_SLOT_B));
	}
	else {
		/* A card's save becomes a .gci; a file keeps its name and form. */
		if(isCard(save->device)) {
			UISaves_FileName(name, sizeof(name), entry);
		}
		else {
			snprintf(name, sizeof(name), "%s", getRelativeName(save->name));
		}
		folderEnsure(devices[DEVICE_CONFIG], getDevicePath(folder) + 1);
		ok = folderWrite(folder, name, data, length, why, sizeof(why));
		snprintf(done, sizeof(done), "%s to %s.", move ? "Moved" : "Copied",
			getDevicePath(folder));
	}
	free(data);
	if(ok && move && !saveDelete(save)) {
		DrawDispose(progress);
		savesTell(D_WARN, "Copied, but the original couldn't be removed.\n"
			"Press A to continue.");
		return;
	}
	DrawDispose(progress);
	if(ok) {
		strlcat(done, "\nPress A to continue.", sizeof(done));
		savesTell(D_INFO, done);
	}
	else {
		snprintf(done, sizeof(done), "%s\nNothing was %s.\nPress A to continue.",
			why, move ? "moved" : "changed");
		savesTell(D_FAIL, done);
	}
}

/* ------------------------------------------------------------------------
 * The folder chooser, and the explorer.
 * --------------------------------------------------------------------- */

/* Browses the settings device's folders from start; X chooses the one open.
 * The device must be mounted. */
static bool chooseFolder(const char *start, char *path, size_t size)
{
	uiDrawObj_t *page = NULL;
	savesInput_t input;
	bool chosen = false;

	folderSet(&chooser, start);
	loadFolder(&chooser, true);
	if(!chooser.ready) {
		/* The folder is gone: start at the root. */
		folderSet(&chooser, "/");
		loadFolder(&chooser, true);
	}
	inputInit(&input);
	while(1) {
		u32 pressed;
		file_handle *focus;

		pageBuild(&chooser, 0, 0, NULL);
		pageShow(&page);
		pressed = inputNext(&input);
		focus = chooser.count > 0 ? chooser.list[chooser.selection] : NULL;
		if(pressed & BUTTON_B) {
			break;
		}
		if((pressed & BUTTON_X) && chooser.ready) {
			folderPath(&chooser.dir, path, size);
			chosen = true;
			break;
		}
		if((pressed & BUTTON_A) && focus != NULL) {
			if(focus->fileType == IS_SPECIAL) {
				getParentPath(chooser.dir.name, chooser.dir.name);
			}
			else {
				snprintf(chooser.dir.name, sizeof(chooser.dir.name), "%s",
					focus->name);
			}
			chooser.selection = 0;
			loadFolder(&chooser, true);
		}
		else if(pressed & (BUTTON_UP | BUTTON_LEFT)) {
			chooser.selection = UICheats_Move(chooser.selection, chooser.count,
				-1, (pressed & BUTTON_LEFT) != 0u);
		}
		else if(pressed & (BUTTON_DOWN | BUTTON_RIGHT)) {
			chooser.selection = UICheats_Move(chooser.selection, chooser.count,
				1, (pressed & BUTTON_RIGHT) != 0u);
		}
	}
	placeClear(&chooser);
	DrawDispose(page);
	return chosen;
}

static bool destinationFolder(uiSavesPlace_t to, char *path, size_t size)
{
	char chosen[PATHNAME_MAX];

	if(to == UI_SAVES_PLACE_FOLDER) {
		snprintf(chosen, sizeof(chosen), "%s", saves_folder());
	}
	else if(!chooseFolder(saves_folder(), chosen, sizeof(chosen))) {
		return false;
	}
	concat_path(path, devices[DEVICE_CONFIG]->initial->name, chosen);
	return true;
}

bool saves_choose_folder(char *folder, size_t size)
{
	bool chosen;

	if(!config_set_device()) {
		savesTell(D_FAIL, "There's no device for settings and saves.\n"
			"Press A to continue.");
		return false;
	}
	chosen = chooseFolder(saves_folder(), folder, size);
	config_unset_device();
	return chosen;
}

static bool inSaveFolder(const savesPlace_t *place)
{
	char path[PATHNAME_MAX];

	folderPath(&place->dir, path, sizeof(path));
	return !strcmp(path, saves_folder());
}

/* A on a save: Copy, Move or Delete, then where to. Returns whether a list
 * may have changed. */
static bool saveOptions(int tab)
{
	static const char *const actions[3] = {"Copy", "Move", "Delete"};
	static const char *const confirm[2] = {"Cancel", "Delete"};
	savesPlace_t *place = &places[tab];
	file_handle *save = place->list[place->selection];
	uiSavesPlace_t to[UI_SAVES_PLACE_COUNT];
	const char *toNames[UI_SAVES_PLACE_COUNT];
	char title[96];
	char saveFolder[PATHNAME_MAX + 16];
	int count, action, choice, i;

	saveTitle(title, sizeof(title), save);
	action = savesPick(title, actions, 3, 0);
	if(action < 0) {
		return false;
	}
	if(action == 2) {
		if(savesPick("Delete this save?", confirm, 2, 0) != 1) {
			return false;
		}
		if(!saveDelete(save)) {
			savesTell(D_FAIL, "The save couldn't be deleted.\n"
				"Press A to continue.");
		}
		return true;
	}
	count = UISaves_Destinations(tab == 0 ? UI_SAVES_PLACE_SLOT_A :
		tab == 1 ? UI_SAVES_PLACE_SLOT_B : UI_SAVES_PLACE_FOLDER,
		places[0].ready, places[1].ready, places[SAVES_TAB_FOLDER].device != NULL &&
		(places[SAVES_TAB_FOLDER].device->features & FEAT_WRITE),
		tab == SAVES_TAB_FOLDER && inSaveFolder(place), to);
	if(count == 0) {
		savesTell(D_INFO, "There's nowhere else to put it.\n"
			"Put a memory card in the other slot.\nPress A to continue.");
		return false;
	}
	snprintf(saveFolder, sizeof(saveFolder), "Save Folder  /%s",
		strcmp(saves_folder(), "/") ? saves_folder() : "");
	for(i = 0; i < count; i++) {
		toNames[i] = to[i] == UI_SAVES_PLACE_SLOT_A ? "Slot A" :
			to[i] == UI_SAVES_PLACE_SLOT_B ? "Slot B" :
			to[i] == UI_SAVES_PLACE_FOLDER ? saveFolder : "Another folder\205";
	}
	choice = savesPick(action == 1 ? "Move to" : "Copy to", toNames, count, 0);
	if(choice < 0) {
		return false;
	}
	saveTransfer(save, tab, to[choice], action == 1);
	return true;
}

static void loadTab(int tab)
{
	if(tab == SAVES_TAB_FOLDER) {
		loadFolder(&places[tab], false);
	}
	else {
		loadCard(&places[tab], tab);
	}
}

void show_saves(void)
{
	uiDrawObj_t *page = NULL;
	savesInput_t input;
	bool folderMounted = config_set_device();
	int tab, i;

	memset(places, 0, sizeof(places));
	if(folderMounted) {
		folderSet(&places[SAVES_TAB_FOLDER], saves_folder());
		folderEnsure(devices[DEVICE_CONFIG], saves_folder());
	}
	for(i = 0; i < SAVES_TABS; i++) {
		if(i < SAVES_TAB_FOLDER || folderMounted) {
			loadTab(i);
		}
		else {
			places[i].device = NULL;
			snprintf(places[i].note[0], sizeof(places[i].note[0]),
				"No device for settings and saves");
			snprintf(places[i].note[1], sizeof(places[i].note[1]),
				"Choose one in Settings, Storage.");
		}
	}
	if(folderMounted && !places[SAVES_TAB_FOLDER].ready) {
		/* A Save Folder that can't be read or made: start at the root. */
		folderSet(&places[SAVES_TAB_FOLDER], "/");
		loadTab(SAVES_TAB_FOLDER);
	}
	/* Slot A first, as the IPL opens. */
	tab = 0;
	inputInit(&input);
	while(1) {
		savesPlace_t *place = &places[tab];
		file_handle *focus = place->count > 0 ? place->list[place->selection] :
			NULL;
		u32 pressed;

		pageBuild(place, tab, SAVES_TABS, NULL);
		pageShow(&page);
		pressed = inputNext(&input);
		if(pressed & BUTTON_B) {
			break;
		}
		if(pressed & (BUTTON_L | BUTTON_R)) {
			tab = (tab + ((pressed & BUTTON_R) ? 1 : SAVES_TABS - 1)) % SAVES_TABS;
			if(tab < SAVES_TAB_FOLDER || folderMounted) {
				loadTab(tab);
			}
		}
		else if((pressed & BUTTON_A) && focus != NULL) {
			if(focus->fileType == IS_FILE) {
				if(saveOptions(tab)) {
					/* A save may have gone or come anywhere. */
					for(i = 0; i < SAVES_TABS; i++) {
						if(i < SAVES_TAB_FOLDER || folderMounted) {
							loadTab(i);
						}
					}
				}
			}
			else {
				if(focus->fileType == IS_SPECIAL) {
					getParentPath(place->dir.name, place->dir.name);
				}
				else {
					snprintf(place->dir.name, sizeof(place->dir.name), "%s",
						focus->name);
				}
				place->selection = 0;
				loadFolder(place, false);
			}
			inputInit(&input);
		}
		else if(pressed & (BUTTON_UP | BUTTON_LEFT)) {
			place->selection = UICheats_Move(place->selection, place->count, -1,
				(pressed & BUTTON_LEFT) != 0u);
		}
		else if(pressed & (BUTTON_DOWN | BUTTON_RIGHT)) {
			place->selection = UICheats_Move(place->selection, place->count, 1,
				(pressed & BUTTON_RIGHT) != 0u);
		}
	}
	for(i = 0; i < SAVES_TABS; i++) {
		placeClear(&places[i]);
		/* Unmount what this screen mounted, unless it's the source. */
		if(i < SAVES_TAB_FOLDER && places[i].mounted &&
			devices[DEVICE_CUR] != slotDevice(i)) {
			slotDevice(i)->deinit(slotDevice(i)->initial);
		}
	}
	if(folderMounted) {
		config_unset_device();
	}
	DrawDispose(page);
	while(padsButtonsHeld() & BUTTON_B) {
		VIDEO_WaitVSync();
	}
}
