/*
 * Host test for ui_manual.c: a game's manual (/swiss/ui/manuals/<ID>.pak),
 * its checks, the viewer's two page slots and where a page draws.
 *
 * usage: test_ui_manual            the unit tests
 *        test_ui_manual FILE...    reads each file as the viewer does and
 *                                  prints "ok pages=N height=H format=F
 *                                  intact=K" or "refused" (test_manual_pack.py
 *                                  writes them with manual_pack.py)
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "ui_manual.h"

#define CHECK(condition) do { \
	if(!(condition)) { \
		fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
			#condition); \
		exit(EXIT_FAILURE); \
	} \
} while(0)

static void put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

static void put16(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 8);
	p[1] = (uint8_t)v;
}

/* A manual of three pages 640 x 32, written as manual_pack.py writes one;
 * *size its bytes. */
static uint8_t *makeManual(uint32_t *size)
{
	const uint32_t pages = 3u, height = 32u;
	const uint32_t record = 400u * height;
	const uint32_t data = 64u + pages * 32u;
	uint8_t *file;
	uint32_t n;

	*size = data + pages * record;
	file = calloc(1u, *size);
	CHECK(file != NULL);
	memcpy(file, "SWMN", 4u);
	put32(file + 0x04, 1u);
	put32(file + 0x0C, pages);
	put32(file + 0x10, 64u);
	put32(file + 0x14, pages * 32u);
	put32(file + 0x18, data);
	put32(file + 0x1C, *size);
	put32(file + 0x20, record);
	put16(file + 0x24, 640u);
	put16(file + 0x26, height);
	put16(file + 0x28, 320u);
	put16(file + 0x2A, height / 2u);
	file[0x2C] = 2u;
	file[0x2D] = UI_MANUAL_FORMAT_CMPR;
	for(n = 0u; n < pages; ++n) {
		uint8_t *entry = file + 64u + n * 32u;
		uint8_t *texels = file + data + n * record;

		memset(texels, (int)(0x40u + n), record);
		put32(entry, data + n * record);
		put32(entry + 0x04, record);
		put32(entry + 0x08, (uint32_t)crc32(0L, texels, record));
		put16(entry + 0x0C, 600u + n);
		put16(entry + 0x0E, 30u - n);
	}
	put32(file + 0x08, (uint32_t)crc32(crc32(0L, file, 64u), file + 64u,
		pages * 32u));
	return file;
}

/* The viewer's checks: header, then index, then every page's CRC. */
static int readManual(const uint8_t *file, uint32_t size, uiManualInfo_t *info,
	uiManualPage_t *pages)
{
	uint32_t n;
	int intact = 0;

	if(size < UI_MANUAL_HEADER_BYTES || !UIManual_Header(file, size, info) ||
		!UIManual_Index(file, file + UI_MANUAL_HEADER_BYTES, info, pages)) {
		return -1;
	}
	for(n = 0u; n < info->pages; ++n) {
		intact += UIManual_RecordIntact(info, &pages[n],
			file + UIManual_RecordOffset(info, n));
	}
	return intact;
}

/* A full-size page (640 x 1152 and its half), summed a piece at a time as
 * the console must (ui_manual.c), has the CRC of the whole. */
static void testBigRecordCrc(void)
{
	uiManualInfo_t info = {1u, 32u, 96u, 400u * 1152u, 320u * 1152u, 1152u,
		UI_MANUAL_FORMAT_CMPR};
	uiManualPage_t page = {0u, 640u, 1152u};
	uint8_t *record = malloc(info.recordBytes);
	uint32_t i;

	CHECK(record != NULL);
	for(i = 0u; i < info.recordBytes; ++i) {
		record[i] = (uint8_t)(i * 2654435761u >> 24);
	}
	page.crc = (uint32_t)crc32(0uL, record, info.recordBytes);
	CHECK(UIManual_RecordIntact(&info, &page, record));
	record[info.recordBytes - 1u] = (uint8_t)(record[info.recordBytes - 1u] ^ 1u);
	CHECK(!UIManual_RecordIntact(&info, &page, record));
	free(record);
}

