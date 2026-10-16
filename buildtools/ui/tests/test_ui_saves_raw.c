#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ui_saves_raw.h"

#define IMAGE_SIZE (512u * 1024u)
static uint8_t metadata[UI_SAVES_RAW_METADATA_SIZE];
static uiSavesRawCard_t card;

static void put16(uint8_t *at, unsigned value)
{
	at[0] = (uint8_t)(value >> 8);
	at[1] = (uint8_t)value;
}

static void sum(uint8_t *bytes, size_t length, uint8_t *stored)
{
	unsigned value = 0u;
	unsigned inverse = 0u;
	for(size_t i = 0u; i < length; i += 2u) {
		unsigned word = ((unsigned)bytes[i] << 8) | bytes[i + 1u];
		value = (value + word) & 65535u;
		inverse = (inverse + (word ^ 65535u)) & 65535u;
	}
	put16(stored, value == 65535u ? 0u : value);
	put16(stored + 2u, inverse == 65535u ? 0u : inverse);
}

static void fix(void)
{
	sum(metadata, 0x1fcu, metadata + 0x1fcu);
	for(unsigned i = 1u; i <= 2u; ++i) {
		uint8_t *directory = metadata + i * UI_SAVES_BLOCK_SIZE;
		sum(directory, UI_SAVES_BLOCK_SIZE - 4u,
			directory + UI_SAVES_BLOCK_SIZE - 4u);
	}
	for(unsigned i = 3u; i <= 4u; ++i) {
		uint8_t *map = metadata + i * UI_SAVES_BLOCK_SIZE;
		sum(map + 4u, UI_SAVES_BLOCK_SIZE - 4u, map);
	}
}

static void fixture(void)
{
	memset(metadata, 0xff, sizeof(metadata));
	put16(metadata + 0x22u, 4u);
	put16(metadata + 0x24u, 0u);
	for(unsigned copy = 0u; copy < 2u; ++copy) {
		uint8_t *directory = metadata + (1u + copy) * UI_SAVES_BLOCK_SIZE;
		uint8_t *map = metadata + (3u + copy) * UI_SAVES_BLOCK_SIZE;
		memset(directory, 0, 2u * UI_SAVES_ENTRY_SIZE);
		memcpy(directory, "DEMO01", 6u);
		memcpy(directory + 8u, "Demo Save", 10u);
		put16(directory + 0x36u, 5u);
		put16(directory + 0x38u, 2u);
		memcpy(directory + UI_SAVES_ENTRY_SIZE, "TEST01", 6u);
		memcpy(directory + UI_SAVES_ENTRY_SIZE + 8u, "Other Save", 11u);
		put16(directory + UI_SAVES_ENTRY_SIZE + 0x36u, 6u);
		put16(directory + UI_SAVES_ENTRY_SIZE + 0x38u, 1u);
		put16(directory + 0x1ffau, copy);
		memset(map, 0, UI_SAVES_BLOCK_SIZE);
		put16(map + 4u, copy);
		put16(map + 6u, 56u);
		put16(map + 8u, 9u);
		put16(map + 5u * 2u, 9u);
		put16(map + 6u * 2u, 65535u);
		put16(map + 9u * 2u, 65535u);
	}
	fix();
}

static void expect(uiSavesRawStatus_t status)
{
	assert(UISavesRaw_Parse(metadata, sizeof(metadata), IMAGE_SIZE, &card) == status);
	if(status != UI_SAVES_RAW_OK) {
		assert(card.valid == 0u && card.count == 0u);
		assert(UISavesRaw_Entry(&card, 0u) == NULL);
		assert(UISavesRaw_GciSize(&card, 0u) == 0u);
	}
}

static unsigned reads;
static bool failRead;
static bool readAt(void *opaque, uint32_t offset, void *destination, uint32_t length)
{
	uint8_t *out = destination;
	(void)opaque;
	assert(offset >= UI_SAVES_RAW_METADATA_SIZE);
	assert(offset <= IMAGE_SIZE && length <= IMAGE_SIZE - offset);
	assert(length <= UI_SAVES_BLOCK_SIZE);
	++reads;
	if(failRead) return false;
	for(uint32_t i = 0u; i < length; ++i) {
		out[i] = (uint8_t)(((offset + i) / UI_SAVES_BLOCK_SIZE) ^
			((offset + i) % UI_SAVES_BLOCK_SIZE));
	}
	return true;
}

