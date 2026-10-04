#include "ui_saves_raw.h"

#include <string.h>

#define RAW_VALID 0x52415731u
#define RAW_END 0xffffu

static unsigned be16(const uint8_t *bytes)
{
	return ((unsigned)bytes[0] << 8) | bytes[1];
}

static bool checksum(const uint8_t *bytes, size_t length,
	const uint8_t *stored)
{
	unsigned sum = 0u;
	unsigned inverse = 0u;
	size_t at;

	for(at = 0u; at < length; at += 2u) {
		unsigned value = be16(bytes + at);
		sum = (sum + value) & 0xffffu;
		inverse = (inverse + (value ^ 0xffffu)) & 0xffffu;
	}
	if(sum == 0xffffu) sum = 0u;
	if(inverse == 0xffffu) inverse = 0u;
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
	unsigned used = 0u;
	unsigned allocated = 0u;
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
	directory = directories[selectedCopy(directories[0], directories[1],
		dirValid[0], dirValid[1], 0x1ffau)];
	map = maps[selectedCopy(maps[0], maps[1], mapValid[0], mapValid[1], 4u)];
	card->totalBlocks = (uint16_t)blocks;
	card->freeBlocks = (uint16_t)be16(map + 6u);
	for(i = UI_SAVES_RAW_SYSTEM_BLOCKS; i < blocks; ++i) {
		card->next[i] = (uint16_t)be16(map + i * 2u);
		allocated += card->next[i] != 0u;
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
			++used;
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
	if(used != allocated) return failed(card, UI_SAVES_RAW_INVALID_CHAIN);
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
		default: return "This card image could not be opened";
	}
}
