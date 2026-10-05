#include "saves_stats.h"
#include "saves.h"
#include "saves_raw.h"
#include "deviceHandler.h"
#include "config.h"
#include "files.h"
#include "main.h"
#include "util.h"

#include <ogc/card.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* The folder listing is the device's normal readDir allocation, but only
 * these first entries/images are examined. Never read a complete save. */
#define SAVES_STATS_FILES 256
#define SAVES_STATS_IMAGES 16u

static bool statsCardDevice(const DEVICEHANDLER_INTERFACE *device)
{
	return device == &__device_card_a || device == &__device_card_b;
}

static bool statsSlotInUse(unsigned slot)
{
	u32 location = slot == 0u ? LOC_MEMCARD_SLOT_A : LOC_MEMCARD_SLOT_B;
	DEVICEHANDLER_INTERFACE *users[2] = {devices[DEVICE_CUR], devices[DEVICE_CONFIG]};
	unsigned i;

	for(i = 0u; i < 2u; i++) {
		if(users[i] != NULL && !statsCardDevice(users[i]) &&
			(users[i]->location & location) != 0u) return true;
	}
	return false;
}

static void statsWrite32(u8 *out, u32 value)
{
	out[0] = (u8)(value >> 24);
	out[1] = (u8)(value >> 16);
	out[2] = (u8)(value >> 8);
	out[3] = (u8)value;
}

static void statsSlot(const char gameId[6], uiSavesGameStats_t *stats,
	unsigned slot)
{
	DEVICEHANDLER_INTERFACE *device = slot == 0u ? &__device_card_a : &__device_card_b;
	card_dir dir;
	s32 result;
	unsigned count = 0u;

	if(statsSlotInUse(slot)) return;
	/* Do not wait on a busy slot or probe an SD adapter as a memory card. */
	result = CARD_ProbeEx((s32)slot, NULL, NULL);
	if(result == CARD_ERROR_NOCARD || result == CARD_ERROR_WRONGDEVICE) return;
	if(result != CARD_ERROR_READY || device->init(device->initial) != 0) {
		stats->partial = true;
		return;
	}
	memset(&dir, 0, sizeof(dir));
	result = CARD_FindFirst((s32)slot, &dir, true);
	while(result == CARD_ERROR_READY && count < UI_SAVES_CARD_FILES) {
		u8 entry[UI_SAVES_ENTRY_SIZE] = {0};
		card_stat status;
		unsigned blocks = dir.filelen / UI_SAVES_BLOCK_SIZE;

		count++;
		if(dir.filelen == 0u || dir.filelen % UI_SAVES_BLOCK_SIZE != 0u ||
			blocks > 2043u || (unsigned)dir.fileno >= UI_SAVES_CARD_FILES) {
			stats->partial = true;
			break;
		}
		if(!memcmp(dir.gamecode, gameId, 4u) && !memcmp(dir.company, gameId + 4, 2u)) {
			memcpy(entry, dir.gamecode, 4u);
			memcpy(entry + 4, dir.company, 2u);
			entry[0x38] = (u8)(blocks >> 8);
			entry[0x39] = (u8)blocks;
			if(CARD_GetStatus((s32)slot, dir.fileno, &status) == CARD_ERROR_READY &&
				!memcmp(status.gamecode, dir.gamecode, 4u) &&
				!memcmp(status.company, dir.company, 2u) &&
				!memcmp(status.filename, dir.filename, UI_SAVES_NAME_LENGTH) &&
				status.len == dir.filelen) {
				statsWrite32(entry + 0x28, status.time);
			}
			else stats->partial = true;
			UISaves_StatsAdd(stats, entry, gameId, slot);
		}
		result = CARD_FindNext(&dir);
	}
	if(result == CARD_ERROR_NOFILE) stats->checkedSources |= (u8)(1u << slot);
	else stats->partial = true;
	/* Preserve mounts used by the game or settings device. */
	if(device != devices[DEVICE_CUR] && device != devices[DEVICE_CONFIG]) {
		device->deinit(device->initial);
	}
}

static bool statsSaveName(const char *name)
{
	size_t length = strlen(name);

	return length > 4u && (!strcasecmp(name + length - 4u, ".gci") ||
		!strcasecmp(name + length - 4u, ".gcs") ||
		!strcasecmp(name + length - 4u, ".sav"));
}