static void testReads(void)
{
	uint8_t output[2u * UI_SAVES_BLOCK_SIZE + UI_SAVES_ENTRY_SIZE];
	uint32_t size;

	fixture();
	expect(UI_SAVES_RAW_OK);
	assert(card.count == 2u && card.totalBlocks == 64u && card.freeBlocks == 56u);
	assert(sizeof(card) < 16u * 1024u);
	size = UISavesRaw_GciSize(&card, 0u);
	assert(size == sizeof(output));
	reads = 0u;
	failRead = false;
	assert(UISavesRaw_ReadGci(&card, 0u, 0u, output, size, readAt, NULL));
	assert(reads == 2u);
	assert(!memcmp(output, UISavesRaw_Entry(&card, 0u), UI_SAVES_ENTRY_SIZE));
	for(unsigned i = 0u; i < 2u * UI_SAVES_BLOCK_SIZE; ++i) {
		unsigned block = i < UI_SAVES_BLOCK_SIZE ? 5u : 9u;
		assert(output[UI_SAVES_ENTRY_SIZE + i] ==
			(uint8_t)(block ^ (i % UI_SAVES_BLOCK_SIZE)));
	}
	assert(UISavesRaw_ReadGci(&card, 0u, UI_SAVES_ENTRY_SIZE + 8190u,
		output, 6u, readAt, NULL));
	assert(output[0] == (uint8_t)(5u ^ 8190u));
	assert(output[2] == 9u && output[5] == (uint8_t)(9u ^ 3u));
	reads = 0u;
	assert(UISavesRaw_ReadGci(&card, 0u, 4u, output, 8u, readAt, NULL));
	assert(reads == 0u);
	assert(!UISavesRaw_ReadGci(&card, 0u, size, output, 1u, readAt, NULL));
	assert(!UISavesRaw_ReadGci(&card, 0u, UINT32_MAX, output, 2u, readAt, NULL));
	assert(!UISavesRaw_ReadGci(&card, 2u, 0u, output, 1u, readAt, NULL));
	assert(!UISavesRaw_ReadGci(&card, 0u, 0u, NULL, 1u, readAt, NULL));
	assert(reads == 0u);
	assert(UISavesRaw_ReadGci(&card, 0u, size, NULL, 0u, readAt, NULL));
	failRead = true;
	assert(!UISavesRaw_ReadGci(&card, 0u, UI_SAVES_ENTRY_SIZE,
		output, 1u, readAt, NULL));
	assert(reads == 1u);
	failRead = false;
}

