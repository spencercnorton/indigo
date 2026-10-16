#include "ui_saves_raw.h"

#include <string.h>

#define RAW_VALID 0x52415731u
#define RAW_END 0xffffu

static unsigned be16(const uint8_t *bytes)
{
	return ((unsigned)bytes[0] << 8) | bytes[1];
}

static void put16(uint8_t *bytes, unsigned value)
{
	bytes[0] = (uint8_t)(value >> 8);
	bytes[1] = (uint8_t)value;
}

/* The GameCube's two sums over a system block's 16-bit words. */
static void sums(const uint8_t *bytes, size_t length, unsigned *sum,
	unsigned *inverse)
{
	size_t at;

	*sum = 0u;
	*inverse = 0u;
	for(at = 0u; at < length; at += 2u) {
		unsigned value = be16(bytes + at);
		*sum = (*sum + value) & 0xffffu;
		*inverse = (*inverse + (value ^ 0xffffu)) & 0xffffu;
	}
	if(*sum == 0xffffu) *sum = 0u;
	if(*inverse == 0xffffu) *inverse = 0u;
}

static bool checksum(const uint8_t *bytes, size_t length,
	const uint8_t *stored)
{
	unsigned sum;
	unsigned inverse;

	sums(bytes, length, &sum, &inverse);
	return sum == be16(stored) && inverse == be16(stored + 2u);
}

static int signedCounter(const uint8_t *bytes)
{
	unsigned value = be16(bytes);
	return value < 0x8000u ? (int)value : (int)value - 0x10000;
}

static unsigned selectedCopy(const uint8_t *first, const uint8_t *second,
	bool firstValid, bool secondValid, size_t counterAt)
{
	if(!firstValid) return 1u;
	if(!secondValid) return 0u;
	return signedCounter(first + counterAt) >= signedCounter(second + counterAt) ?
		0u : 1u;
}

static bool validMap(const uint8_t *map, unsigned blocks)
{
	unsigned freeBlocks = 0u;
	unsigned block;

	if(!checksum(map + 4u, UI_SAVES_BLOCK_SIZE - 4u, map)) return false;
	for(block = UI_SAVES_RAW_SYSTEM_BLOCKS; block < blocks; ++block) {
		unsigned next = be16(map + block * 2u);
		if(next == 0u) ++freeBlocks;
		else if(next != RAW_END &&
			(next < UI_SAVES_RAW_SYSTEM_BLOCKS || next >= blocks)) return false;
	}
	return freeBlocks == be16(map + 6u);
}

static uiSavesRawStatus_t failed(uiSavesRawCard_t *card, uiSavesRawStatus_t status)
{
	memset(card, 0, sizeof(*card));
	return status;
}

