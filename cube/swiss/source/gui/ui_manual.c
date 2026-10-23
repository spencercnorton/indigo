/* ui_manual.c - a game's manual: the file's checks, the viewer's two page
 * slots and where a page draws. See ui_manual.h; the format is
 * docs/MANUALS.md, and buildtools/ui/manual_pack.py writes it. */

#include <math.h>
#include <string.h>
#include <zlib.h>

#include "ui_manual.h"

#define MANUAL_MAGIC 0x53574D4Eu	/* 'SWMN' */
#define MANUAL_VERSION 1u
#define MANUAL_LEVELS 2u	/* the page, then its half */
#define STAGE_HEIGHT 480.0f
#define WHOLE_MARGIN 12.0f
/* libogc2's zlib (zlib-ng) takes a big buffer's CRC with a 64 KiB table on
 * the stack, more than the page reader's thread has: a page is summed a
 * piece at a time instead, each too small for that. */
#define CRC_PIECE (16u * 1024u)

static uint32_t be32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
		(uint32_t)p[2] << 8 | (uint32_t)p[3];
}

static uint32_t be16(const uint8_t *p)
{
	return (uint32_t)p[0] << 8 | (uint32_t)p[1];
}

static bool allZero(const uint8_t *p, uint32_t count)
{
	while(count-- > 0u) {
		if(*p++ != 0u) {
			return false;
		}
	}
	return true;
}

bool UIManual_Header(const uint8_t *header, uint32_t fileSize,
	uiManualInfo_t *info)
{
	uint32_t pages;
	uint32_t height;
	uint32_t page;
	uint32_t record;
	uint32_t index;
	uint64_t length;

	if(header == NULL || info == NULL) {
		return false;
	}
	pages = be32(header + 0x0C);
	height = be16(header + 0x26);
	if(be32(header) != MANUAL_MAGIC || be32(header + 0x04) != MANUAL_VERSION ||
		pages < 1u || pages > UI_MANUAL_MAX_PAGES ||
		height < 16u || height > UI_MANUAL_MAX_HEIGHT || height % 16u != 0u) {
		return false;
	}
	/* 4 bits a texel, the page 640 wide and its half 320. */
	page = UI_MANUAL_WIDTH * height / 2u;
	record = page + page / 4u;
	index = pages * UI_MANUAL_ENTRY_BYTES;
	length = (uint64_t)UI_MANUAL_HEADER_BYTES + index + (uint64_t)pages * record;
	if(be32(header + 0x10) != UI_MANUAL_HEADER_BYTES ||
		be32(header + 0x14) != index ||
		be32(header + 0x18) != UI_MANUAL_HEADER_BYTES + index ||
		be32(header + 0x1C) != length || length != fileSize ||
		be32(header + 0x20) != record ||
		be16(header + 0x24) != UI_MANUAL_WIDTH ||
		be16(header + 0x28) != UI_MANUAL_WIDTH / 2u ||
		be16(header + 0x2A) != height / 2u ||
		header[0x2C] != MANUAL_LEVELS ||
		(header[0x2D] != UI_MANUAL_FORMAT_I4 &&
			header[0x2D] != UI_MANUAL_FORMAT_CMPR) ||
		!allZero(header + 0x2E, UI_MANUAL_HEADER_BYTES - 0x2Eu)) {
		return false;
	}
	info->pages = pages;
	info->indexBytes = index;
	info->dataOffset = UI_MANUAL_HEADER_BYTES + index;
	info->recordBytes = record;
	info->pageBytes = page;
	info->height = (uint16_t)height;
	info->format = header[0x2D];
	return true;
}