static void testMetadata(void)
{
	fixture(); metadata[0] ^= 1u; expect(UI_SAVES_RAW_INVALID_HEADER);
	fixture(); metadata[UI_SAVES_BLOCK_SIZE] ^= 1u; expect(UI_SAVES_RAW_OK);
	metadata[2u * UI_SAVES_BLOCK_SIZE] ^= 1u; expect(UI_SAVES_RAW_INVALID_METADATA);
	fixture(); metadata[3u * UI_SAVES_BLOCK_SIZE] ^= 1u; expect(UI_SAVES_RAW_OK);
	metadata[4u * UI_SAVES_BLOCK_SIZE] ^= 1u; expect(UI_SAVES_RAW_INVALID_METADATA);
	fixture(); metadata[UI_SAVES_BLOCK_SIZE] ^= 1u;
	metadata[3u * UI_SAVES_BLOCK_SIZE] ^= 1u; expect(UI_SAVES_RAW_INVALID_METADATA);
	fixture();
	memcpy(metadata + UI_SAVES_BLOCK_SIZE + 8u, "FirstSave", 10u);
	put16(metadata + UI_SAVES_BLOCK_SIZE + 0x1ffau, 32767u);
	put16(metadata + 2u * UI_SAVES_BLOCK_SIZE + 0x1ffau, 32768u);
	fix(); expect(UI_SAVES_RAW_OK);
	assert(!memcmp(UISavesRaw_Entry(&card, 0u) + 8u, "FirstSave", 10u));
	put16(metadata + 2u * UI_SAVES_BLOCK_SIZE + 0x1ffau, 32767u);
	fix(); expect(UI_SAVES_RAW_OK);
	assert(!memcmp(UISavesRaw_Entry(&card, 0u) + 8u, "FirstSave", 10u));
	fixture();
	for(unsigned copy = 1u; copy <= 2u; ++copy) {
		put16(metadata + copy * UI_SAVES_BLOCK_SIZE + 0x38u, 3u);
	}
	fix(); expect(UI_SAVES_RAW_INVALID_CHAIN);
	fixture();
	for(unsigned copy = 1u; copy <= 2u; ++copy) {
		put16(metadata + copy * UI_SAVES_BLOCK_SIZE + UI_SAVES_ENTRY_SIZE + 0x36u, 5u);
	}
	fix(); expect(UI_SAVES_RAW_INVALID_CHAIN);
	fixture();
	for(unsigned copy = 3u; copy <= 4u; ++copy) {
		put16(metadata + copy * UI_SAVES_BLOCK_SIZE + 9u * 2u, 5u);
	}
	fix(); expect(UI_SAVES_RAW_INVALID_CHAIN);
	/* A block the map holds that no save reaches: what an interrupted write
	 * leaves, as on the console. It stays allocated, and the card reads. */
	fixture();
	for(unsigned copy = 3u; copy <= 4u; ++copy) {
		put16(metadata + copy * UI_SAVES_BLOCK_SIZE + 7u * 2u, 65535u);
		put16(metadata + copy * UI_SAVES_BLOCK_SIZE + 6u, 55u);
	}
	fix(); expect(UI_SAVES_RAW_OK);
	assert(card.count == 2u && card.freeBlocks == 55u && card.next[7] == 65535u);
	fixture();
	for(unsigned copy = 3u; copy <= 4u; ++copy) {
		put16(metadata + copy * UI_SAVES_BLOCK_SIZE + 5u * 2u, 64u);
	}
	fix(); expect(UI_SAVES_RAW_INVALID_METADATA);
	fixture();
	assert(UISavesRaw_Parse(metadata, sizeof(metadata) - 1u, IMAGE_SIZE, &card) ==
		UI_SAVES_RAW_INVALID_ARGUMENT);
	assert(UISavesRaw_Parse(metadata, sizeof(metadata), IMAGE_SIZE + 1u, &card) ==
		UI_SAVES_RAW_INVALID_SIZE);
	assert(UISavesRaw_Parse(NULL, 0u, IMAGE_SIZE, &card) == UI_SAVES_RAW_INVALID_ARGUMENT);
	assert(UISavesRaw_Parse(metadata, sizeof(metadata), IMAGE_SIZE, NULL) ==
		UI_SAVES_RAW_INVALID_ARGUMENT);
	memset(metadata, 0, sizeof(metadata)); expect(UI_SAVES_RAW_UNFORMATTED);
	memset(metadata, 0xff, sizeof(metadata)); expect(UI_SAVES_RAW_UNFORMATTED);
}

static void testGeometry(void)
{
	for(unsigned megabits = 4u; megabits <= 128u; megabits *= 2u) {
		unsigned blocks = megabits * 16u;
		fixture();
		put16(metadata + 0x22u, megabits);
		for(unsigned copy = 3u; copy <= 4u; ++copy) {
			put16(metadata + copy * UI_SAVES_BLOCK_SIZE + 6u, blocks - 8u);
		}
		fix();
		assert(UISavesRaw_Parse(metadata, sizeof(metadata), megabits * 131072u,
			&card) == UI_SAVES_RAW_OK);
		assert(card.totalBlocks == blocks && card.count == 2u);
	}
	/* A formatted empty card is valid and differs from an unformatted image. */
	fixture();
	for(unsigned copy = 1u; copy <= 2u; ++copy) {
		uint8_t *directory = metadata + copy * UI_SAVES_BLOCK_SIZE;
		memset(directory, 0xff, UI_SAVES_CARD_FILES * UI_SAVES_ENTRY_SIZE);
	}
	for(unsigned copy = 3u; copy <= 4u; ++copy) {
		uint8_t *map = metadata + copy * UI_SAVES_BLOCK_SIZE;
		memset(map + 10u, 0, UI_SAVES_BLOCK_SIZE - 10u);
		put16(map + 6u, 59u);
	}
	fix(); expect(UI_SAVES_RAW_OK);
	assert(card.count == 0u && card.freeBlocks == 59u);
}

