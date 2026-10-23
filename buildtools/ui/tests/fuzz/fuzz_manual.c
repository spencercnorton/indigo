/* Fuzz a game's manual as the viewer reads it (swiss/ui/manuals/<ID>.pak):
 * its header, its index and every page's CRC, then page turns and reads
 * over it as the viewer makes them, steered by the file's own last bytes.
 * Any file is refused or read without a sanitizer finding; one that is
 * read never points past its own end, its pages draw on the screen, and
 * the two page slots keep their promises. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "ui_manual.h"

static uint32_t be32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static void put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

/* A manual's CRCs guard it against a damaged card, and a fuzzer cannot forge
 * them: nearly every mutation would stop at the first check. So recompute
 * every CRC the file declares, each page's and then the header and index's,
 * as ui_manual.c checks them. The checks themselves have their own tests. */
static void fixCrcs(uint8_t *file, size_t size)
{
	uint32_t pages, record, length;

	if (size < 64)
		return;
	pages = be32(file + 0x0C);
	record = be32(file + 0x20);
	if (pages > (size - 64) / 32)
		return;
	length = pages * 32u;
	for (uint32_t at = 64; at < 64 + length; at += 32) {
		uint32_t offset = be32(file + at);
		if (offset <= size && record <= size - offset)
			put32(file + at + 8, (uint32_t)crc32(0, file + offset, record));
	}
	put32(file + 8, 0);
	put32(file + 8, (uint32_t)crc32(crc32(0, file, 64), file + 64, length));
}

static void checkPlace(const uiManualPage_t *page)
{
	uiManualRect_t whole = UIManual_Place(page, true, 0, 1.0f);
	uiManualRect_t full = UIManual_Place(page, false, 0xFFFFFFFFu, 1.0f);
	uint32_t most = UIManual_MaxScroll(page, 1.0f);

	/* The whole page fits above the bar; full width never leaves the
	 * page's bottom above the bar, nor its top below the screen's. */
	if (whole.x < 0.0f || whole.y < 0.0f || whole.x + whole.width > 640.0f ||
	    whole.y + whole.height > 480.0f - UI_MANUAL_BAR || full.x < 0.0f ||
	    full.y * 2.0f > 480.0f - UI_MANUAL_BAR ||
	    (most > 0 && full.y + full.height < 480.0f - UI_MANUAL_BAR - 0.5f))
		abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	static uiManualPage_t pages[UI_MANUAL_MAX_PAGES];
	uiManualInfo_t info;
	uint8_t *file;

	if (size > 64u << 20)
		return 0;
	file = malloc(size ? size : 1);
	if (!file)
		return 0;
	memcpy(file, data, size);
	fixCrcs(file, size);
	if (size >= 64 && UIManual_Header(file, (uint32_t)size, &info) &&
	    UIManual_Index(file, file + 64, &info, pages)) {
		uiManualReader_t reader;
		uint32_t now = 0xFFFFFF00u;	/* wraps meanwhile */

		if ((uint64_t)info.dataOffset + (uint64_t)info.pages * info.recordBytes != size ||
		    info.pageBytes * 5u / 4u != info.recordBytes)
			abort();
		for (uint32_t n = 0; n < info.pages; n++) {
			(void)UIManual_RecordIntact(&info, &pages[n], file + UIManual_RecordOffset(&info, n));
			checkPlace(&pages[n]);
		}
		UIManual_ReaderInit(&reader, info.pages);
		for (size_t i = 0; i < 512 && i < size; i++) {
			uint8_t op = file[size - 1 - i];
			uint32_t slot, page;

			if (op & 1)
				UIManual_Go(&reader, op >> 1, now);
			if (UIManual_NextRead(&reader, now, &slot, &page)) {
				if (slot >= UI_MANUAL_SLOTS || page >= info.pages ||
				    (int32_t)slot == UIManual_Shown(&reader))
					abort();
				UIManual_ReadDone(&reader, slot, (op & 2) == 0, now);
			}
			if (UIManual_Shown(&reader) >= 0 &&
			    reader.slot[UIManual_Shown(&reader)].page != reader.page)
				abort();
			now += (op >> 5) + 1u;
		}
	}
	free(file);
	return 0;
}
