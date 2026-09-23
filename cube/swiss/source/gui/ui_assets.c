/*
 * ui_assets.c -- bounded poster cache (Phase 4A).
 *
 * Pack format and budgets: docs/ui-redesign/POSTER_PACK_FORMAT.md.
 * Contract, locking rules, and integration sketch: ui_assets.h.
 *
 * Compiles for the GameCube target and, with -DUI_ASSETS_HOST_BUILD, for
 * host test harnesses (buildtools/ui/tests/) with GX and allocation stubs.
 *
 * Concurrency model: one external critical section (the video mutex,
 * supplied as callbacks at Init). Menu-thread APIs enter it around every
 * metadata mutation and publication; video-thread APIs (Query/Peek/
 * DominantColor) are read-only and run with the caller already holding
 * it, so they never lock. The lock is never held across source.read, CRC,
 * allocation, or free: Init stages and validates the new index in private
 * memory before publishing it, Poll captures a load job inside the lock,
 * performs the bounded read + CRC + cache flush outside it, then
 * revalidates slot ownership (generation/state/record) inside the lock
 * before publishing the texture or failure. Published index memory is
 * detached inside the critical section before free, so no locked reader
 * can observe it. The arena is freed only by the separate lock-free final
 * disposal after all readers stop. Slot generations are u32 and advance
 * exactly once per new slot ownership (at assignment), never on eviction.
 */

#include <string.h>
#include <zlib.h>

#include "ui_assets.h"

#ifdef UI_ASSETS_HOST_BUILD

/* GX constants mirrored from libogc for the host build. */
#define GX_TF_CMPR 14
#define GX_CLAMP 0
#define GX_FALSE 0
#define GX_TRUE 1
#define GX_LINEAR 1
#define GX_LIN_MIP_LIN 5
#define GX_ANISO_1 0

/* Provided by the test harness so allocations can be counted. */
extern void *uiAssetsHostAllocAligned32(u32 size);
extern void *uiAssetsHostAlloc(u32 size);
extern void uiAssetsHostFree(void *ptr);

#define UIA_ALLOC_ALIGNED32(size) uiAssetsHostAllocAligned32(size)
#define UIA_ALLOC(size) uiAssetsHostAlloc(size)
#define UIA_FREE(ptr) uiAssetsHostFree(ptr)

static void GX_InitTexObj(GXTexObj *obj, void *img, u16 wd, u16 ht, u8 fmt,
                          u8 wrap_s, u8 wrap_t, u8 mipmap) {
	(void)wrap_s; (void)wrap_t;
	obj->data = img;
	obj->width = wd;
	obj->height = ht;
	obj->format = fmt;
	obj->mipmap = mipmap;
	obj->lodConfigured = 0;
}

static void GX_InitTexObjLOD(GXTexObj *obj, u8 minfilt, u8 magfilt,
                             float minlod, float maxlod, float lodbias,
                             u8 biasclamp, u8 edgelod, u8 maxaniso) {
	(void)minfilt; (void)magfilt; (void)lodbias;
	(void)biasclamp; (void)edgelod; (void)maxaniso;
	obj->minlod = minlod;
	obj->maxlod = maxlod;
	obj->lodConfigured = 1;
}

/* Routed to the harness so tests can assert the flush happens outside the
 * critical section, once per successful load. */
extern void uiAssetsHostFlush(void *ptr, u32 len);

static void DCFlushRange(void *ptr, u32 len) {
	uiAssetsHostFlush(ptr, len);
}

#else /* target */

#include <malloc.h>
#include <ogc/cache.h>
#include <ogc/lwp_watchdog.h>

#define UIA_ALLOC_ALIGNED32(size) memalign(32, size)
#define UIA_ALLOC(size) malloc(size)
#define UIA_FREE(ptr) free(ptr)

#endif

/* Pack layout constants (mirrors buildtools/ui/poster_pack.py). */
#define PAK_MAGIC 0x5357504Bu /* 'SWPK' */
#define PAK_VERSION 1
#define PAK_HEADER_SIZE 64
#define PAK_RECORD_SIZE 32
#define PAK_FLAG_UNIVERSAL 0x01
#define PAK_FLAGS_KNOWN PAK_FLAG_UNIVERSAL