static void testFile(void)
{
	static uiManualPage_t pages[UI_MANUAL_MAX_PAGES];
	uiManualInfo_t info;
	uint32_t size;
	uint8_t *file = makeManual(&size);

	CHECK(readManual(file, size, &info, pages) == 3);
	CHECK(info.pages == 3u && info.height == 32u && info.format == UI_MANUAL_FORMAT_CMPR);
	CHECK(info.recordBytes == 12800u && info.pageBytes == 10240u);
	CHECK(info.dataOffset == 160u && UIManual_RecordOffset(&info, 2u) == 160u + 25600u);
	CHECK(pages[1].width == 601u && pages[1].height == 29u);
	/* The file's real size, exactly. */
	CHECK(!UIManual_Header(file, size + 1u, &info));
	CHECK(!UIManual_Header(file, size - 1u, &info));
	/* Any bit of the header or the index changed: refused before a page is
	 * read, by a field's check or by the CRC. */
	for(uint32_t at = 0u; at < 64u + 3u * 32u; ++at) {
		for(uint32_t bit = 0u; bit < 8u; ++bit) {
			file[at] = (uint8_t)(file[at] ^ (1u << bit));
			CHECK(readManual(file, size, &info, pages) < 0);
			file[at] = (uint8_t)(file[at] ^ (1u << bit));
		}
	}
	/* A damaged page is only that page. */
	file[160u + 12800u + 7u] = (uint8_t)(file[160u + 12800u + 7u] ^ 1u);
	CHECK(readManual(file, size, &info, pages) == 2);
	CHECK(!UIManual_RecordIntact(&info, &pages[1], file + UIManual_RecordOffset(&info, 1u)));
	CHECK(UIManual_RecordIntact(&info, &pages[2], file + UIManual_RecordOffset(&info, 2u)));
	free(file);
}

/* Fields consistent with each other but out of bounds: refused too. */
static void testBounds(void)
{
	uint8_t header[64];
	uiManualInfo_t info;
	uint32_t size;
	uint8_t *file = makeManual(&size);

	memcpy(header, file, sizeof(header));
	free(file);
	CHECK(UIManual_Header(header, size, &info));
	/* Too tall, not a multiple of 16, gray, another level count. */
	for(uint32_t height = 0u; height <= 2048u; height += 8u) {
		uint32_t pages = 3u;
		uint32_t record = 400u * height;

		put16(header + 0x26, height);
		put16(header + 0x2A, height / 2u);
		put32(header + 0x20, record);
		put32(header + 0x1C, 160u + pages * record);
		CHECK(UIManual_Header(header, 160u + pages * record, &info) ==
			(height >= 16u && height <= 1152u && height % 16u == 0u));
	}
	header[0x2D] = UI_MANUAL_FORMAT_I4;
	put16(header + 0x26, 32u);
	put16(header + 0x2A, 16u);
	put32(header + 0x20, 12800u);
	put32(header + 0x1C, 160u + 3u * 12800u);
	CHECK(UIManual_Header(header, 160u + 3u * 12800u, &info) &&
		info.format == UI_MANUAL_FORMAT_I4);
	header[0x2D] = 1u;	/* another GX format */
	CHECK(!UIManual_Header(header, 160u + 3u * 12800u, &info));
	header[0x2D] = UI_MANUAL_FORMAT_CMPR;
	/* 257 pages, then none. */
	put32(header + 0x0C, 257u);
	CHECK(!UIManual_Header(header, 160u + 3u * 12800u, &info));
	put32(header + 0x0C, 0u);
	CHECK(!UIManual_Header(header, 160u + 3u * 12800u, &info));
	CHECK(!UIManual_Header(NULL, 0u, &info));
}

static void readAll(uiManualReader_t *reader, uint32_t now, uint32_t *reads)
{
	uint32_t slot, page;

	while(UIManual_NextRead(reader, now, &slot, &page)) {
		CHECK(reader->slot[slot].state == UI_MANUAL_SLOT_READING);
		CHECK(!UIManual_NextRead(reader, now, &slot, &page));
		UIManual_ReadDone(reader, slot, true, now);
		++*reads;
	}
}