static bool statsReadEntry(file_handle *file, u8 *head,
	u8 entry[UI_SAVES_ENTRY_SIZE])
{
	u32 want = file->size < UI_SAVES_HEAD_SIZE ? file->size : UI_SAVES_HEAD_SIZE;
	bool ok;

	if(file->size < UI_SAVES_ENTRY_SIZE ||
		file->size > UI_SAVES_HEAD_SIZE + 2043u * UI_SAVES_BLOCK_SIZE) return false;
	ok = file->device->seekFile(file, 0, DEVICE_HANDLER_SEEK_SET) == 0 &&
		file->device->readFile(file, head, want) == (s32)want;
	file->device->closeFile(file);
	return ok && UISaves_FindEntryPrefix(head, want, file->size, entry) != 0u;
}

static void statsFolder(const char gameId[6], uiSavesGameStats_t *stats)
{
	DEVICEHANDLER_INTERFACE *device = devices[DEVICE_CONFIG];
	file_handle folder = {0}, *files = NULL;
	uiSavesRawCard_t *raw = NULL;
	u8 *head;
	unsigned images = 0u;
	int total, i;

	if(device == NULL || statsCardDevice(device) || device->initial == NULL ||
		device->readDir == NULL || device->seekFile == NULL ||
		device->readFile == NULL || device->closeFile == NULL) {
		stats->partial = true;
		return;
	}
	folder.device = device;
	folder.fileType = IS_DIR;
	concat_path(folder.name, device->initial->name, saves_folder());
	total = device->readDir(&folder, &files, -1);
	if(total < 0 || (total > 0 && files == NULL)) {
		stats->partial = true;
		free(files);
		return;
	}
	stats->checkedSources |= 4u;
	if(total > SAVES_STATS_FILES) stats->partial = true;
	head = memalign(32, UI_SAVES_HEAD_SIZE);
	if(head == NULL) {
		stats->partial = true;
		free(files);
		return;
	}
	for(i = 0; i < total && i < SAVES_STATS_FILES; i++) {
		file_handle *file = &files[i];
		const char *leaf = getRelativeName(file->name);
		u8 entry[UI_SAVES_ENTRY_SIZE];

		if(leaf[0] == '.') continue;
		/* Folder browsing can expose more saves than this bounded direct
		 * scan. Mark the summary partial rather than claiming a full total. */
		if(file->fileType == IS_DIR) {
			stats->partial = true;
			continue;
		}
		if(file->fileType != IS_FILE) continue;
		file->device = device;
		if(SavesRaw_IsImageName(leaf)) {
			uiSavesRawStatus_t status;
			unsigned ordinal;

			if(images++ >= SAVES_STATS_IMAGES) {
				stats->partial = true;
				continue;
			}
			if(raw == NULL) raw = calloc(1, sizeof(*raw));
			if(raw == NULL) {
				stats->partial = true;
				continue;
			}
			status = SavesRaw_Load(file, raw);
			if(status == UI_SAVES_RAW_UNFORMATTED) continue;
			if(status != UI_SAVES_RAW_OK) {
				stats->partial = true;
				continue;
			}
			for(ordinal = 0u; ordinal < raw->count; ordinal++) {
				UISaves_StatsAdd(stats, UISavesRaw_Entry(raw, ordinal), gameId, 2u);
			}
		}
		else if(statsSaveName(leaf)) {
			if(statsReadEntry(file, head, entry)) {
				UISaves_StatsAdd(stats, entry, gameId, 2u);
			}
			else stats->partial = true;
		}
	}
	free(head);
	free(raw);
	free(files);
}

void Saves_CollectGameStats(const char gameId[6], uiSavesGameStats_t *stats)
{
	bool mounted;
	unsigned i;

	if(stats == NULL) return;
	memset(stats, 0, sizeof(*stats));
	if(gameId == NULL) return;
	/* Only six-byte disc game IDs can match a save; never a title/path. */
	for(i = 0u; i < 6u; i++) {
		if(!((gameId[i] >= 'A' && gameId[i] <= 'Z') ||
			(gameId[i] >= 'a' && gameId[i] <= 'z') ||
			(gameId[i] >= '0' && gameId[i] <= '9'))) return;
	}
	mounted = config_set_device();
	if(mounted) statsFolder(gameId, stats);
	else stats->partial = true;
	for(i = 0u; i < 2u; i++) statsSlot(gameId, stats, i);
	if(mounted) config_unset_device();
}