enum {
	SLOT_EMPTY = 0,
	SLOT_PENDING,
	SLOT_LOADING, /* captured by an in-flight Poll job */
	SLOT_READY,
	SLOT_FAILED
};

typedef struct {
	u32 generation; /* advances exactly once per new ownership */
	u8 state;
	u8 pinCount;
	u8 distance;
	s16 recordPos; /* index into the pack index; -1 when empty */
	u32 quarantineUntil; /* nowMs() stamp before which texels may not be rewritten */
	u8 *data; /* fixed arena region */
	GXTexObj tex;
} uiAssetSlot_t;

static struct {
	bool arenaReady;   /* arena allocated (survives CancelForDeviceChange) */
	bool packOpen;     /* header+index validated, source readable */
	bool syncBound;    /* exact callback/context triple bound until disposal */
	uiAssetsSource_t src;
	uiAssetsSync_t sync;
	u32 (*nowMs)(void);
	u8 *arena;         /* UI_ASSETS_WINDOW * UI_ASSETS_POSTER_BYTES, 32-aligned */
	u8 *index;         /* recordCount * PAK_RECORD_SIZE raw big-endian records */
	u32 recordCount;
	u32 dataOffset;
	u32 fileLength;
	uiAssetSlot_t slots[UI_ASSETS_WINDOW];
} ua;

/* Menu-thread-only critical-section entry/exit. Video APIs never call
 * these: their caller already owns the lock. */
static void syncLock(void) {
	if (ua.sync.lock)
		ua.sync.lock(ua.sync.ctx);
}

static void syncUnlock(void) {
	if (ua.sync.unlock)
		ua.sync.unlock(ua.sync.ctx);
}

/* Init binds one exact critical-section identity for the cache lifetime.
 * Host-only, single-threaded tests may bind the all-zero identity; target
 * builds require a complete callback pair. A half-pair is never valid. */
static s32 validateSync(const uiAssetsSync_t *sync,
                        uiAssetsSync_t *requested) {
	memset(requested, 0, sizeof(*requested));
	if (sync)
		*requested = *sync;
	if ((requested->lock == NULL) != (requested->unlock == NULL))
		return UI_ASSETS_ERR_STATE;
#if !defined(UI_ASSETS_HOST_BUILD) || defined(UI_ASSETS_HOST_REQUIRE_SYNC)
	if (!requested->lock)
		return UI_ASSETS_ERR_STATE;
#endif
	if (ua.syncBound &&
	    (ua.sync.lock != requested->lock ||
	     ua.sync.unlock != requested->unlock ||
	     ua.sync.ctx != requested->ctx))
		return UI_ASSETS_ERR_STATE;
	return UI_ASSETS_OK;
}