/* ------------------------------------------------------------------------
 * Writing: changes planned in the system blocks, checked here by their own
 * sums and by reading them as the parser does.
 * --------------------------------------------------------------------- */
#define BLOCK UI_SAVES_BLOCK_SIZE
static uint8_t before[UI_SAVES_RAW_METADATA_SIZE];
static uiSavesRawChange_t change;

static unsigned get16(const uint8_t *at)
{
	return ((unsigned)at[0] << 8) | at[1];
}

/* A .gci's directory entry: a game, a name and a size; its first block is
 * left for the image to set. */
static void gciEntry(uint8_t entry[UI_SAVES_ENTRY_SIZE], const char *id,
	const char *name, unsigned blocks)
{
	memset(entry, 0, UI_SAVES_ENTRY_SIZE);
	memcpy(entry, id, 6u);
	entry[6] = 0xffu;
	memcpy(entry + 8u, name, strlen(name));
	put16(entry + 0x2cu, 0x40u);
	entry[0x34] = 0x04u;
	put16(entry + 0x36u, 0x1234u);
	put16(entry + 0x38u, blocks);
	put16(entry + 0x3au, 0xffffu);
}

/* A system block whose sums the console would accept, by this file's own
 * arithmetic. */
static bool sealed(unsigned block)
{
	uint8_t copy[BLOCK];

	memcpy(copy, metadata + block * BLOCK, BLOCK);
	if(block >= 3u) sum(copy + 4u, BLOCK - 4u, copy);
	else sum(copy, BLOCK - 4u, copy + BLOCK - 4u);
	return !memcmp(copy, metadata + block * BLOCK, BLOCK);
}

static bool unchanged(void)
{
	return !memcmp(metadata, before, sizeof(metadata));
}

/* A 16 Mbit card holding count one-block saves at blocks 5 onwards. */
static void manySaves(unsigned count)
{
	memset(metadata, 0xff, sizeof(metadata));
	put16(metadata + 0x22u, 16u);
	put16(metadata + 0x24u, 0u);
	for(unsigned copy = 0u; copy < 2u; ++copy) {
		uint8_t *directory = metadata + (1u + copy) * BLOCK;
		uint8_t *map = metadata + (3u + copy) * BLOCK;
		memset(map, 0, BLOCK);
		for(unsigned i = 0u; i < count; ++i) {
			uint8_t *entry = directory + i * UI_SAVES_ENTRY_SIZE;
			char name[16];
			snprintf(name, sizeof(name), "Save %u", i);
			gciEntry(entry, "MANY01", name, 1u);
			put16(entry + 0x36u, 5u + i);
			put16(map + (5u + i) * 2u, 65535u);
		}
		put16(directory + 0x1ffau, copy);
		put16(map + 4u, copy);
		put16(map + 6u, 251u - count);
		put16(map + 8u, 4u + count);
	}
	fix();
}