uiSavesRawStatus_t UISavesRaw_Parse(const uint8_t *metadata,
	size_t metadataLength, uint32_t imageSize, uiSavesRawCard_t *card)
{
	const uint8_t *directories[2];
	const uint8_t *maps[2];
	const uint8_t *directory;
	const uint8_t *map;
	bool dirValid[2];
	bool mapValid[2];
	uint8_t claimed[UI_SAVES_RAW_MAX_BLOCKS / 8u] = {0};
	unsigned blocks;
	unsigned megabits;
	unsigned invalidCopies = 0u;
	unsigned i;
	bool allZero = true;
	bool allErased = true;

	if(card == NULL) return UI_SAVES_RAW_INVALID_ARGUMENT;
	memset(card, 0, sizeof(*card));
	if(metadata == NULL || metadataLength < UI_SAVES_RAW_METADATA_SIZE) {
		return UI_SAVES_RAW_INVALID_ARGUMENT;
	}
	megabits = imageSize / 131072u;
	if(imageSize % 131072u != 0u ||
		(megabits != 4u && megabits != 8u && megabits != 16u &&
		megabits != 32u && megabits != 64u && megabits != 128u)) {
		return UI_SAVES_RAW_INVALID_SIZE;
	}
	blocks = imageSize / UI_SAVES_BLOCK_SIZE;
	for(i = 0u; i < UI_SAVES_RAW_METADATA_SIZE; ++i) {
		allZero = allZero && metadata[i] == 0u;
		allErased = allErased && metadata[i] == 0xffu;
	}
	if(allZero || allErased) return UI_SAVES_RAW_UNFORMATTED;
	if(be16(metadata + 0x22u) != megabits || be16(metadata + 0x24u) > 1u ||
		!checksum(metadata, 0x1fcu, metadata + 0x1fcu)) {
		return UI_SAVES_RAW_INVALID_HEADER;
	}
	for(i = 0u; i < 2u; ++i) {
		directories[i] = metadata + (1u + i) * UI_SAVES_BLOCK_SIZE;
		maps[i] = metadata + (3u + i) * UI_SAVES_BLOCK_SIZE;
		dirValid[i] = checksum(directories[i], UI_SAVES_BLOCK_SIZE - 4u,
			directories[i] + UI_SAVES_BLOCK_SIZE - 4u);
		mapValid[i] = validMap(maps[i], blocks);
		invalidCopies += !dirValid[i];
		invalidCopies += !mapValid[i];
	}
	if(invalidCopies > 1u) return UI_SAVES_RAW_INVALID_METADATA;
	card->directoryCopy = (uint8_t)selectedCopy(directories[0], directories[1],
		dirValid[0], dirValid[1], 0x1ffau);
	card->mapCopy = (uint8_t)selectedCopy(maps[0], maps[1], mapValid[0],
		mapValid[1], 4u);
	directory = directories[card->directoryCopy];
	map = maps[card->mapCopy];
	card->totalBlocks = (uint16_t)blocks;
	card->freeBlocks = (uint16_t)be16(map + 6u);
	for(i = UI_SAVES_RAW_SYSTEM_BLOCKS; i < blocks; ++i) {
		card->next[i] = (uint16_t)be16(map + i * 2u);
	}
	for(i = 0u; i < UI_SAVES_CARD_FILES; ++i) {
		const uint8_t *entry = directory + i * UI_SAVES_ENTRY_SIZE;
		unsigned count;
		unsigned block;
		unsigned step;
		unsigned previous;

		if(entry[0] == 0xffu && entry[1] == 0xffu &&
			entry[2] == 0xffu && entry[3] == 0xffu) continue;
		count = UISaves_Blocks(entry);
		block = be16(entry + 0x36u);
		if(entry[0] == 0xffu || entry[4] == 0xffu || count == 0u ||
			count > blocks - UI_SAVES_RAW_SYSTEM_BLOCKS) {
			return failed(card, UI_SAVES_RAW_INVALID_CHAIN);
		}
		for(previous = 0u; previous < card->count; ++previous) {
			const uint8_t *other = card->entry[previous];
			if(!memcmp(entry, other, 6u) &&
				!memcmp(entry + 8u, other + 8u, UI_SAVES_NAME_LENGTH)) {
				return failed(card, UI_SAVES_RAW_INVALID_CHAIN);
			}
		}
		for(step = 0u; step < count; ++step) {
			unsigned next;
			uint8_t mask;

			if(block < UI_SAVES_RAW_SYSTEM_BLOCKS || block >= blocks) {
				return failed(card, UI_SAVES_RAW_INVALID_CHAIN);
			}
			mask = (uint8_t)(1u << (block & 7u));
			if((claimed[block / 8u] & mask) != 0u) {
				return failed(card, UI_SAVES_RAW_INVALID_CHAIN);
			}
			claimed[block / 8u] |= mask;
			next = card->next[block];
			if(step + 1u == count && next != RAW_END) {
				return failed(card, UI_SAVES_RAW_INVALID_CHAIN);
			}
			block = next;
		}
		memcpy(card->entry[card->count], entry, UI_SAVES_ENTRY_SIZE);
		card->directoryIndex[card->count] = (uint8_t)i;
		++card->count;
	}
	card->valid = RAW_VALID;
	return UI_SAVES_RAW_OK;
}

const uint8_t *UISavesRaw_Entry(const uiSavesRawCard_t *card, unsigned ordinal)
{
	return card != NULL && card->valid == RAW_VALID &&
		card->count <= UI_SAVES_CARD_FILES && ordinal < card->count ?
		card->entry[ordinal] : NULL;
}

uint32_t UISavesRaw_GciSize(const uiSavesRawCard_t *card, unsigned ordinal)
{
	const uint8_t *entry = UISavesRaw_Entry(card, ordinal);
	unsigned blocks = entry != NULL ? UISaves_Blocks(entry) : 0u;

	return blocks > 0u && blocks <= UI_SAVES_RAW_MAX_BLOCKS - UI_SAVES_RAW_SYSTEM_BLOCKS ?
		UI_SAVES_ENTRY_SIZE + blocks * UI_SAVES_BLOCK_SIZE : 0u;
}