bool UIManual_Index(const uint8_t *header, const uint8_t *index,
	const uiManualInfo_t *info, uiManualPage_t *pages)
{
	uint8_t blank[UI_MANUAL_HEADER_BYTES];
	uLong crc;
	uint32_t n;

	if(header == NULL || index == NULL || info == NULL || pages == NULL) {
		return false;
	}
	memcpy(blank, header, sizeof(blank));
	memset(blank + 0x08, 0, 4u);
	crc = crc32(0uL, blank, UI_MANUAL_HEADER_BYTES);
	crc = crc32(crc, index, info->indexBytes);
	if((uint32_t)crc != be32(header + 0x08)) {
		return false;
	}
	for(n = 0u; n < info->pages; ++n) {
		const uint8_t *entry = index + n * UI_MANUAL_ENTRY_BYTES;
		uint32_t width = be16(entry + 0x0C);
		uint32_t height = be16(entry + 0x0E);

		if(be32(entry) != UIManual_RecordOffset(info, n) ||
			be32(entry + 0x04) != info->recordBytes ||
			width < 1u || width > UI_MANUAL_WIDTH ||
			height < 1u || height > info->height ||
			!allZero(entry + 0x10, UI_MANUAL_ENTRY_BYTES - 0x10u)) {
			return false;
		}
		pages[n].crc = be32(entry + 0x08);
		pages[n].width = (uint16_t)width;
		pages[n].height = (uint16_t)height;
	}
	return true;
}

uint32_t UIManual_RecordOffset(const uiManualInfo_t *info, uint32_t n)
{
	return info->dataOffset + n * info->recordBytes;
}

bool UIManual_RecordIntact(const uiManualInfo_t *info,
	const uiManualPage_t *page, const uint8_t *record)
{
	uLong crc = crc32(0uL, Z_NULL, 0u);
	uint32_t left;

	if(info == NULL || page == NULL || record == NULL) {
		return false;
	}
	for(left = info->recordBytes; left > 0u; ) {
		uint32_t piece = left < CRC_PIECE ? left : CRC_PIECE;

		crc = crc32(crc, record, piece);
		record += piece;
		left -= piece;
	}
	return (uint32_t)crc == page->crc;
}

void UIManual_ReaderInit(uiManualReader_t *reader, uint32_t pages)
{
	memset(reader, 0, sizeof(*reader));
	reader->pages = pages;
	reader->step = 1;
	reader->shown = -1;
}

/* The slot drawn is the shown page's, once it is ready; the slot that stops
 * being drawn rests a few frames. */
static void settle(uiManualReader_t *reader, uint32_t now)
{
	int32_t shown = -1;
	uint32_t s;

	for(s = 0u; s < UI_MANUAL_SLOTS; ++s) {
		if(reader->slot[s].state == UI_MANUAL_SLOT_READY &&
			reader->slot[s].page == reader->page) {
			shown = (int32_t)s;
		}
	}
	if(shown != reader->shown && reader->shown >= 0) {
		uiManualSlot_t *left = &reader->slot[(uint32_t)reader->shown];

		left->restUntil = now + UI_MANUAL_GPU_FRAMES;
		left->resting = 1u;
	}
	reader->shown = shown;
}

void UIManual_Go(uiManualReader_t *reader, uint32_t page, uint32_t now)
{
	if(reader->pages == 0u) {
		return;
	}
	if(page >= reader->pages) {
		page = reader->pages - 1u;
	}
	if(page == reader->page) {
		return;
	}
	/* Read ahead the way the pages turn; at either end, back into the
	 * manual (L and R jump there). */
	reader->step = page == 0u ? 1 : page + 1u == reader->pages ? -1 :
		page > reader->page ? 1 : -1;
	reader->page = page;
	settle(reader, now);
}

static bool holds(const uiManualSlot_t *slot, uint32_t page)
{
	return (slot->state == UI_MANUAL_SLOT_READY ||
		slot->state == UI_MANUAL_SLOT_FAILED ||
		slot->state == UI_MANUAL_SLOT_READING) && slot->page == page;
}