static void testAdd(void)
{
	uint8_t entry[UI_SAVES_ENTRY_SIZE];
	unsigned at;

	fixture();
	memcpy(before, metadata, sizeof(metadata));
	gciEntry(entry, "NEWS01", "New Save", 3u);
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_OK);
	/* The fixture's newer copies are the second ones: the new versions go
	 * into the first, one count past, and nothing else changes. */
	assert(change.directory[0] == 1u && change.directory[1] == 0u);
	assert(change.map[0] == 3u && change.map[1] == 0u);
	assert(!memcmp(metadata, before, BLOCK));
	assert(!memcmp(metadata + 2u * BLOCK, before + 2u * BLOCK, BLOCK));
	assert(!memcmp(metadata + 4u * BLOCK, before + 4u * BLOCK, BLOCK));
	assert(sealed(1u) && sealed(3u));
	assert(get16(metadata + BLOCK + 0x1ffau) == 2u);
	assert(get16(metadata + 3u * BLOCK + 4u) == 2u);
	/* Blocks after the last allocated (9), in order; that one now last. */
	assert(change.first == 10u);
	assert(get16(metadata + 3u * BLOCK + 6u) == 53u);
	assert(get16(metadata + 3u * BLOCK + 8u) == 12u);
	assert(card.count == 3u && card.freeBlocks == 53u);
	assert(card.directoryCopy == 0u && card.mapCopy == 0u);
	assert(card.next[10] == 11u && card.next[11] == 12u && card.next[12] == 65535u);
	assert(card.next[5] == 9u && card.next[9] == 65535u && card.next[6] == 65535u);
	/* The .gci's entry in the first free place, starting at its block. */
	at = UISavesRaw_Find(&card, entry);
	assert(at < card.count && card.directoryIndex[at] == 2u);
	assert(get16(card.entry[at] + 0x36u) == 10u);
	assert(!memcmp(card.entry[at], entry, 0x36u));
	assert(!memcmp(card.entry[at] + 0x38u, entry + 0x38u, 8u));
	assert(UISavesRaw_GciSize(&card, at) == UI_SAVES_ENTRY_SIZE + 3u * BLOCK);

	/* Round from the last block to the first data block, skipping used. */
	fixture();
	for(unsigned copy = 3u; copy <= 4u; ++copy) {
		put16(metadata + copy * BLOCK + 8u, 62u);
	}
	fix();
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_OK);
	assert(change.first == 63u && card.next[63] == 7u && card.next[7] == 8u &&
		card.next[8] == 65535u);
	assert(get16(metadata + 3u * BLOCK + 8u) == 8u);
	/* An out-of-range last block starts at the first data block. */
	fixture();
	for(unsigned copy = 3u; copy <= 4u; ++copy) {
		put16(metadata + copy * BLOCK + 8u, 65535u);
	}
	fix();
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_OK);
	assert(change.first == 7u && card.next[7] == 8u && card.next[8] == 10u);

	/* A block the map holds without a save is never taken. */
	fixture();
	for(unsigned copy = 3u; copy <= 4u; ++copy) {
		put16(metadata + copy * BLOCK + 10u * 2u, 65535u);
		put16(metadata + copy * BLOCK + 6u, 55u);
	}
	fix();
	gciEntry(entry, "NEWS01", "New Save", 1u);
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_OK);
	assert(change.first == 11u && card.next[10] == 65535u && card.freeBlocks == 54u);

	/* Every free block, exactly. */
	fixture();
	gciEntry(entry, "BIGS01", "Big Save", 56u);
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_OK);
	assert(card.freeBlocks == 0u && card.count == 3u);
}

static void testAddRefused(void)
{
	uint8_t entry[UI_SAVES_ENTRY_SIZE];

	fixture();
	memcpy(before, metadata, sizeof(metadata));
	/* The same game, maker and name, whatever else differs. */
	gciEntry(entry, "DEMO01", "Demo Save", 1u);
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_EXISTS);
	assert(unchanged() && change.directory[0] == 0u && change.map[0] == 0u);
	gciEntry(entry, "DEMO02", "Demo Save", 1u);
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_OK);
	fixture();
	gciEntry(entry, "BIGS01", "Big Save", 57u);
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_NO_ROOM);
	assert(unchanged());
	gciEntry(entry, "NEWS01", "New Save", 0u);
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_INVALID_ARGUMENT);
	/* A first byte of 0xff is a free place, or libogc2's wildcard. */
	gciEntry(entry, "NEWS01", "New Save", 1u);
	entry[0] = 0xffu;
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_INVALID_ARGUMENT);
	gciEntry(entry, "NEWS01", "New Save", 1u);
	entry[4] = 0xffu;
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_INVALID_ARGUMENT);
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, NULL, &card, &change) ==
		UI_SAVES_RAW_INVALID_ARGUMENT);
	gciEntry(entry, "NEWS01", "New Save", 1u);
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, NULL, &change) ==
		UI_SAVES_RAW_INVALID_ARGUMENT);
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, &card, NULL) ==
		UI_SAVES_RAW_INVALID_ARGUMENT);
	assert(unchanged());
	/* An image that doesn't read is never planned on. */
	metadata[0] ^= 1u;
	memcpy(before, metadata, sizeof(metadata));
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_INVALID_HEADER);
	assert(unchanged() && change.directory[0] == 0u);
	/* 127 saves fill the directory; 126 leave its last place. */
	manySaves(127u);
	memcpy(before, metadata, sizeof(metadata));
	assert(UISavesRaw_PlanAdd(metadata, 16u * 131072u, entry, &card, &change) ==
		UI_SAVES_RAW_FULL);
	assert(unchanged());
	manySaves(126u);
	assert(UISavesRaw_PlanAdd(metadata, 16u * 131072u, entry, &card, &change) ==
		UI_SAVES_RAW_OK);
	assert(card.count == 127u && card.directoryIndex[UISavesRaw_Find(&card, entry)] == 126u);
	assert(change.first == 131u);
}