bool UISavesRaw_ReadGci(const uiSavesRawCard_t *card, unsigned ordinal,
	uint32_t offset, void *destination, uint32_t length,
	uiSavesRawReadAt_t readAt, void *opaque)
{
	const uint8_t *entry = UISavesRaw_Entry(card, ordinal);
	uint32_t size = UISavesRaw_GciSize(card, ordinal);
	uint8_t *out = destination;
	unsigned block;
	unsigned skip;
	unsigned step;

	if(entry == NULL || size == 0u || offset > size || length > size - offset ||
		(length != 0u && destination == NULL) || readAt == NULL ||
		card->totalBlocks > UI_SAVES_RAW_MAX_BLOCKS ||
		card->totalBlocks <= UI_SAVES_RAW_SYSTEM_BLOCKS) return false;
	if(length == 0u) return true;
	if(offset < UI_SAVES_ENTRY_SIZE) {
		uint32_t take = UI_SAVES_ENTRY_SIZE - offset;
		if(take > length) take = length;
		memcpy(out, entry + offset, take);
		out += take;
		offset += take;
		length -= take;
	}
	if(length == 0u) return true;
	offset -= UI_SAVES_ENTRY_SIZE;
	skip = offset / UI_SAVES_BLOCK_SIZE;
	block = be16(entry + 0x36u);
	for(step = 0u; step < skip; ++step) {
		if(block < UI_SAVES_RAW_SYSTEM_BLOCKS || block >= card->totalBlocks) return false;
		block = card->next[block];
	}
	while(length != 0u) {
		uint32_t within = offset % UI_SAVES_BLOCK_SIZE;
		uint32_t take = UI_SAVES_BLOCK_SIZE - within;
		if(block < UI_SAVES_RAW_SYSTEM_BLOCKS || block >= card->totalBlocks) return false;
		if(take > length) take = length;
		if(!readAt(opaque, block * UI_SAVES_BLOCK_SIZE + within, out, take)) return false;
		out += take;
		offset += take;
		length -= take;
		block = card->next[block];
	}
	return true;
}

unsigned UISavesRaw_Find(const uiSavesRawCard_t *card,
	const uint8_t entry[UI_SAVES_ENTRY_SIZE])
{
	unsigned ordinal;

	if(card == NULL || card->valid != RAW_VALID || entry == NULL) {
		return card != NULL ? card->count : 0u;
	}
	for(ordinal = 0u; ordinal < card->count; ++ordinal) {
		if(!memcmp(card->entry[ordinal], entry, 6u) &&
			!memcmp(card->entry[ordinal] + 8u, entry + 8u, UI_SAVES_NAME_LENGTH)) {
			break;
		}
	}
	return ordinal;
}

/* The new directory (first 1) or block map (first 3) is in the copy the
 * parse didn't choose: its update count goes one past the chosen copy's and
 * its sums are made. When that count doesn't compare higher, the chosen copy
 * becomes the same, written second. */
static void seal(uint8_t *metadata, unsigned first, unsigned chosen,
	uint8_t written[2])
{
	bool map = first == 3u;
	size_t counterAt = map ? 4u : 0x1ffau;
	uint8_t *older = metadata + (first + (chosen ^ 1u)) * UI_SAVES_BLOCK_SIZE;
	const uint8_t *newer = metadata + (first + chosen) * UI_SAVES_BLOCK_SIZE;
	int count = signedCounter(newer + counterAt);
	unsigned sum;
	unsigned inverse;

	put16(older + counterAt, (unsigned)(count + 1) & 0xffffu);
	if(map) {
		sums(older + 4u, UI_SAVES_BLOCK_SIZE - 4u, &sum, &inverse);
		put16(older, sum);
		put16(older + 2u, inverse);
	}
	else {
		sums(older, UI_SAVES_BLOCK_SIZE - 4u, &sum, &inverse);
		put16(older + UI_SAVES_BLOCK_SIZE - 4u, sum);
		put16(older + UI_SAVES_BLOCK_SIZE - 2u, inverse);
	}
	written[0] = (uint8_t)(first + (chosen ^ 1u));
	written[1] = 0u;
	if(signedCounter(older + counterAt) <= count) {
		memcpy(metadata + (first + chosen) * UI_SAVES_BLOCK_SIZE, older,
			UI_SAVES_BLOCK_SIZE);
		written[1] = (uint8_t)(first + chosen);
	}
}

/* Starts a change once nothing refuses it: the chosen directory and block
 * map copied over the older ones, which the plan then edits. */