bool UIManual_NextRead(uiManualReader_t *reader, uint32_t now,
	uint32_t *slot, uint32_t *page)
{
	uint32_t want[2];
	uint32_t wants = 1u;
	uint32_t w;
	uint32_t s;

	if(reader->pages == 0u) {
		return false;
	}
	for(s = 0u; s < UI_MANUAL_SLOTS; ++s) {
		if(reader->slot[s].state == UI_MANUAL_SLOT_READING) {
			return false;	/* one read at a time */
		}
	}
	want[0] = reader->page;
	if(reader->step > 0 ? reader->page + 1u < reader->pages : reader->page > 0u) {
		want[wants++] = reader->step > 0 ? reader->page + 1u : reader->page - 1u;
	}
	for(w = 0u; w < wants; ++w) {
		uiManualSlot_t *chosen;
		int32_t best = -1;
		int score = 0;

		if(holds(&reader->slot[0], want[w]) || holds(&reader->slot[1], want[w])) {
			continue;
		}
		for(s = 0u; s < UI_MANUAL_SLOTS; ++s) {
			const uiManualSlot_t *candidate = &reader->slot[s];
			int value;

			if(holds(candidate, want[0])) {
				continue;	/* the page shown stays */
			}
			value = candidate->state == UI_MANUAL_SLOT_EMPTY ? 3 :
				wants > 1u && holds(candidate, want[1]) ? 1 : 2;
			if(value > score) {
				score = value;
				best = (int32_t)s;
			}
		}
		if(best < 0) {
			return false;
		}
		chosen = &reader->slot[(uint32_t)best];
		if(chosen->resting != 0u) {
			if((int32_t)(now - chosen->restUntil) < 0) {
				return false;	/* the GPU may still be drawing it */
			}
			chosen->resting = 0u;
		}
		chosen->state = UI_MANUAL_SLOT_READING;
		chosen->page = want[w];
		*slot = (uint32_t)best;
		*page = want[w];
		return true;
	}
	return false;
}

void UIManual_ReadDone(uiManualReader_t *reader, uint32_t slot, bool ok,
	uint32_t now)
{
	if(slot >= UI_MANUAL_SLOTS ||
		reader->slot[slot].state != UI_MANUAL_SLOT_READING) {
		return;
	}
	reader->slot[slot].state = (uint8_t)(ok ? UI_MANUAL_SLOT_READY :
		UI_MANUAL_SLOT_FAILED);
	settle(reader, now);
}

int32_t UIManual_Shown(const uiManualReader_t *reader)
{
	return reader->shown;
}

bool UIManual_ShownFailed(const uiManualReader_t *reader)
{
	uint32_t s;

	for(s = 0u; s < UI_MANUAL_SLOTS; ++s) {
		if(reader->slot[s].state == UI_MANUAL_SLOT_FAILED &&
			reader->slot[s].page == reader->page) {
			return true;
		}
	}
	return false;
}

static float visibleRows(void)
{
	return STAGE_HEIGHT - (float)UI_MANUAL_BAR;
}

uint32_t UIManual_MaxScroll(const uiManualPage_t *page, float zoom)
{
	float over = (float)page->height * zoom - visibleRows();

	return over > 0.0f ? (uint32_t)(over + 0.5f) : 0u;
}

uiManualRect_t UIManual_Place(const uiManualPage_t *page, bool whole,
	uint32_t scroll, float zoom)
{
	uiManualRect_t rect;
	uint32_t most;

	if(whole) {
		/* The half copy, as big as fits above the bar, never past the
		 * page's own size. */
		float width = (float)((page->width + 1u) / 2u);
		float height = (float)((page->height + 1u) / 2u);
		float scale = (visibleRows() - 2.0f * WHOLE_MARGIN) / height;
		float across = ((float)UI_MANUAL_WIDTH - 2.0f * WHOLE_MARGIN) / width;

		if(across < scale) {
			scale = across;
		}
		if(scale > 2.0f) {
			scale = 2.0f;
		}
		rect.width = width * scale;
		rect.height = height * scale;
		rect.x = floorf(((float)UI_MANUAL_WIDTH - rect.width) / 2.0f);
		rect.y = floorf((visibleRows() - rect.height) / 2.0f);
		return rect;
	}
	rect.width = (float)page->width * zoom;
	rect.height = (float)page->height * zoom;
	rect.x = floorf(((float)UI_MANUAL_WIDTH - rect.width) / 2.0f);
	most = UIManual_MaxScroll(page, zoom);
	if(rect.height < visibleRows()) {
		rect.y = floorf((visibleRows() - rect.height) / 2.0f);
	}
	else {
		rect.y = -(float)(scroll < most ? scroll : most);
	}
	return rect;
}

bool UIManual_Fits(uint32_t freeBytes, const uiManualInfo_t *info)
{
	return info != NULL && (uint64_t)freeBytes >=
		(uint64_t)UI_MANUAL_SLOTS * info->recordBytes + UI_MANUAL_MEMORY_MARGIN;
}