static void testErase(void)
{
	uint8_t entry[UI_SAVES_ENTRY_SIZE];
	uint8_t other[UI_SAVES_ENTRY_SIZE];

	fixture();
	expect(UI_SAVES_RAW_OK);
	memcpy(entry, UISavesRaw_Entry(&card, 0u), sizeof(entry));
	memcpy(other, UISavesRaw_Entry(&card, 1u), sizeof(other));
	memcpy(before, metadata, sizeof(metadata));
	assert(UISavesRaw_PlanErase(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_OK);
	assert(change.directory[0] == 1u && change.directory[1] == 0u);
	assert(change.map[0] == 3u && change.map[1] == 0u);
	assert(sealed(1u) && sealed(3u));
	assert(!memcmp(metadata + 2u * BLOCK, before + 2u * BLOCK, BLOCK));
	assert(!memcmp(metadata + 4u * BLOCK, before + 4u * BLOCK, BLOCK));
	assert(card.count == 1u && card.freeBlocks == 58u);
	assert(card.next[5] == 0u && card.next[9] == 0u && card.next[6] == 65535u);
	assert(!memcmp(UISavesRaw_Entry(&card, 0u), other, sizeof(other)));
	for(unsigned i = 0u; i < UI_SAVES_ENTRY_SIZE; ++i) {
		assert(metadata[BLOCK + i] == 0xffu);
	}
	/* The last block allocated stays where it was; a new save follows it. */
	assert(get16(metadata + 3u * BLOCK + 8u) == 9u);
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_OK);
	assert(change.first == 10u && card.count == 2u && card.freeBlocks == 56u);
	assert(change.directory[0] == 2u && change.map[0] == 4u);
	assert(get16(metadata + 2u * BLOCK + 0x1ffau) == 3u);
	/* The save comes back at the free place its erasing left. */
	assert(card.directoryIndex[UISavesRaw_Find(&card, entry)] == 0u);

	/* Only the save exactly as it was listed. */
	fixture();
	memcpy(before, metadata, sizeof(metadata));
	entry[0x28] ^= 1u;
	assert(UISavesRaw_PlanErase(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_NOT_FOUND);
	assert(unchanged() && change.directory[0] == 0u);
	gciEntry(entry, "GONE01", "Gone Save", 1u);
	assert(UISavesRaw_PlanErase(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_NOT_FOUND);
	assert(UISavesRaw_PlanErase(metadata, IMAGE_SIZE, NULL, &card, &change) ==
		UI_SAVES_RAW_INVALID_ARGUMENT);
	assert(unchanged());
}

/* At the highest count the next compares lower, so the new version goes
 * into both copies, the older first, and they tie (the first is read). */
static void testCounterWrap(void)
{
	uint8_t entry[UI_SAVES_ENTRY_SIZE];

	fixture();
	put16(metadata + BLOCK + 0x1ffau, 32766u);
	put16(metadata + 2u * BLOCK + 0x1ffau, 32767u);
	put16(metadata + 4u * BLOCK + 4u, 32767u);
	fix();
	gciEntry(entry, "NEWS01", "New Save", 1u);
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_OK);
	assert(change.directory[0] == 1u && change.directory[1] == 2u);
	assert(change.map[0] == 3u && change.map[1] == 4u);
	assert(!memcmp(metadata + BLOCK, metadata + 2u * BLOCK, BLOCK));
	assert(!memcmp(metadata + 3u * BLOCK, metadata + 4u * BLOCK, BLOCK));
	assert(get16(metadata + BLOCK + 0x1ffau) == 32768u);
	assert(get16(metadata + 3u * BLOCK + 4u) == 32768u);
	assert(sealed(1u) && sealed(2u) && sealed(3u) && sealed(4u));
	assert(card.count == 3u && card.directoryCopy == 0u);
	/* And the count goes on from there. The entry may be the card's own. */
	assert(UISavesRaw_PlanErase(metadata, IMAGE_SIZE, card.entry[UISavesRaw_Find(&card, entry)],
		&card, &change) == UI_SAVES_RAW_OK);
	assert(change.directory[0] == 2u && change.directory[1] == 0u);
	assert(get16(metadata + 2u * BLOCK + 0x1ffau) == 32769u);
	assert(card.count == 2u && card.directoryCopy == 1u);
}

/* Each write is a whole system block. An add writes its data, then the map,
 * then the directory; an erase the directory, then the map. Stopped between
 * any two, or partway through one, the image reads as before or with the
 * blocks held and no save in them. */
static void testInterrupted(void)
{
	static uint8_t planned[UI_SAVES_RAW_METADATA_SIZE];
	uint8_t entry[UI_SAVES_ENTRY_SIZE];

	fixture();
	memcpy(before, metadata, sizeof(metadata));
	gciEntry(entry, "NEWS01", "New Save", 3u);
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_OK);
	memcpy(planned, metadata, sizeof(planned));
	/* The map only: the save's blocks held, the save not yet there. */
	memcpy(metadata, before, sizeof(metadata));
	memcpy(metadata + change.map[0] * BLOCK, planned + change.map[0] * BLOCK, BLOCK);
	expect(UI_SAVES_RAW_OK);
	assert(card.count == 2u && card.freeBlocks == 53u && card.next[10] == 11u);
	/* Half of the directory: it doesn't sum, so the other copy stands. */
	memcpy(metadata + change.directory[0] * BLOCK,
		planned + change.directory[0] * BLOCK, BLOCK / 2u);
	expect(UI_SAVES_RAW_OK);
	assert(card.count == 2u && UISavesRaw_Find(&card, entry) == 2u);
	/* The map's first sums and counts without its chain (on a small card
	 * every change is in its first sector): as if nothing had been written. */
	memcpy(metadata, before, sizeof(metadata));
	memcpy(metadata + change.map[0] * BLOCK, planned + change.map[0] * BLOCK, 8u);
	expect(UI_SAVES_RAW_OK);
	assert(card.count == 2u && card.freeBlocks == 56u && card.next[10] == 0u);

	fixture();
	memcpy(before, metadata, sizeof(metadata));
	expect(UI_SAVES_RAW_OK);
	memcpy(entry, UISavesRaw_Entry(&card, 0u), sizeof(entry));
	assert(UISavesRaw_PlanErase(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_OK);
	memcpy(planned, metadata, sizeof(planned));
	/* The directory only: the save gone, its blocks still held. */
	memcpy(metadata, before, sizeof(metadata));
	memcpy(metadata + change.directory[0] * BLOCK,
		planned + change.directory[0] * BLOCK, BLOCK);
	expect(UI_SAVES_RAW_OK);
	assert(card.count == 1u && card.freeBlocks == 56u && card.next[5] == 9u);
	/* And the next change starts from there. */
	gciEntry(entry, "NEWS01", "New Save", 1u);
	assert(UISavesRaw_PlanAdd(metadata, IMAGE_SIZE, entry, &card, &change) ==
		UI_SAVES_RAW_OK);
	assert(change.first == 10u && card.count == 2u && card.freeBlocks == 55u);
}

int main(void)
{
	testReads();
	testMetadata();
	testGeometry();
	testAdd();
	testAddRefused();
	testErase();
	testCounterWrap();
	testInterrupted();
	puts("raw memory card parser and planner tests passed");
	return 0;
}