static void testSlots(void)
{
	uiManualReader_t reader;
	uint32_t slot, page, reads = 0u;
	uint32_t now = 0xFFFFFFF0u;	/* the frame count wraps meanwhile */

	UIManual_ReaderInit(&reader, 5u);
	CHECK(UIManual_Shown(&reader) == -1);
	/* The page shown first, then the next. */
	CHECK(UIManual_NextRead(&reader, now, &slot, &page) && page == 0u);
	CHECK(!UIManual_NextRead(&reader, now, &slot, &page));	/* one at a time */
	UIManual_ReadDone(&reader, 0u, true, now);
	CHECK(UIManual_Shown(&reader) == 0);
	CHECK(UIManual_NextRead(&reader, now, &slot, &page) && slot == 1u && page == 1u);
	UIManual_ReadDone(&reader, slot, true, now);
	CHECK(!UIManual_NextRead(&reader, now, &slot, &page));
	/* Right: page 1 shows at once, from the page read ahead. Page 0's slot
	 * rests while the GPU may draw it, then takes page 2. */
	UIManual_Go(&reader, 1u, now);
	CHECK(UIManual_Shown(&reader) == 1);
	CHECK(!UIManual_NextRead(&reader, now + 1u, &slot, &page));
	CHECK(UIManual_NextRead(&reader, now + 2u, &slot, &page) && slot == 0u && page == 2u);
	UIManual_ReadDone(&reader, slot, true, now + 2u);
	now += 30u;
	/* Left: page 0 isn't held; it is read, then page 0 has nothing before
	 * it, so the read ahead turns forward again. */
	UIManual_Go(&reader, 0u, now);
	CHECK(UIManual_Shown(&reader) == -1);
	CHECK(reader.step == 1);
	/* Page 1's slot has just stopped showing: it rests; page 2's is free. */
	CHECK(UIManual_NextRead(&reader, now, &slot, &page) && slot == 0u && page == 0u);
	UIManual_ReadDone(&reader, slot, true, now);
	CHECK(UIManual_Shown(&reader) == 0);
	CHECK(!UIManual_NextRead(&reader, now, &slot, &page));	/* page 1 is still held */
	/* R: the last page, then the one before it. */
	UIManual_Go(&reader, 99u, now + 10u);
	CHECK(reader.page == 4u && reader.step == -1);
	readAll(&reader, now + 20u, &reads);
	CHECK(reads == 2u && UIManual_Shown(&reader) >= 0);
	CHECK(reader.slot[0].page + reader.slot[1].page == 7u);
	/* Turning back reads ahead backwards. */
	UIManual_Go(&reader, 3u, now + 30u);
	CHECK(UIManual_Shown(&reader) >= 0 && reader.step == -1);
	CHECK(UIManual_NextRead(&reader, now + 40u, &slot, &page) && page == 2u);
	UIManual_ReadDone(&reader, slot, true, now + 40u);
	/* A page that can't be read: said once, never read again in a loop; the
	 * page after it is still read ahead. */
	UIManual_Go(&reader, 0u, now + 50u);
	CHECK(UIManual_NextRead(&reader, now + 60u, &slot, &page) && page == 0u);
	UIManual_ReadDone(&reader, slot, false, now + 60u);
	CHECK(UIManual_ShownFailed(&reader) && UIManual_Shown(&reader) == -1);
	CHECK(UIManual_NextRead(&reader, now + 70u, &slot, &page) && page == 1u);
	UIManual_ReadDone(&reader, slot, true, now + 70u);
	CHECK(!UIManual_NextRead(&reader, now + 80u, &slot, &page));
	/* A finished read for a slot not reading is ignored. */
	UIManual_ReadDone(&reader, 0u, true, now + 80u);
	UIManual_ReadDone(&reader, 7u, true, now + 80u);
	CHECK(UIManual_ShownFailed(&reader));
	/* One page: nothing to read ahead. */
	UIManual_ReaderInit(&reader, 1u);
	reads = 0u;
	readAll(&reader, 5u, &reads);
	CHECK(reads == 1u && UIManual_Shown(&reader) >= 0);
	UIManual_Go(&reader, 3u, 6u);
	CHECK(reader.page == 0u);
	/* No pages: nothing at all. */
	UIManual_ReaderInit(&reader, 0u);
	CHECK(!UIManual_NextRead(&reader, 0u, &slot, &page));
	UIManual_Go(&reader, 1u, 0u);
	CHECK(UIManual_Shown(&reader) == -1);
}

/* Fast page turns: whatever order the reads finish in, the page shown is
 * the last one asked for, and no slot is read into while it shows. */