static void planStart(uint8_t *metadata, const uiSavesRawCard_t *card,
	uint8_t **directory, uint8_t **map)
{
	*directory = metadata + (2u - card->directoryCopy) * UI_SAVES_BLOCK_SIZE;
	*map = metadata + (4u - card->mapCopy) * UI_SAVES_BLOCK_SIZE;
	memcpy(*directory, metadata + (1u + card->directoryCopy) * UI_SAVES_BLOCK_SIZE,
		UI_SAVES_BLOCK_SIZE);
	memcpy(*map, metadata + (3u + card->mapCopy) * UI_SAVES_BLOCK_SIZE,
		UI_SAVES_BLOCK_SIZE);
}

/* Seals the planned copies and reads them back as the console would: they
 * must parse, with count saves and freeBlocks free. Anything else, and the
 * change is refused with nothing to write. */
static uiSavesRawStatus_t planFinish(uint8_t *metadata, uint32_t imageSize,
	uiSavesRawCard_t *card, uiSavesRawChange_t *change, unsigned count,
	unsigned freeBlocks)
{
	unsigned directoryCopy = card->directoryCopy;
	unsigned mapCopy = card->mapCopy;

	seal(metadata, 1u, directoryCopy, change->directory);
	seal(metadata, 3u, mapCopy, change->map);
	if(UISavesRaw_Parse(metadata, UI_SAVES_RAW_METADATA_SIZE, imageSize,
		card) != UI_SAVES_RAW_OK || card->count != count ||
		card->freeBlocks != freeBlocks) {
		memset(change, 0, sizeof(*change));
		memset(card, 0, sizeof(*card));
		return UI_SAVES_RAW_INVALID_METADATA;
	}
	return UI_SAVES_RAW_OK;
}

uiSavesRawStatus_t UISavesRaw_PlanAdd(uint8_t *metadata, uint32_t imageSize,
	const uint8_t entry[UI_SAVES_ENTRY_SIZE], uiSavesRawCard_t *card,
	uiSavesRawChange_t *change)
{
	uint8_t own[UI_SAVES_ENTRY_SIZE];
	uint8_t *directory;
	uint8_t *map;
	unsigned need = entry != NULL ? UISaves_Blocks(entry) : 0u;
	unsigned found = 0u;
	unsigned block;
	unsigned previous = 0u;
	unsigned first = 0u;
	unsigned place;
	unsigned step;
	uiSavesRawStatus_t status;

	if(change == NULL || card == NULL) return UI_SAVES_RAW_INVALID_ARGUMENT;
	memset(change, 0, sizeof(*change));
	/* A first byte of 0xff marks a free directory place. */
	if(entry == NULL || entry[0] == 0xffu || entry[4] == 0xffu || need == 0u) {
		return UI_SAVES_RAW_INVALID_ARGUMENT;
	}
	/* entry may be card's own: parsing rewrites card. */
	memcpy(own, entry, sizeof(own));
	entry = own;
	status = UISavesRaw_Parse(metadata, UI_SAVES_RAW_METADATA_SIZE, imageSize, card);
	if(status != UI_SAVES_RAW_OK) return status;
	if(UISavesRaw_Find(card, entry) != card->count) return UI_SAVES_RAW_EXISTS;
	if(card->count >= UI_SAVES_CARD_FILES) return UI_SAVES_RAW_FULL;
	if(need > card->freeBlocks) return UI_SAVES_RAW_NO_ROOM;
	planStart(metadata, card, &directory, &map);
	for(place = 0u; place < UI_SAVES_CARD_FILES; ++place) {
		const uint8_t *unused = directory + place * UI_SAVES_ENTRY_SIZE;
		if(unused[0] == 0xffu && unused[1] == 0xffu && unused[2] == 0xffu &&
			unused[3] == 0xffu) break;
	}
	/* From after the last block allocated, round to the first data block,
	 * as the GameCube's card library looks. */
	block = be16(map + 8u);
	for(step = 0u; found < need && step < card->totalBlocks; ++step) {
		if(++block < UI_SAVES_RAW_SYSTEM_BLOCKS || block >= card->totalBlocks) {
			block = UI_SAVES_RAW_SYSTEM_BLOCKS;
		}
		if(be16(map + block * 2u) != 0u) continue;
		if(found++ == 0u) first = block;
		else put16(map + previous * 2u, block);
		put16(map + block * 2u, RAW_END);
		previous = block;
	}
	if(place == UI_SAVES_CARD_FILES || found != need) {
		return UI_SAVES_RAW_INVALID_METADATA;
	}
	put16(map + 6u, card->freeBlocks - need);
	put16(map + 8u, previous);
	memcpy(directory + place * UI_SAVES_ENTRY_SIZE, entry, UI_SAVES_ENTRY_SIZE);
	put16(directory + place * UI_SAVES_ENTRY_SIZE + 0x36u, first);
	change->first = (uint16_t)first;
	status = planFinish(metadata, imageSize, card, change, card->count + 1u,
		card->freeBlocks - need);
	if(status == UI_SAVES_RAW_OK) {
		/* The save as its .gci has it, but for where it now starts. */
		const uint8_t *added = UISavesRaw_Entry(card, UISavesRaw_Find(card, entry));
		if(added == NULL || be16(added + 0x36u) != first ||
			memcmp(added, entry, 0x36u) || memcmp(added + 0x38u, entry + 0x38u,
			UI_SAVES_ENTRY_SIZE - 0x38u)) {
			memset(change, 0, sizeof(*change));
			return UI_SAVES_RAW_INVALID_METADATA;
		}
	}
	return status;
}

