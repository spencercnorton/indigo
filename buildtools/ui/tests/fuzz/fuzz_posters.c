/* Fuzz the poster pack (swiss/ui/posters.pak) as the Library loads it: any
 * file is refused or served without a sanitizer finding, and the cache always
 * tears down cleanly afterwards. Bytes past the header also pick the game IDs
 * the Library asks for, so a fuzzed index gets queried too. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "ui_assets.h"

static uint32_t be32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static void put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

/* A pack's CRCs guard it against a damaged card, and a fuzzer cannot forge
 * them: nearly every mutation would stop at the first check. So recompute
 * every CRC the pack declares, each poster's and then the header and index's,
 * as ui_assets.c checks them. The fuzzer then tests what lies behind the
 * checks; the checks themselves have their own tests. */
static void fixCrcs(uint8_t *pack, size_t size)
{
	uint32_t count, index, length;

	if (size < 64)
		return;
	count = be32(pack + 0x0C);
	index = be32(pack + 0x10);
	if (count > size / 32 || index > size || count * 32u > size - index)
		return;
	length = count * 32u;
	for (uint32_t at = index; at < index + length; at += 32) {
		uint32_t offset = be32(pack + at + 8);
		if (offset <= size && UI_ASSETS_POSTER_BYTES <= size - offset)
			put32(pack + at + 16, (uint32_t)crc32(0, pack + offset, UI_ASSETS_POSTER_BYTES));
	}
	put32(pack + 8, 0);
	put32(pack + 8, (uint32_t)crc32(crc32(0, pack, 64), pack + index, length));
}

static const uint8_t *packData;
static size_t packSize;
static u32 clockMs;

void *uiAssetsHostAllocAligned32(u32 size) { return aligned_alloc(32, (size + 31u) & ~31u); }
void *uiAssetsHostAlloc(u32 size) { return malloc(size ? size : 1); }
void uiAssetsHostFree(void *ptr) { free(ptr); }
void uiAssetsHostFlush(void *ptr, u32 len) { (void)ptr; (void)len; }

static s32 memRead(void *ctx, u32 offset, void *dst, u32 len)
{
	(void)ctx;
	if ((size_t)offset + len > packSize)
		return UI_ASSETS_ERR_IO;
	memcpy(dst, packData + offset, len);
	return (s32)len;
}

static u32 nowMs(void) { return clockMs; }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	uiAssetsSource_t source = { memRead, (u32)size, NULL, nowMs };
	char ids[4][8];
	uint8_t *pack = malloc(size ? size : 1);

	if (!pack)
		return 0;
	memcpy(pack, data, size);
	fixCrcs(pack, size);
	packData = pack;
	packSize = size;
	clockMs = 1000;
	if (UIAssets_Init(&source, NULL) == UI_ASSETS_OK) {
		memset(ids, 0, sizeof(ids));
		for (int i = 0; i < 4; i++) {
			/* Game IDs taken from the pack's own bytes past the header, so
			 * the fuzzer can steer a request onto a record it made. */
			for (int c = 0; c < UI_ASSETS_ID_LEN; c++) {
				size_t at = 64u + (size_t)i * 16u + (size_t)c;
				ids[i][c] = at < size ? (char)pack[at] : 'A';
			}
		}
		UIAssets_RequestWindow((const char (*)[8])ids, 4, 0);
		for (int step = 0; step < 32 && UIAssets_Poll(); step++)
			clockMs += UI_ASSETS_EVICT_QUARANTINE_MS + 1;
		for (int i = 0; i < 4; i++) {
			uiPosterHandle_t handle;
			u8 r, g, b;
			uiPosterResult_t result = UIAssets_Query(ids[i], UI_ASSETS_ID_LEN, i & 1, &handle);
			if (result == UI_POSTER_EXACT || result == UI_POSTER_UNIVERSAL) {
				(void)UIAssets_Peek(handle);
				if (UIAssets_Acquire(handle) != NULL)
					UIAssets_Release(handle);
			}
			(void)UIAssets_DominantColor(ids[i], UI_ASSETS_ID_LEN, &r, &g, &b);
		}
	}
	UIAssets_CancelForDeviceChange();
	if (UIAssets_DisposeAfterVideoStop() != UI_ASSETS_OK)
		abort(); /* the cache must always come apart cleanly */
	free(pack);
	return 0;
}