static u32 be32(const u8 *p) {
	return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static u16 be16(const u8 *p) {
	return (u16)((p[0] << 8) | p[1]);
}

static const u8 *record(const u8 *index, u32 pos) {
	return index + (size_t)pos * PAK_RECORD_SIZE;
}

static bool idCharsValid(const char *id) {
	int i;
	for (i = 0; i < UI_ASSETS_ID_LEN; i++) {
		char c = id[i];
		if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
			return false;
	}
	return true;
}

#ifndef UI_ASSETS_HOST_BUILD
static u32 defaultNowMs(void) {
	return (u32)ticks_to_millisecs(gettime());
}
#endif

/* Binary search for an exact 6-char ID; -1 when absent. */
static s32 findExact(const char *id) {
	s32 lo = 0, hi = (s32)ua.recordCount - 1;
	while (lo <= hi) {
		s32 mid = lo + (hi - lo) / 2;
		int cmp = memcmp(record(ua.index, mid), id, UI_ASSETS_ID_LEN);
		if (cmp == 0)
			return mid;
		if (cmp < 0)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return -1;
}

/* First record whose ID is >= the 4-char prefix; recordCount when none. */
static s32 lowerBound4(const char *id) {
	s32 lo = 0, hi = (s32)ua.recordCount;
	while (lo < hi) {
		s32 mid = lo + (hi - lo) / 2;
		if (memcmp(record(ua.index, mid), id, 4) < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	return lo;
}

/*
 * Exact 6-char lookup first; the 4-char fallback applies ONLY to records
 * the pack explicitly marks universal. Returns the record position or -1;
 * *universal reports which path matched. Caller holds the lock (or is the
 * menu thread inside its own locked section).
 */
static s32 resolveRecord(const char *id, bool *universal) {
	s32 pos = findExact(id);
	*universal = false;
	if (pos >= 0)
		return pos;
	for (pos = lowerBound4(id);
	     pos < (s32)ua.recordCount &&
	     memcmp(record(ua.index, pos), id, 4) == 0; pos++) {
		if (record(ua.index, pos)[6] & PAK_FLAG_UNIVERSAL) {
			*universal = true;
			return pos;
		}
	}
	return -1;
}

static uiAssetSlot_t *slotForRecord(s32 recordPos) {
	int i;
	for (i = 0; i < UI_ASSETS_WINDOW; i++) {
		if (ua.slots[i].state != SLOT_EMPTY && ua.slots[i].recordPos == recordPos)
			return &ua.slots[i];
	}
	return NULL;
}

/* Stale handles (evicted, cancelled, shut down) resolve to NULL here. */
static uiAssetSlot_t *slotFromHandle(uiPosterHandle_t handle) {
	uiAssetSlot_t *slot;
	if (handle.slot >= UI_ASSETS_WINDOW)
		return NULL;
	slot = &ua.slots[handle.slot];
	if (slot->generation != handle.generation)
		return NULL;
	return slot;
}

/* Inside the critical section. Handles die via the EMPTY state; the
 * generation advances when the slot is next ASSIGNED, keeping exactly one
 * increment per ownership. */
static void evictSlot(uiAssetSlot_t *slot) {
	slot->state = SLOT_EMPTY;
	slot->pinCount = 0;
	slot->recordPos = -1;
	slot->quarantineUntil = ua.nowMs ? ua.nowMs() + UI_ASSETS_EVICT_QUARANTINE_MS : 0;
}

static void invalidateAll(void) {
	int i;
	for (i = 0; i < UI_ASSETS_WINDOW; i++)
		evictSlot(&ua.slots[i]);
}

/*
 * Validate a staged header + index completely; nothing global is touched.
 * Version 1 packs are canonical: the generator emits one exact layout, so
 * validation enforces equality rather than tolerating variants.
 */
static s32 validateHeaderAndIndex(const u8 *header, const u8 *index,
                                  u32 srcSize, u32 recordCount,
                                  u32 dataOffset, u32 fileLength) {
	u32 indexLength = recordCount * PAK_RECORD_SIZE;
	u32 crcStored, crcComputed;
	u8 headerCopy[PAK_HEADER_SIZE];
	const u8 *lastUniversal = NULL;
	u32 i;

	(void)srcSize;
	crcStored = be32(header + 0x08);
	memcpy(headerCopy, header, PAK_HEADER_SIZE);
	memset(headerCopy + 0x08, 0, 4);
	crcComputed = crc32(0, headerCopy, PAK_HEADER_SIZE);
	crcComputed = crc32(crcComputed, index, indexLength);
	if (crcComputed != crcStored)
		return UI_ASSETS_ERR_CRC;

	for (i = 0; i < recordCount; i++) {
		const u8 *rec = record(index, i);
		if (!idCharsValid((const char *)rec))
			return UI_ASSETS_ERR_FORMAT;
		if (i > 0 && memcmp(record(index, i - 1), rec, UI_ASSETS_ID_LEN) >= 0)
			return UI_ASSETS_ERR_FORMAT; /* unsorted or duplicate */
		if ((rec[6] & ~PAK_FLAGS_KNOWN) != 0 || rec[7] != 0)
			return UI_ASSETS_ERR_FORMAT;
		if (be32(rec + 0x08) != dataOffset + i * (u32)UI_ASSETS_POSTER_BYTES)
			return UI_ASSETS_ERR_FORMAT; /* overlap/gap/out-of-range */
		if (be32(rec + 0x0C) != UI_ASSETS_POSTER_BYTES)
			return UI_ASSETS_ERR_FORMAT;
		if (be32(rec + 0x1C) != 0)
			return UI_ASSETS_ERR_FORMAT;
		/* An ambiguous universal fallback (two universal records sharing a
		 * 4-char prefix) is a generator error; reject crafted packs too.
		 * Records are sorted, so same-prefix records are contiguous and
		 * comparing against the last universal record suffices. */
		if (rec[6] & PAK_FLAG_UNIVERSAL) {
			if (lastUniversal && memcmp(lastUniversal, rec, 4) == 0)
				return UI_ASSETS_ERR_FORMAT;
			lastUniversal = rec;
		}
	}
	(void)fileLength;
	return UI_ASSETS_OK;
}

/* Header field validation into locals; no global state touched. */
static s32 validateHeader(const u8 *header, u32 srcSize, u32 *outCount,
                          u32 *outDataOffset, u32 *outFileLength) {
	u32 recordCount, indexLength, dataOffset, fileLength;
	u64 expectedLength;
	u32 i;

	if (be32(header + 0x00) != PAK_MAGIC || be32(header + 0x04) != PAK_VERSION)
		return UI_ASSETS_ERR_FORMAT;
	recordCount = be32(header + 0x0C);
	if (recordCount < 1 || recordCount > UI_ASSETS_MAX_RECORDS)
		return UI_ASSETS_ERR_FORMAT;
	indexLength = recordCount * PAK_RECORD_SIZE;
	dataOffset = be32(header + 0x18);
	fileLength = be32(header + 0x1C);
	expectedLength = (u64)dataOffset + (u64)recordCount * UI_ASSETS_POSTER_BYTES;
	if (be32(header + 0x10) != PAK_HEADER_SIZE ||
	    be32(header + 0x14) != indexLength ||
	    dataOffset != PAK_HEADER_SIZE + indexLength ||
	    fileLength != expectedLength ||
	    fileLength != srcSize ||
	    be32(header + 0x20) != UI_ASSETS_POSTER_BYTES ||
	    be16(header + 0x24) != UI_ASSETS_CANVAS_W ||
	    be16(header + 0x26) != UI_ASSETS_CANVAS_H ||
	    be16(header + 0x28) != UI_ASSETS_CONTENT_W ||
	    be16(header + 0x2A) != UI_ASSETS_CONTENT_H ||
	    header[0x2C] != UI_ASSETS_MIP_LEVELS ||
	    header[0x2D] != GX_TF_CMPR ||
	    be16(header + 0x2E) != 0)
		return UI_ASSETS_ERR_FORMAT;
	for (i = 0x30; i < PAK_HEADER_SIZE; i++) {
		if (header[i] != 0)
			return UI_ASSETS_ERR_FORMAT;
	}
	*outCount = recordCount;
	*outDataOffset = dataOffset;
	*outFileLength = fileLength;
	return UI_ASSETS_OK;
}

s32 UIAssets_Init(const uiAssetsSource_t *source, const uiAssetsSync_t *sync) {
	u8 header[PAK_HEADER_SIZE];
	u8 *newIndex = NULL;
	u8 *oldIndex;
	uiAssetsSync_t requestedSync;
	u32 recordCount, dataOffset, fileLength, indexLength;
	u32 (*nowMs)(void);
	s32 err;
	int i;

	err = validateSync(sync, &requestedSync);
	if (err != UI_ASSETS_OK)
		return err;
	if (!source || !source->read)
		return UI_ASSETS_ERR_STATE;
	if (ua.packOpen) /* menu-thread state, menu-thread caller: race-free */
		return UI_ASSETS_ERR_STATE;

#ifdef UI_ASSETS_HOST_BUILD
	if (!source->nowMs)
		return UI_ASSETS_ERR_STATE; /* host tests must inject a clock */
	nowMs = source->nowMs;
#else
	nowMs = source->nowMs ? source->nowMs : defaultNowMs;
#endif

	/* Stage + validate everything in private memory; no publication and
	 * no lock until the pack is proven good. */
	if (source->size < PAK_HEADER_SIZE)
		return UI_ASSETS_ERR_FORMAT;
	if (source->read(source->ctx, 0, header, PAK_HEADER_SIZE) != PAK_HEADER_SIZE)
		return UI_ASSETS_ERR_IO;
	err = validateHeader(header, source->size, &recordCount, &dataOffset,
	                     &fileLength);
	if (err != UI_ASSETS_OK)
		return err;
	indexLength = recordCount * PAK_RECORD_SIZE;
	newIndex = UIA_ALLOC(indexLength);
	if (!newIndex)
		return UI_ASSETS_ERR_NOMEM;
	if (source->read(source->ctx, PAK_HEADER_SIZE, newIndex, indexLength) !=
	    (s32)indexLength) {
		err = UI_ASSETS_ERR_IO;
		goto fail;
	}
	err = validateHeaderAndIndex(header, newIndex, source->size, recordCount,
	                             dataOffset, fileLength);
	if (err != UI_ASSETS_OK)
		goto fail;

	if (!ua.arenaReady) {
		ua.arena = UIA_ALLOC_ALIGNED32(
			(u32)UI_ASSETS_WINDOW * UI_ASSETS_POSTER_BYTES);
		if (!ua.arena) {
			err = UI_ASSETS_ERR_NOMEM;
			goto fail;
		}
	}

	/* Bind once, immediately before the first publication. Cancel keeps the
	 * binding alive, so every re-Init must present the identical callbacks
	 * and context. Only DisposeAfterVideoStop clears it. */
	if (!ua.syncBound) {
		ua.sync = requestedSync;
		ua.syncBound = true;
	}

	syncLock();
	ua.src = *source;
	ua.nowMs = nowMs;
	if (!ua.arenaReady) {
		for (i = 0; i < UI_ASSETS_WINDOW; i++)
			ua.slots[i].data =
				ua.arena + (size_t)i * UI_ASSETS_POSTER_BYTES;
		ua.arenaReady = true;
	}
	oldIndex = ua.index;
	ua.index = newIndex;
	ua.recordCount = recordCount;
	ua.dataOffset = dataOffset;
	ua.fileLength = fileLength;
	invalidateAll(); /* new pack: every previous handle goes stale */
	ua.packOpen = true;
	syncUnlock();

	if (oldIndex)
		UIA_FREE(oldIndex); /* detached above; freed outside the lock */
	return UI_ASSETS_OK;

fail:
	UIA_FREE(newIndex);
	return err;
}

bool UIAssets_Ready(void) {
	return ua.packOpen;
}

void UIAssets_RequestWindow(const char (*ids)[8], int count, int selected) {
	struct {
		s32 recordPos;
		u8 distance;
	} want[UI_ASSETS_WINDOW];
	int wantCount = 0;
	int i, j;

	if (!ids)
		return;
	if (count < 0)
		count = 0;
	if (count > UI_ASSETS_WINDOW)
		count = UI_ASSETS_WINDOW;
	if (selected < 0)
		selected = 0;
	if (selected >= count && count > 0)
		selected = count - 1;

	syncLock();
	if (!ua.packOpen) {
		syncUnlock();
		return;
	}

	/* Resolve requested IDs to unique pack records, keeping the smallest
	 * distance when two window entries share one universal record. */
	for (i = 0; i < count; i++) {
		bool universal;
		s32 pos;
		u8 distance = (u8)(i >= selected ? i - selected : selected - i);
		if (!idCharsValid(ids[i]))
			continue;
		pos = resolveRecord(ids[i], &universal);
		if (pos < 0)
			continue;
		for (j = 0; j < wantCount; j++) {
			if (want[j].recordPos == pos) {
				if (distance < want[j].distance)
					want[j].distance = distance;
				break;
			}
		}
		if (j == wantCount) {
			want[wantCount].recordPos = pos;
			want[wantCount].distance = distance;
			wantCount++;
		}
	}

	/* Keep slots already holding wanted records (including FAILED ones:
	 * a corrupt poster stays corrupt until the pack changes, so don't
	 * retry-loop); evict unpinned slots that fell out of the window. */
	for (i = 0; i < UI_ASSETS_WINDOW; i++) {
		uiAssetSlot_t *slot = &ua.slots[i];
		bool wanted = false;
		if (slot->state == SLOT_EMPTY)
			continue;
		for (j = 0; j < wantCount; j++) {
			if (want[j].recordPos == slot->recordPos) {
				slot->distance = want[j].distance;
				want[j].recordPos = -1; /* satisfied */
				wanted = true;
				break;
			}
		}
		if (!wanted && slot->pinCount == 0)
			evictSlot(slot);
	}

	/* Assign the remaining wanted records to free slots, nearest first.
	 * Pinned out-of-window slots shrink capacity; the farthest requests
	 * simply wait for a later window. Assignment is the single point
	 * where a slot's generation advances. */
	for (;;) {
		int best = -1;
		uiAssetSlot_t *freeSlot = NULL;
		for (j = 0; j < wantCount; j++) {
			if (want[j].recordPos >= 0 &&
			    (best < 0 || want[j].distance < want[best].distance))
				best = j;
		}
		if (best < 0)
			break;
		for (i = 0; i < UI_ASSETS_WINDOW; i++) {
			if (ua.slots[i].state == SLOT_EMPTY) {
				freeSlot = &ua.slots[i];
				break;
			}
		}
		if (!freeSlot)
			break;
		freeSlot->generation++;
		freeSlot->recordPos = want[best].recordPos;
		freeSlot->distance = want[best].distance;
		freeSlot->pinCount = 0;
		freeSlot->state = SLOT_PENDING;
		want[best].recordPos = -1;
	}
	syncUnlock();
}

bool UIAssets_Poll(void) {
	struct {
		int slotIdx;
		u32 generation;
		s16 recordPos;
		u32 dataOffset;
		u32 expectedCrc;
		u8 *data;
		s32 (*read)(void *ctx, u32 offset, void *dst, u32 len);
		void *readCtx;
		u32 fileLength;
	} job;
	uiAssetSlot_t *slot = NULL;
	bool pending = false;
	bool loadOk;
	u32 now;
	int i;

	/* Phase 1 (locked): pick and capture the nearest pending job. */
	syncLock();
	if (!ua.packOpen || !ua.arenaReady) {
		syncUnlock();
		return false;
	}
	now = ua.nowMs();
	for (i = 0; i < UI_ASSETS_WINDOW; i++) {
		uiAssetSlot_t *candidate = &ua.slots[i];
		if (candidate->state != SLOT_PENDING)
			continue;
		pending = true;
		if ((s32)(now - candidate->quarantineUntil) < 0)
			continue; /* GPU may still sample the old texels */
		if (!slot || candidate->distance < slot->distance)
			slot = candidate;
	}
	if (!slot) {
		syncUnlock();
		return pending;
	}
	{
		const u8 *rec = record(ua.index, (u32)slot->recordPos);
		slot->state = SLOT_LOADING;
		job.slotIdx = (int)(slot - ua.slots);
		job.generation = slot->generation;
		job.recordPos = slot->recordPos;
		job.dataOffset = be32(rec + 0x08);
		job.expectedCrc = be32(rec + 0x10);
		job.data = slot->data;
		job.read = ua.src.read;
		job.readCtx = ua.src.ctx;
		job.fileLength = ua.fileLength;
	}
	syncUnlock();

	/* Phase 2 (UNLOCKED): the bounded device read, CRC, and cache flush.
	 * Writing into the captured arena region is single-writer: Poll runs
	 * on the menu thread only, and the GPU is fenced by the quarantine. */
	loadOk = job.read != NULL &&
	         (u64)job.dataOffset + UI_ASSETS_POSTER_BYTES <= job.fileLength &&
	         job.read(job.readCtx, job.dataOffset, job.data,
	                  UI_ASSETS_POSTER_BYTES) == UI_ASSETS_POSTER_BYTES &&
	         crc32(0, job.data, UI_ASSETS_POSTER_BYTES) == job.expectedCrc;
	if (loadOk)
		DCFlushRange(job.data, UI_ASSETS_POSTER_BYTES);

	/* Phase 3 (locked): revalidate ownership, then publish. A slot that
	 * was cancelled, re-inited, evicted, or reassigned while we were
	 * reading has a different generation/state and the stale job is
	 * dropped without publishing anything. */
	syncLock();
	slot = &ua.slots[job.slotIdx];
	if (ua.packOpen && slot->state == SLOT_LOADING &&
	    slot->generation == job.generation &&
	    slot->recordPos == job.recordPos) {
		if (loadOk) {
			GX_InitTexObj(&slot->tex, slot->data, UI_ASSETS_CANVAS_W,
			              UI_ASSETS_CANVAS_H, GX_TF_CMPR, GX_CLAMP,
			              GX_CLAMP, GX_TRUE);
			GX_InitTexObjLOD(&slot->tex, GX_LIN_MIP_LIN, GX_LINEAR, 0.0f,
			                 (float)(UI_ASSETS_MIP_LEVELS - 1), 0.0f,
			                 GX_FALSE, GX_TRUE, GX_ANISO_1);
			slot->state = SLOT_READY;
		} else {
			slot->state = SLOT_FAILED;
		}
	}
	pending = false;
	for (i = 0; i < UI_ASSETS_WINDOW; i++) {
		if (ua.slots[i].state == SLOT_PENDING) {
			pending = true;
			break;
		}
	}
	syncUnlock();
	return pending;
}

uiPosterResult_t UIAssets_Query(const char *gameId, size_t gameIdLen,
                                bool bnrAvailable, uiPosterHandle_t *out) {
	uiPosterResult_t base;
	uiAssetSlot_t *slot;
	bool universal;
	s32 pos;

	if (out) {
		out->slot = 0xFFFF;
		out->generation = 0;
		out->reserved = 0;
	}
	if (!gameId || gameIdLen < UI_ASSETS_ID_LEN || !idCharsValid(gameId) ||
	    !ua.packOpen)
		return bnrAvailable ? UI_POSTER_USE_BNR : UI_POSTER_PROCEDURAL_CARD;

	pos = resolveRecord(gameId, &universal);
	if (pos < 0)
		return bnrAvailable ? UI_POSTER_USE_BNR : UI_POSTER_PROCEDURAL_CARD;
	base = universal ? UI_POSTER_UNIVERSAL : UI_POSTER_EXACT;

	slot = slotForRecord(pos);
	if (slot) {
		if (slot->state == SLOT_FAILED)
			return UI_POSTER_CORRUPT_OR_UNAVAILABLE;
		if (out) {
			out->slot = (u16)(slot - ua.slots);
			out->generation = slot->generation;
		}
	}
	/* No slot yet: still EXACT/UNIVERSAL -- the caller renders its BNR/
	 * procedural stand-in until RequestWindow + Poll land the texels. */
	return base;
}

GXTexObj *UIAssets_Peek(uiPosterHandle_t handle) {
	uiAssetSlot_t *slot = slotFromHandle(handle);
	if (!slot || slot->state != SLOT_READY)
		return NULL;
	return &slot->tex;
}

GXTexObj *UIAssets_Acquire(uiPosterHandle_t handle) {
	uiAssetSlot_t *slot;
	GXTexObj *tex = NULL;
	syncLock();
	slot = slotFromHandle(handle);
	if (slot && slot->state == SLOT_READY && slot->pinCount < 0xFF) {
		/* Refuse rather than saturate: an Acquire that returned a texture
		 * without recording its pin would let paired Releases drive the
		 * count to zero while a holder still retains the pointer. */
		slot->pinCount++;
		tex = &slot->tex;
	}
	syncUnlock();
	return tex;
}

void UIAssets_Release(uiPosterHandle_t handle) {
	uiAssetSlot_t *slot;
	syncLock();
	slot = slotFromHandle(handle);
	if (slot && slot->pinCount > 0)
		slot->pinCount--;
	syncUnlock();
}

bool UIAssets_DominantColor(const char *gameId, size_t gameIdLen,
                            u8 *r, u8 *g, u8 *b) {
	const u8 *rec;
	bool universal;
	s32 pos;

	if (!gameId || gameIdLen < UI_ASSETS_ID_LEN || !idCharsValid(gameId) ||
	    !ua.packOpen)
		return false;
	pos = resolveRecord(gameId, &universal);
	if (pos < 0)
		return false;
	rec = record(ua.index, (u32)pos);
	if (r)
		*r = rec[0x14];
	if (g)
		*g = rec[0x15];
	if (b)
		*b = rec[0x16];
	return true;
}

void UIAssets_CancelForDeviceChange(void) {
	u8 *oldIndex;
	if (!ua.arenaReady && !ua.packOpen)
		return;
	syncLock();
	invalidateAll();
	ua.packOpen = false;
	ua.recordCount = 0;
	oldIndex = ua.index;
	ua.index = NULL;
	memset(&ua.src, 0, sizeof(ua.src));
	syncUnlock();
	/* Unpublished inside the critical section above: no locked reader can
	 * still see it. Freed outside so the lock never covers free(). */
	if (oldIndex)
		UIA_FREE(oldIndex);
	/* Arena and nowMs stay: in-flight frames may still sample resident
	 * (stale but intact) texels, and re-Init reuses the allocation. */
}

s32 UIAssets_DisposeAfterVideoStop(void) {
	u8 *oldArena;
	int i;

	/* Disposal is deliberately lock-free: its caller has already stopped
	 * the video thread and may already have destroyed the video mutex. The
	 * live-mutex Cancel step must have unpublished and freed the index. */
	if (ua.packOpen || ua.index != NULL)
		return UI_ASSETS_ERR_STATE;
	oldArena = ua.arena;
	ua.arena = NULL;
	ua.arenaReady = false;
	for (i = 0; i < UI_ASSETS_WINDOW; i++) {
		ua.slots[i].state = SLOT_EMPTY;
		ua.slots[i].pinCount = 0;
		ua.slots[i].distance = 0;
		ua.slots[i].recordPos = -1;
		ua.slots[i].quarantineUntil = 0;
		ua.slots[i].data = NULL;
		memset(&ua.slots[i].tex, 0, sizeof(ua.slots[i].tex));
	}
	memset(&ua.src, 0, sizeof(ua.src));
	ua.recordCount = 0;
	ua.dataOffset = 0;
	ua.fileLength = 0;
	ua.nowMs = NULL;
	memset(&ua.sync, 0, sizeof(ua.sync));
	ua.syncBound = false;
	if (oldArena)
		UIA_FREE(oldArena);
	return UI_ASSETS_OK;
}

u32 UIAssets_MemoryFootprint(void) {
	u32 total = 0;
	if (ua.arenaReady)
		total += (u32)UI_ASSETS_WINDOW * UI_ASSETS_POSTER_BYTES;
	if (ua.index)
		total += ua.recordCount * PAK_RECORD_SIZE;
	return total;
}

#ifdef UI_ASSETS_HOST_BUILD

void UIAssetsTest_ForceGeneration(u16 slot, u32 generation) {
	if (slot < UI_ASSETS_WINDOW)
		ua.slots[slot].generation = generation;
}

#else

static s32 fileHandleRead(void *ctx, u32 offset, void *dst, u32 len) {
	file_handle *file = (file_handle *)ctx;
	if (file->device->seekFile(file, offset, DEVICE_HANDLER_SEEK_SET) != offset)
		return UI_ASSETS_ERR_IO;
	return file->device->readFile(file, dst, len);
}

s32 UIAssets_SourceFromFileHandle(file_handle *file, uiAssetsSource_t *out) {
	if (!file || !file->device || !file->device->readFile ||
	    !file->device->seekFile || !out)
		return UI_ASSETS_ERR_STATE;
	if (!file->size && file->device->statFile &&
	    file->device->statFile(file) != 0)
		return UI_ASSETS_ERR_IO;
	if (!file->size)
		return UI_ASSETS_ERR_FORMAT;
	out->read = fileHandleRead;
	out->size = file->size;
	out->ctx = file;
	out->nowMs = NULL;
	return UI_ASSETS_OK;
}

static void syncMutexLock(void *ctx) {
	LWP_MutexLock((mutex_t)(uintptr_t)ctx);
}

static void syncMutexUnlock(void *ctx) {
	LWP_MutexUnlock((mutex_t)(uintptr_t)ctx);
}

void UIAssets_SyncFromMutex(mutex_t mutex, uiAssetsSync_t *out) {
	out->lock = syncMutexLock;
	out->unlock = syncMutexUnlock;
	out->ctx = (void *)(uintptr_t)mutex;
}

#endif