uiSavesRawStatus_t UISavesRaw_PlanErase(uint8_t *metadata, uint32_t imageSize,
	const uint8_t entry[UI_SAVES_ENTRY_SIZE], uiSavesRawCard_t *card,
	uiSavesRawChange_t *change)
{
	uint8_t own[UI_SAVES_ENTRY_SIZE];
	uint8_t *directory;
	uint8_t *map;
	unsigned ordinal;
	unsigned block;
	unsigned need;
	unsigned step;
	uiSavesRawStatus_t status;

	if(change == NULL || card == NULL) return UI_SAVES_RAW_INVALID_ARGUMENT;
	memset(change, 0, sizeof(*change));
	if(entry == NULL) return UI_SAVES_RAW_INVALID_ARGUMENT;
	memcpy(own, entry, sizeof(own));
	entry = own;
	status = UISavesRaw_Parse(metadata, UI_SAVES_RAW_METADATA_SIZE, imageSize, card);
	if(status != UI_SAVES_RAW_OK) return status;
	ordinal = UISavesRaw_Find(card, entry);
	if(ordinal == card->count || memcmp(card->entry[ordinal], entry,
		UI_SAVES_ENTRY_SIZE)) return UI_SAVES_RAW_NOT_FOUND;
	planStart(metadata, card, &directory, &map);
	memset(directory + card->directoryIndex[ordinal] * UI_SAVES_ENTRY_SIZE, 0xff,
		UI_SAVES_ENTRY_SIZE);
	/* The parse walked this chain: need blocks, ending at RAW_END. */
	need = UISaves_Blocks(entry);
	block = be16(entry + 0x36u);
	for(step = 0u; step < need; ++step) {
		unsigned next = card->next[block];
		put16(map + block * 2u, 0u);
		block = next;
	}
	put16(map + 6u, card->freeBlocks + need);
	status = planFinish(metadata, imageSize, card, change, card->count - 1u,
		card->freeBlocks + need);
	if(status == UI_SAVES_RAW_OK && UISavesRaw_Find(card, entry) != card->count) {
		memset(change, 0, sizeof(*change));
		return UI_SAVES_RAW_INVALID_METADATA;
	}
	return status;
}

const char *UISavesRaw_StatusText(uiSavesRawStatus_t status)
{
	switch(status) {
		case UI_SAVES_RAW_OK: return "Card image ready";
		case UI_SAVES_RAW_UNFORMATTED: return "This card image has not been formatted";
		case UI_SAVES_RAW_INVALID_SIZE: return "This is not a supported card image";
		case UI_SAVES_RAW_INVALID_HEADER: return "This card image's header is invalid";
		case UI_SAVES_RAW_INVALID_METADATA: return "This card image's directory or block map is invalid";
		case UI_SAVES_RAW_INVALID_CHAIN: return "This card image's saves do not match its block map";
		case UI_SAVES_RAW_READ_ERROR: return "This card image could not be read";
		case UI_SAVES_RAW_EXISTS: return "The card image already has this save";
		case UI_SAVES_RAW_FULL: return "The card image has 127 saves";
		case UI_SAVES_RAW_NO_ROOM: return "The card image has too few free blocks";
		case UI_SAVES_RAW_NOT_FOUND: return "The save is no longer on the card image";
		case UI_SAVES_RAW_READ_ONLY: return "Card images on this device can't be changed";
		case UI_SAVES_RAW_WRITE_ERROR: return "The card image couldn't be written";
		case UI_SAVES_RAW_MISMATCH: return "The card image didn't read back the same";
		default: return "This card image could not be opened";
	}
}
