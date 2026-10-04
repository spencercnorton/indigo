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
	fixture();
	for(unsigned copy = 3u; copy <= 4u; ++copy) {
		put16(metadata + copy * UI_SAVES_BLOCK_SIZE + 7u * 2u, 65535u);
		put16(metadata + copy * UI_SAVES_BLOCK_SIZE + 6u, 55u);
	}
	fix(); expect(UI_SAVES_RAW_INVALID_CHAIN);
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

int main(void)
{
	testReads();
	testMetadata();
	testGeometry();
	puts("raw memory card parser tests passed");
	return 0;
}