static void testTurns(void)
{
	uiManualReader_t reader;
	uint32_t seed = 12345u;
	uint32_t now = 100u;
	uint32_t i;

	UIManual_ReaderInit(&reader, 40u);
	for(i = 0u; i < 20000u; ++i) {
		uint32_t slot, page;

		seed = seed * 1103515245u + 12345u;
		if((seed >> 16) % 3u == 0u) {
			UIManual_Go(&reader, (seed >> 8) % 40u, now);
		}
		if(UIManual_NextRead(&reader, now, &slot, &page)) {
			CHECK((int32_t)slot != UIManual_Shown(&reader));
			CHECK(page < 40u);
			UIManual_ReadDone(&reader, slot, true, now);
		}
		if(UIManual_Shown(&reader) >= 0) {
			CHECK(reader.slot[UIManual_Shown(&reader)].page == reader.page);
		}
		now += 1u + (seed >> 28);
	}
	/* Left alone, the page shown is always read in the end. */
	for(i = 0u; i < 10u; ++i) {
		uint32_t slot, page;

		now += 3u;
		if(UIManual_NextRead(&reader, now, &slot, &page)) {
			UIManual_ReadDone(&reader, slot, true, now);
		}
	}
	CHECK(UIManual_Shown(&reader) >= 0);
}

static void testPlace(void)
{
	uiManualPage_t tall = {0u, 640u, 1100u};
	uiManualPage_t narrow = {0u, 614u, 1152u};
	uiManualPage_t shortPage = {0u, 640u, 300u};
	uiManualRect_t r;

	/* Full width: one texel a pixel, from the page's top; the page's bottom
	 * stops at the bar. */
	CHECK(UIManual_MaxScroll(&tall, 1.0f) == 1100u - (480u - UI_MANUAL_BAR));
	r = UIManual_Place(&tall, false, 0u, 1.0f);
	CHECK(r.x == 0.0f && r.y == 0.0f && r.width == 640.0f && r.height == 1100.0f);
	r = UIManual_Place(&tall, false, 9999u, 1.0f);
	CHECK(r.y == -(float)UIManual_MaxScroll(&tall, 1.0f));
	r = UIManual_Place(&narrow, false, 10u, 1.0f);
	CHECK(r.x == 13.0f && r.y == -10.0f);
	/* A page shorter than the screen sits in the middle, still. */
	CHECK(UIManual_MaxScroll(&shortPage, 1.0f) == 0u);
	r = UIManual_Place(&shortPage, false, 40u, 1.0f);
	CHECK(r.y == 75.0f);
	/* Filling a 16:9 screen: 4/3 bigger, more to scroll. */
	CHECK(UIManual_MaxScroll(&tall, 4.0f / 3.0f) == 1017u);
	r = UIManual_Place(&tall, false, 0u, 4.0f / 3.0f);
	CHECK(r.x < -106.0f && r.x > -108.0f);
	/* The whole page: its half copy fitted above the bar, in the middle. */
	r = UIManual_Place(&tall, true, 500u, 1.0f);
	CHECK(r.height <= 480.0f - UI_MANUAL_BAR - 24.0f + 0.01f);
	CHECK(r.y >= 0.0f && r.y + r.height <= 480.0f - UI_MANUAL_BAR);
	CHECK(r.x > 150.0f && r.x + r.width < 490.0f);
	CHECK(r.width / r.height > 0.58f && r.width / r.height < 0.59f);
	/* ...never more than the page's own size. */
	r = UIManual_Place(&shortPage, true, 0u, 1.0f);
	CHECK(r.width <= 640.0f && r.height <= 300.0f);
}

static int readFiles(int argc, char **argv)
{
	static uiManualPage_t pages[UI_MANUAL_MAX_PAGES];
	int i;

	for(i = 1; i < argc; ++i) {
		FILE *f = fopen(argv[i], "rb");
		uint8_t *file;
		long size;
		uiManualInfo_t info;
		int intact;

		CHECK(f != NULL);
		CHECK(fseek(f, 0L, SEEK_END) == 0 && (size = ftell(f)) >= 0);
		rewind(f);
		file = malloc(size > 0 ? (size_t)size : 1u);
		CHECK(file != NULL && fread(file, 1u, (size_t)size, f) == (size_t)size);
		fclose(f);
		intact = readManual(file, (uint32_t)size, &info, pages);
		if(intact < 0) {
			printf("refused\n");
		}
		else {
			printf("ok pages=%u height=%u format=%u intact=%d\n",
				(unsigned)info.pages, (unsigned)info.height,
				(unsigned)info.format, intact);
		}
		free(file);
	}
	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	if(argc > 1) {
		return readFiles(argc, argv);
	}
	testFile();
	testBigRecordCrc();
	testBounds();
	testSlots();
	testTurns();
	testPlace();
	printf("test_ui_manual passed\n");
	return EXIT_SUCCESS;
}
