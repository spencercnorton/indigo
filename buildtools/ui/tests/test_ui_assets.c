/*
 * test_ui_assets.c -- host test harness for the Phase 4A poster cache.
 *
 * Builds ui_assets.c with -DUI_ASSETS_HOST_BUILD (GX stubbed, allocation
 * counted, clock injected) and drives it against in-memory packs built by
 * the same layout rules as buildtools/ui/poster_pack.py. Poster payloads
 * are deterministic per-ID noise: the runtime never decodes CMPR, so tests
 * only need byte-exact transport, not real textures. One optional argv[1]
 * points at a real generator-produced .pak for an end-to-end pass.
 *
 * Usage: ./test_ui_assets [real-pack.pak]
 */

#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "ui_assets.h"

/* ---- tiny assert harness ---- */

static int failures;
static int checks;
static const char *currentTest;

#define CHECK(cond) do { \
	checks++; \
	if (!(cond)) { \
		failures++; \
		fprintf(stderr, "FAIL %s:%d [%s] %s\n", __FILE__, __LINE__, \
		        currentTest, #cond); \
	} \
} while (0)

#define RUN(fn) do { \
	currentTest = #fn; \
	resetWorld(); \
	fn(); \
	printf("  %-44s %s\n", #fn, failures == failuresBefore ? "ok" : "FAILED"); \
	failuresBefore = failures; \
} while (0)

/* ---- fake external critical section with invariant checking ----
 *
 * Non-blocking (single-threaded harness), but it enforces the contract
 * mechanically: the lock is never taken recursively, never released
 * unheld, source reads and heap traffic never happen inside it, and every
 * test ends balanced. Tests additionally count acquisitions to prove the
 * Poll capture/publish split. */

static int lockDepth;
static int lockCalls;

/* Boundary probe: the observable module state (a probe ID's classification
 * plus its Peek pointer) is sampled at every unlock and compared at the
 * NEXT lock. Any menu-side mutation of video-visible state that happens
 * OUTSIDE a critical section changes the probe between those two points
 * and fails here -- this falsifies "mutation outside the lock", not just
 * section counts. Disabled around test seams that mutate state directly. */
static int probeEnabled = 1;
static int probeArmed;
static char probeId[8] = "GALE01";

typedef struct {
	int result;
	const GXTexObj *tex;
	int ready;
} probeSnap_t;

static probeSnap_t probeLastUnlock;

static probeSnap_t probeSample(void) {
	probeSnap_t snap;
	uiPosterHandle_t h;
	snap.result = (int)UIAssets_Query(probeId, 6, true, &h);
	snap.tex = UIAssets_Peek(h);
	snap.ready = UIAssets_Ready() ? 1 : 0;
	return snap;
}

static void fakeLock(void *ctx) {
	(void)ctx;
	if (lockDepth != 0) {
		fprintf(stderr, "FAIL [%s] recursive lock (depth %d)\n",
		        currentTest, lockDepth);
		failures++;
	}
	if (probeEnabled && probeArmed) {
		probeSnap_t now = probeSample();
		if (now.result != probeLastUnlock.result ||
		    now.tex != probeLastUnlock.tex ||
		    now.ready != probeLastUnlock.ready) {
			fprintf(stderr,
			        "FAIL [%s] observable state mutated outside the "
			        "critical section (probe %s)\n", currentTest, probeId);
			failures++;
		}
	}
	probeArmed = 0;
	lockDepth++;
	lockCalls++;
}

static void fakeUnlock(void *ctx) {
	(void)ctx;
	if (lockDepth != 1) {
		fprintf(stderr, "FAIL [%s] unlock at depth %d\n",
		        currentTest, lockDepth);
		failures++;
	}
	lockDepth--;
	if (probeEnabled) {
		probeLastUnlock = probeSample();
		probeArmed = 1;
	}
}

static const uiAssetsSync_t testSync = { fakeLock, fakeUnlock, NULL };

typedef struct {
	int destroyed;
	int lockCalls;
	int unlockCalls;
} trackedMutex_t;

static void trackedLock(void *ctx) {
	trackedMutex_t *mutex = (trackedMutex_t *)ctx;
	if (!mutex || mutex->destroyed) {
		fprintf(stderr, "FAIL [%s] lock callback used after destruction\n",
		        currentTest);
		failures++;
	}
	if (mutex)
		mutex->lockCalls++;
	fakeLock(NULL);
}

static void trackedUnlock(void *ctx) {
	trackedMutex_t *mutex = (trackedMutex_t *)ctx;
	if (!mutex || mutex->destroyed) {
		fprintf(stderr, "FAIL [%s] unlock callback used after destruction\n",
		        currentTest);
		failures++;
	}
	if (mutex)
		mutex->unlockCalls++;
	fakeUnlock(NULL);
}

/* A second complete pair used only to prove callback identity is part of
 * the lifetime binding. These delegate to the same fake mutex mechanics. */
static void alternateLock(void *ctx) {
	fakeLock(ctx);
}

static void alternateUnlock(void *ctx) {
	fakeUnlock(ctx);
}

/* ---- allocation hooks required by the host build ---- */

static int liveAllocs;
static long liveBytes;
static int allocCalls;
static int allocsFrozen; /* set outside Init to prove nothing allocates */

typedef struct {
	size_t size;
} allocHeader_t;

static void assertUnlockedHeap(const char *what) {
	if (lockDepth != 0) {
		fprintf(stderr, "FAIL [%s] %s inside the critical section\n",
		        currentTest, what);
		failures++;
	}
}

void *uiAssetsHostAlloc(u32 size) {
	allocHeader_t *h;
	assertUnlockedHeap("allocation");
	if (allocsFrozen) {
		fprintf(stderr, "FAIL [%s] allocation outside Init (%u bytes)\n",
		        currentTest, (unsigned)size);
		failures++;
	}
	h = malloc(sizeof(*h) + size);
	if (!h)
		return NULL;
	h->size = size;
	liveAllocs++;
	allocCalls++;
	liveBytes += (long)size;
	return h + 1;
}

static void *alignedBlocks[16];
static long alignedSizes[16];
static int alignedCount;

void *uiAssetsHostAllocAligned32(u32 size) {
	void *p = NULL;
	assertUnlockedHeap("aligned allocation");
	if (allocsFrozen) {
		fprintf(stderr, "FAIL [%s] aligned allocation outside Init\n",
		        currentTest);
		failures++;
	}
	/* Track aligned blocks in a side table: posix_memalign leaves no room
	 * for a size header without breaking the 32-byte alignment. */
	if (alignedCount >= 16 || posix_memalign(&p, 32, size) != 0)
		return NULL;
	alignedBlocks[alignedCount] = p;
	alignedSizes[alignedCount] = (long)size;
	alignedCount++;
	liveAllocs++;
	allocCalls++;
	liveBytes += (long)size;
	return p;
}

static int flushCalls;

void uiAssetsHostFlush(void *ptr, u32 len) {
	(void)ptr;
	if (lockDepth != 0) {
		fprintf(stderr, "FAIL [%s] DCFlushRange inside the critical "
		        "section\n", currentTest);
		failures++;
	}
	if (len != UI_ASSETS_POSTER_BYTES) {
		fprintf(stderr, "FAIL [%s] flush of %u bytes\n", currentTest,
		        (unsigned)len);
		failures++;
	}
	flushCalls++;
}

void uiAssetsHostFree(void *ptr) {
	int i;
	assertUnlockedHeap("free");
	if (!ptr)
		return;
	for (i = 0; i < alignedCount; i++) {
		if (alignedBlocks[i] == ptr) {
			liveAllocs--;
			liveBytes -= alignedSizes[i];
			alignedBlocks[i] = NULL;
			free(ptr);
			return;
		}
	}
	{
		allocHeader_t *h = (allocHeader_t *)ptr - 1;
		liveAllocs--;
		liveBytes -= (long)h->size;
		free(h);
	}
}

/* ---- fake clock ---- */

static u32 fakeNowValue = 1000;

static u32 fakeNow(void) {
	return fakeNowValue;
}

/* ---- pack builder (mirrors poster_pack.py layout) ---- */

#define HDR 64
#define REC 32

typedef struct {
	const char *id;
	int universal;
	u8 dom[3];
} testRec_t;

static u32 wbe32(u8 *p, u32 v) {
	p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v;
	return v;
}

static void wbe16(u8 *p, u16 v) {
	p[0] = (u8)(v >> 8); p[1] = (u8)v;
}

static u32 rbe32(const u8 *p) {
	return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

/* Deterministic per-ID payload so slot contents are verifiable. */
static void fillPayload(u8 *dst, const char *id) {
	u32 x = 2166136261u;
	u32 i;
	for (i = 0; i < UI_ASSETS_ID_LEN; i++)
		x = (x ^ (u8)id[i]) * 16777619u;
	for (i = 0; i < UI_ASSETS_POSTER_BYTES; i++) {
		x ^= x << 13; x ^= x >> 17; x ^= x << 5;
		dst[i] = (u8)x;
	}
}

/* count is passed explicitly: corruption tests mutate the stored count and
 * the fixer must still hash the real header+index extent. */
static void packFixCrc(u8 *pack, u32 count) {
	u32 crc;
	wbe32(pack + 0x08, 0);
	crc = crc32(0, pack, HDR + count * REC);
	wbe32(pack + 0x08, crc);
}

#define BUILD_NOSORT 1

static u8 *buildPack(const testRec_t *recs, int count, size_t *outLen, int flags) {
	u32 dataOffset = HDR + (u32)count * REC;
	size_t len = dataOffset + (size_t)count * UI_ASSETS_POSTER_BYTES;
	u8 *pack = calloc(1, len);
	int order[1100];
	int i, j;

	for (i = 0; i < count; i++)
		order[i] = i;
	if (!(flags & BUILD_NOSORT)) {
		for (i = 0; i < count; i++) {
			for (j = i + 1; j < count; j++) {
				if (strcmp(recs[order[j]].id, recs[order[i]].id) < 0) {
					int t = order[i]; order[i] = order[j]; order[j] = t;
				}
			}
		}
	}

	wbe32(pack + 0x00, 0x5357504Bu);
	wbe32(pack + 0x04, 1);
	wbe32(pack + 0x0C, (u32)count);
	wbe32(pack + 0x10, HDR);
	wbe32(pack + 0x14, (u32)count * REC);
	wbe32(pack + 0x18, dataOffset);
	wbe32(pack + 0x1C, (u32)len);
	wbe32(pack + 0x20, UI_ASSETS_POSTER_BYTES);
	wbe16(pack + 0x24, UI_ASSETS_CANVAS_W);
	wbe16(pack + 0x26, UI_ASSETS_CANVAS_H);
	wbe16(pack + 0x28, UI_ASSETS_CONTENT_W);
	wbe16(pack + 0x2A, UI_ASSETS_CONTENT_H);
	pack[0x2C] = UI_ASSETS_MIP_LEVELS;
	pack[0x2D] = 14; /* GX_TF_CMPR */

	for (i = 0; i < count; i++) {
		const testRec_t *r = &recs[order[i]];
		u8 *rec = pack + HDR + (size_t)i * REC;
		u8 *payload = pack + dataOffset + (size_t)i * UI_ASSETS_POSTER_BYTES;
		memcpy(rec, r->id, UI_ASSETS_ID_LEN);
		rec[6] = r->universal ? 0x01 : 0x00;
		wbe32(rec + 0x08, dataOffset + (u32)i * UI_ASSETS_POSTER_BYTES);
		wbe32(rec + 0x0C, UI_ASSETS_POSTER_BYTES);
		fillPayload(payload, r->id);
		wbe32(rec + 0x10, crc32(0, payload, UI_ASSETS_POSTER_BYTES));
		rec[0x14] = r->dom[0]; rec[0x15] = r->dom[1]; rec[0x16] = r->dom[2];
		rec[0x17] = 0xFF;
		wbe16(rec + 0x18, 0x8000);
		wbe16(rec + 0x1A, 0x8000);
	}
	packFixCrc(pack, (u32)count);
	*outLen = len;
	return pack;
}

/* ---- in-memory source with fault injection ---- */

static struct {
	const u8 *data;
	size_t len;
	int readCalls;
	int failAfter;   /* -1: never fail; else fail on the Nth call (0-based) */
	u32 shortReadAt; /* offset whose read returns len-1; 0xFFFFFFFF off */
} mem;

static void (*memReadHook)(void); /* optional reentrant chaos, runs pre-read */

static s32 memRead(void *ctx, u32 offset, void *dst, u32 len) {
	int call = mem.readCalls++;
	(void)ctx;
	if (lockDepth != 0) {
		fprintf(stderr, "FAIL [%s] source read under the lock (depth %d)\n",
		        currentTest, lockDepth);
		failures++;
	}
	if (memReadHook)
		memReadHook();
	if (mem.failAfter >= 0 && call >= mem.failAfter)
		return UI_ASSETS_ERR_IO;
	if ((size_t)offset + len > mem.len)
		return UI_ASSETS_ERR_IO;
	memcpy(dst, mem.data + offset, len);
	if (offset == mem.shortReadAt)
		return (s32)len - 1;
	return (s32)len;
}

static uiAssetsSource_t memSource(const u8 *data, size_t len) {
	uiAssetsSource_t src;
	mem.data = data;
	mem.len = len;
	mem.readCalls = 0;
	mem.failAfter = -1;
	mem.shortReadAt = 0xFFFFFFFFu;
	src.read = memRead;
	src.size = (u32)len;
	src.ctx = NULL;
	src.nowMs = fakeNow;
	return src;
}

static void resetWorld(void) {
	allocsFrozen = 0;
	memReadHook = NULL;
	UIAssets_CancelForDeviceChange();
	if (UIAssets_DisposeAfterVideoStop() != UI_ASSETS_OK) {
		fprintf(stderr, "FAIL [%s] reset could not dispose poster cache\n",
		        currentTest ? currentTest : "startup");
		failures++;
	}
	fakeNowValue = 1000;
	memset(&mem, 0, sizeof(mem));
	alignedCount = 0;
	if (lockDepth != 0) {
		fprintf(stderr, "FAIL [%s] lock left held across a test\n",
		        currentTest);
		failures++;
		lockDepth = 0;
	}
	lockCalls = 0;
	flushCalls = 0;
	probeEnabled = 1;
	probeArmed = 0;
	memcpy(probeId, "GALE01\0", 8);
}

/* ---- helpers ---- */

static void requestOne(const char *id) {
	char ids[1][8];
	memset(ids, 0, sizeof(ids));
	memcpy(ids[0], id, UI_ASSETS_ID_LEN);
	UIAssets_RequestWindow(ids, 1, 0);
}

static void pollAll(void) {
	int guard = 64;
	fakeNowValue += UI_ASSETS_EVICT_QUARANTINE_MS + 1;
	while (UIAssets_Poll() && guard-- > 0)
		fakeNowValue += UI_ASSETS_EVICT_QUARANTINE_MS + 1;
}

static const testRec_t BASIC[] = {
	{ "GALE01", 0, { 10, 20, 30 } },
	{ "GC6E01", 0, { 1, 2, 3 } },
	{ "GZLE01", 1, { 40, 50, 60 } },
};

/* ---- tests ---- */

static void test_init_valid(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	CHECK(UIAssets_Ready());
	CHECK(UIAssets_MemoryFootprint() ==
	      UI_ASSETS_SLOTS * UI_ASSETS_POSTER_BYTES + 3 * REC);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_ERR_STATE); /* double init */
	free(pack);
}

static void test_exact_hit(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	uiPosterHandle_t h;
	GXTexObj *tex;
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	requestOne("GALE01");
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_EXACT);
	CHECK(UIAssets_Peek(h) == NULL); /* not polled yet */
	pollAll();
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_EXACT);
	allocsFrozen = 1; /* per-frame path must not allocate */
	tex = UIAssets_Peek(h);
	allocsFrozen = 0;
	CHECK(tex != NULL);
	if (tex) {
		u8 expect[64];
		u8 full[UI_ASSETS_POSTER_BYTES];
		fillPayload(full, "GALE01");
		memcpy(expect, full, 64);
		CHECK(tex->width == UI_ASSETS_CANVAS_W);
		CHECK(tex->height == UI_ASSETS_CANVAS_H);
		CHECK(tex->format == 14);
		CHECK(tex->mipmap == 1);
		CHECK(tex->lodConfigured);
		CHECK(tex->minlod == 0.0f);
		CHECK(tex->maxlod == (float)(UI_ASSETS_MIP_LEVELS - 1));
		CHECK(memcmp(tex->data, expect, 64) == 0);
	}
	free(pack);
}

static void test_universal_fallback(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	uiPosterHandle_t h;
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	/* GZLE01 is universal: GZLE69 (different publisher) may use it. */
	requestOne("GZLE69");
	CHECK(UIAssets_Query("GZLE69", 6, true, &h) == UI_POSTER_UNIVERSAL);
	pollAll();
	CHECK(UIAssets_Query("GZLE69", 6, true, &h) == UI_POSTER_UNIVERSAL);
	CHECK(UIAssets_Peek(h) != NULL);
	/* The exact ID still classifies as EXACT and shares the slot. */
	CHECK(UIAssets_Query("GZLE01", 6, true, &h) == UI_POSTER_EXACT);
	CHECK(UIAssets_Peek(h) != NULL);
	free(pack);
}

static void test_no_implicit_fallback(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	uiPosterHandle_t h;
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	/* GALE01 is NOT universal: GALE69 must never borrow it. */
	CHECK(UIAssets_Query("GALE69", 6, true, &h) == UI_POSTER_USE_BNR);
	CHECK(UIAssets_Query("GALE69", 6, false, &h) == UI_POSTER_PROCEDURAL_CARD);
	requestOne("GALE69");
	pollAll();
	CHECK(UIAssets_Query("GALE69", 6, true, &h) == UI_POSTER_USE_BNR);
	CHECK(mem.readCalls == 2); /* header + index only, no poster read */
	free(pack);
}

static void test_duplicate_and_unsorted_rejected(void) {
	static const testRec_t DUP[] = {
		{ "GALE01", 0, { 0, 0, 0 } },
		{ "GALE01", 0, { 0, 0, 0 } },
	};
	static const testRec_t UNSORTED[] = {
		{ "GZLE01", 0, { 0, 0, 0 } },
		{ "GALE01", 0, { 0, 0, 0 } },
	};
	size_t len;
	u8 *pack = buildPack(DUP, 2, &len, BUILD_NOSORT);
	uiAssetsSource_t src = memSource(pack, len);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_ERR_FORMAT);
	CHECK(!UIAssets_Ready());
	free(pack);
	pack = buildPack(UNSORTED, 2, &len, BUILD_NOSORT);
	src = memSource(pack, len);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_ERR_FORMAT);
	free(pack);
}

static void test_ambiguous_universal_rejected(void) {
	/* Non-adjacent ambiguity: a non-universal record sits between two
	 * universal records sharing the GALE prefix. */
	static const testRec_t AMB[] = {
		{ "GALE01", 1, { 0, 0, 0 } },
		{ "GALE02", 0, { 0, 0, 0 } },
		{ "GALE03", 1, { 0, 0, 0 } },
	};
	size_t len;
	u8 *pack = buildPack(AMB, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_ERR_FORMAT);
	free(pack);
}

static void test_missing_or_truncated_pack(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src;

	src = memSource(pack, len);
	mem.failAfter = 0; /* header read fails: pack unreadable/missing */
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_ERR_IO);
	{
		uiPosterHandle_t h;
		CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_USE_BNR);
		CHECK(UIAssets_Query("GALE01", 6, false, &h) == UI_POSTER_PROCEDURAL_CARD);
	}

	src = memSource(pack, len - 1); /* truncated: fileLength mismatch */
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_ERR_FORMAT);

	src = memSource(pack, 32); /* shorter than a header */
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_ERR_FORMAT);
	free(pack);
}

static void test_header_field_corruption(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src;
	static const struct { u32 off; u32 val; } cases[] = {
		{ 0x00, 0x4B505753 }, /* wrong magic */
		{ 0x04, 2 },          /* wrong version */
		{ 0x0C, 0 },          /* zero records */
		{ 0x0C, 4096 },       /* over MAX_RECORDS */
		{ 0x0C, 0xFFFFFFFF }, /* count overflow attempt */
		{ 0x10, 128 },        /* bad index offset */
		{ 0x14, 1 },          /* bad index length */
		{ 0x18, 0xFFFFFFE0 }, /* data offset overflow attempt */
		{ 0x1C, 0xFFFFFFFF }, /* absurd file length */
		{ 0x20, 32768 },      /* wrong poster size */
		{ 0x24, 0x00C0 },     /* non-pow2 canvas smuggled in */
	};
	size_t i;
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		u8 *evil = malloc(len);
		memcpy(evil, pack, len);
		wbe32(evil + cases[i].off, cases[i].val);
		packFixCrc(evil, 3); /* re-hash the REAL extent so field checks run */
		src = memSource(evil, len);
		CHECK(UIAssets_Init(&src, &testSync) != UI_ASSETS_OK);
		CHECK(!UIAssets_Ready());
		free(evil);
	}
	free(pack);
}

static void test_malformed_record_offsets(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	u8 *evil;
	uiAssetsSource_t src;

	/* Record 1 overlaps record 0's payload. */
	evil = malloc(len);
	memcpy(evil, pack, len);
	wbe32(evil + HDR + REC + 0x08, rbe32(evil + HDR + 0x08) + 16);
	packFixCrc(evil, 3);
	src = memSource(evil, len);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_ERR_FORMAT);
	free(evil);

	/* Record points past EOF. */
	evil = malloc(len);
	memcpy(evil, pack, len);
	wbe32(evil + HDR + 0x08, (u32)len - 100);
	packFixCrc(evil, 3);
	src = memSource(evil, len);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_ERR_FORMAT);
	free(evil);

	/* Poster length overflow attempt. */
	evil = malloc(len);
	memcpy(evil, pack, len);
	wbe32(evil + HDR + 0x0C, 0xFFFFFF00);
	packFixCrc(evil, 3);
	src = memSource(evil, len);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_ERR_FORMAT);
	free(evil);
	free(pack);
}

static void test_field_validation_behind_valid_crc(void) {
	/* Every one of these mutations is re-CRC'd, so ONLY the field
	 * validators can reject it -- proving they aren't shadowed by the
	 * checksum (a fuzz flip always trips the CRC first). */
	static const struct { u32 off; u8 val; } cases[] = {
		{ 0x2C, 4 },            /* wrong mip count */
		{ 0x2D, 6 },            /* wrong texture format */
		{ 0x2E, 1 },            /* header reserved u16 */
		{ 0x30, 1 },            /* header reserved tail (first byte) */
		{ 0x3F, 1 },            /* header reserved tail (last byte) */
		{ HDR + 0, 'g' },       /* lowercase in record ID */
		{ HDR + 5, '!' },       /* punctuation in record ID */
		{ HDR + 6, 0x80 },      /* unknown record flag bit */
		{ HDR + 7, 1 },         /* record reserved byte */
		{ HDR + 0x1C, 1 },      /* record reserved u32 */
	};
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	size_t i;
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		u8 *evil = malloc(len);
		uiAssetsSource_t src;
		memcpy(evil, pack, len);
		evil[cases[i].off] = cases[i].val;
		packFixCrc(evil, 3);
		src = memSource(evil, len);
		CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_ERR_FORMAT);
		free(evil);
	}
	/* Content-geometry words, also behind a valid CRC. */
	{
		u8 *evil = malloc(len);
		uiAssetsSource_t src;
		memcpy(evil, pack, len);
		wbe16(evil + 0x28, 128); /* wrong content width */
		packFixCrc(evil, 3);
		src = memSource(evil, len);
		CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_ERR_FORMAT);
		free(evil);
	}
	free(pack);
}

static void test_index_read_failure(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	mem.failAfter = 1; /* header succeeds, index read fails */
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_ERR_IO);
	CHECK(!UIAssets_Ready());
	free(pack);
}

static void test_index_crc_mismatch(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src;
	pack[HDR + 0x14] ^= 0xFF; /* flip a dominant-color byte: CRC must catch */
	src = memSource(pack, len);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_ERR_CRC);
	free(pack);
}

static void test_byte_flip_fuzz(void) {
	/* Every single-byte corruption of header+index must be rejected
	 * without a crash. Payload corruption is caught later per-poster. */
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	u32 metaLen = HDR + 3 * REC;
	u32 off;
	uiAssetsSource_t src;
	for (off = 0; off < metaLen; off++) {
		pack[off] ^= 0xA5;
		src = memSource(pack, len);
		CHECK(UIAssets_Init(&src, &testSync) != UI_ASSETS_OK);
		pack[off] ^= 0xA5;
	}
	src = memSource(pack, len); /* pristine pack still loads */
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	free(pack);
}

static void test_poster_payload_corrupt(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src;
	uiPosterHandle_t h;
	u32 dataOffset = rbe32(pack + 0x18);
	int readsAfter;
	pack[dataOffset + 5] ^= 0xFF; /* corrupt GALE01's texels */
	src = memSource(pack, len);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	requestOne("GALE01");
	pollAll();
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_CORRUPT_OR_UNAVAILABLE);
	CHECK(UIAssets_Peek(h) == NULL);
	/* FAILED is a negative cache: re-request must not retry-loop. */
	readsAfter = mem.readCalls;
	requestOne("GALE01");
	pollAll();
	CHECK(mem.readCalls == readsAfter);
	free(pack);
}

static void test_short_read_fails_poster(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	uiPosterHandle_t h;
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	mem.shortReadAt = rbe32(pack + 0x18);
	requestOne("GALE01");
	pollAll();
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_CORRUPT_OR_UNAVAILABLE);
	free(pack);
}

/* n < 1000 -> G###E0, n >= 1000 -> H###E0: unique, valid charset, and
 * lexicographic order matches numeric order up to 1999 titles. */
static void nthId(char *dst, int n) {
	unsigned int value;
	memset(dst, 0, 8);
	CHECK(n >= 0 && n < 2000);
	if (n < 0 || n >= 2000)
		return;
	value = (unsigned int)n % 1000u;
	dst[0] = n < 1000 ? 'G' : 'H';
	dst[1] = (char)('0' + value / 100u);
	dst[2] = (char)('0' + value / 10u % 10u);
	dst[3] = (char)('0' + value % 10u);
	dst[4] = 'E';
	dst[5] = '0';
}

static void test_nth_id_boundaries(void) {
	char id[8];

	nthId(id, 0);
	CHECK(strcmp(id, "G000E0") == 0);
	nthId(id, 999);
	CHECK(strcmp(id, "G999E0") == 0);
	nthId(id, 1000);
	CHECK(strcmp(id, "H000E0") == 0);
	nthId(id, 1999);
	CHECK(strcmp(id, "H999E0") == 0);
}

static void makeIds(char ids[][8], int first, int count) {
	int i;
	for (i = 0; i < count; i++)
		nthId(ids[i], first + i);
}

static testRec_t *makeRecs(int count) {
	testRec_t *recs = calloc((size_t)count, sizeof(*recs) + 8u);
	char *names;
	int i;
	if (!recs) {
		perror("makeRecs");
		exit(2);
	}
	names = (char *)(recs + count);
	for (i = 0; i < count; i++) {
		nthId(names + (size_t)i * 8, i);
		recs[i].id = names + (size_t)i * 8;
		recs[i].dom[0] = (u8)i;
	}
	return recs; /* IDs share this allocation and are released with recs. */
}

/* Assert a poster is fully loaded and holds its record's exact payload. */
static void checkLoaded(const char *id) {
	uiPosterHandle_t h;
	GXTexObj *tex;
	u8 full[UI_ASSETS_POSTER_BYTES];
	CHECK(UIAssets_Query(id, 6, true, &h) == UI_POSTER_EXACT);
	tex = UIAssets_Peek(h);
	CHECK(tex != NULL);
	if (tex) {
		fillPayload(full, id);
		CHECK(memcmp(tex->data, full, 64) == 0);
	}
}

static void test_window_prefetch_loads_all_seven(void) {
	int count = 20;
	testRec_t *recs = makeRecs(count);
	size_t len;
	u8 *pack = buildPack(recs, count, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	char ids[UI_ASSETS_WINDOW][8];
	int i;

	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	makeIds(ids, 5, UI_ASSETS_WINDOW);
	UIAssets_RequestWindow(ids, UI_ASSETS_WINDOW, 3);
	pollAll();
	/* Every window entry -- not just the selected one -- must be resident
	 * with its own record's exact payload. */
	for (i = 0; i < UI_ASSETS_WINDOW; i++)
		checkLoaded(ids[i]);
	CHECK(mem.readCalls == 2 + UI_ASSETS_WINDOW); /* header + index + 7 */
	CHECK(flushCalls == UI_ASSETS_WINDOW); /* one unlocked flush per load */
	free(pack);
	free(recs);
}

static void test_window_eviction_and_reversal(void) {
	int count = 20;
	testRec_t *recs = makeRecs(count);
	size_t len;
	u8 *pack = buildPack(recs, count, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	char ids[UI_ASSETS_WINDOW][8];
	uiPosterHandle_t h;
	int reads;

	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	makeIds(ids, 3, UI_ASSETS_WINDOW); /* window 3..9, selected 6 */
	UIAssets_RequestWindow(ids, UI_ASSETS_WINDOW, 3);
	pollAll();
	CHECK(UIAssets_Query("G006E0", 6, true, &h) == UI_POSTER_EXACT);
	CHECK(UIAssets_Peek(h) != NULL);
	reads = mem.readCalls;

	/* Shift by one: exactly one new poster read, six kept. */
	makeIds(ids, 4, UI_ASSETS_WINDOW);
	UIAssets_RequestWindow(ids, UI_ASSETS_WINDOW, 3);
	pollAll();
	CHECK(mem.readCalls == reads + 1);

	/* The evicted edge (G003E0) is stale now. */
	CHECK(UIAssets_Query("G003E0", 6, true, &h) == UI_POSTER_EXACT);
	CHECK(UIAssets_Peek(h) == NULL); /* record exists but no slot */

	/* Rapid reversal WITH loads interleaved: the clock advances past the
	 * quarantine between flips, so posters land mid-storm and are then
	 * re-evicted -- the harsher path than pure request churn. */
	{
		int flip;
		int readsBefore = mem.readCalls;
		for (flip = 0; flip < 25; flip++) {
			makeIds(ids, (flip & 1) ? 3 : 10, UI_ASSETS_WINDOW);
			UIAssets_RequestWindow(ids, UI_ASSETS_WINDOW, 3);
			fakeNowValue += UI_ASSETS_EVICT_QUARANTINE_MS + 1;
			UIAssets_Poll();
			UIAssets_Poll();
		}
		CHECK(mem.readCalls > readsBefore); /* the storm really loaded */
		pollAll();
		makeIds(ids, 10, UI_ASSETS_WINDOW);
		UIAssets_RequestWindow(ids, UI_ASSETS_WINDOW, 3);
		pollAll();
		/* Full settled window is correct after the storm. */
		{
			int i;
			for (i = 0; i < UI_ASSETS_WINDOW; i++)
				checkLoaded(ids[i]);
		}
	}
	CHECK(UIAssets_MemoryFootprint() ==
	      UI_ASSETS_SLOTS * UI_ASSETS_POSTER_BYTES + (u32)count * REC);
	free(pack);
	free(recs);
}

static void test_stale_handle_after_eviction(void) {
	int count = 20;
	testRec_t *recs = makeRecs(count);
	size_t len;
	u8 *pack = buildPack(recs, count, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	char ids[UI_ASSETS_WINDOW][8];
	uiPosterHandle_t h;

	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	makeIds(ids, 0, UI_ASSETS_WINDOW);
	UIAssets_RequestWindow(ids, UI_ASSETS_WINDOW, 0);
	pollAll();
	CHECK(UIAssets_Query("G000E0", 6, true, &h) == UI_POSTER_EXACT);
	CHECK(UIAssets_Peek(h) != NULL);
	makeIds(ids, 10, UI_ASSETS_WINDOW); /* push far away: full eviction */
	UIAssets_RequestWindow(ids, UI_ASSETS_WINDOW, 0);
	CHECK(UIAssets_Peek(h) == NULL);    /* stale before any reload */
	CHECK(UIAssets_Acquire(h) == NULL);
	pollAll();
	CHECK(UIAssets_Peek(h) == NULL);    /* and after the slot is reused */
	free(pack);
	free(recs);
}

static void test_pin_retention_through_detail_launch(void) {
	int count = 60;
	testRec_t *recs = makeRecs(count);
	size_t len;
	u8 *pack = buildPack(recs, count, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	char ids[UI_ASSETS_SLOTS][8];
	char pinnedId[8];
	uiPosterHandle_t h;
	GXTexObj *tex;
	int middle = UI_ASSETS_SLOTS / 2;
	int i;

	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	makeIds(ids, 0, UI_ASSETS_SLOTS);
	UIAssets_RequestWindow(ids, UI_ASSETS_SLOTS, middle);
	pollAll();
	memcpy(pinnedId, ids[middle], sizeof(pinnedId));
	CHECK(UIAssets_Query(pinnedId, 6, true, &h) == UI_POSTER_EXACT);
	tex = UIAssets_Acquire(h); /* entering Game Detail */
	CHECK(tex != NULL);

	/* Library window scrolls far away; the pinned poster must survive. */
	makeIds(ids, 30, UI_ASSETS_SLOTS);
	UIAssets_RequestWindow(ids, UI_ASSETS_SLOTS, middle);
	pollAll();
	CHECK(UIAssets_Peek(h) == tex);
	/* The pin leaves one slot short. Assignment is distance-ordered
	 * (selected first, the left of a tie first), so every card but the
	 * farthest tie-loser on the right loads, and that one waits slotless. */
	for (i = 0; i < UI_ASSETS_SLOTS - 1; i++)
		checkLoaded(ids[i]);
	{
		uiPosterHandle_t edge;
		CHECK(UIAssets_Query(ids[UI_ASSETS_SLOTS - 1], 6, true, &edge) ==
		      UI_POSTER_EXACT);
		CHECK(UIAssets_Peek(edge) == NULL); /* record exists, no slot */
	}

	/* Back out of Detail: release, then the next window may evict it. */
	{
		uiPosterHandle_t pinned;
		CHECK(UIAssets_Query(pinnedId, 6, true, &pinned) == UI_POSTER_EXACT);
		UIAssets_Release(pinned);
		makeIds(ids, 30, UI_ASSETS_SLOTS);
		UIAssets_RequestWindow(ids, UI_ASSETS_SLOTS, middle);
		pollAll();
		CHECK(UIAssets_Peek(pinned) == NULL);
		checkLoaded(ids[UI_ASSETS_SLOTS - 1]);
	}
	free(pack);
	free(recs);
}

/* The Grid layout asks for its whole window nearest first (selected 0), and
 * a carousel afterwards keeps exactly its own seven: no extra posters stay
 * resident and none of its seven is read again. */
static void test_grid_window_then_carousel(void) {
	int count = 40;
	testRec_t *recs = makeRecs(count);
	size_t len;
	u8 *pack = buildPack(recs, count, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	char ids[UI_ASSETS_SLOTS][8];
	char carousel[UI_ASSETS_WINDOW][8];
	uiPosterHandle_t h;
	int reads;
	int i;
	int j;

	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	/* The window in the grid's own order, not the pack's. */
	for (i = 0; i < UI_ASSETS_SLOTS; i++)
		nthId(ids[i], (i * 7) % UI_ASSETS_SLOTS);
	UIAssets_RequestWindow(ids, UI_ASSETS_SLOTS, 0);
	for (i = 0; i < UI_ASSETS_SLOTS; i++) {
		fakeNowValue += UI_ASSETS_EVICT_QUARANTINE_MS + 1;
		UIAssets_Poll();
		/* After i + 1 polls the first i + 1 of the window are in. */
		for (j = 0; j < UI_ASSETS_SLOTS; j++) {
			CHECK(UIAssets_Query(ids[j], 6, true, &h) == UI_POSTER_EXACT);
			CHECK((UIAssets_Peek(h) != NULL) == (j <= i));
		}
	}
	CHECK(mem.readCalls == 2 + UI_ASSETS_SLOTS);
	reads = mem.readCalls;

	makeIds(carousel, 9, UI_ASSETS_WINDOW); /* all seven already resident */
	UIAssets_RequestWindow(carousel, UI_ASSETS_WINDOW, 3);
	pollAll();
	CHECK(mem.readCalls == reads);
	for (i = 0; i < count; i++) {
		char id[8];
		nthId(id, i);
		CHECK(UIAssets_Query(id, 6, true, &h) == UI_POSTER_EXACT);
		CHECK((UIAssets_Peek(h) != NULL) == (i >= 9 && i < 9 + UI_ASSETS_WINDOW));
	}
	free(pack);
	free(recs);
}

static void test_cancel_for_device_change(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	char ids[3][8];
	uiPosterHandle_t h;
	GXTexObj *tex;

	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	memset(ids, 0, sizeof(ids));
	memcpy(ids[0], "GALE01", 6);
	memcpy(ids[1], "GC6E01", 6);
	memcpy(ids[2], "GZLE01", 6);
	UIAssets_RequestWindow(ids, 3, 0);
	fakeNowValue += UI_ASSETS_EVICT_QUARANTINE_MS + 1;
	UIAssets_Poll(); /* only the selected poster lands; two still pending */
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_EXACT);
	tex = UIAssets_Acquire(h);
	CHECK(tex != NULL);

	UIAssets_CancelForDeviceChange(); /* mid-load, selected pinned */
	CHECK(!UIAssets_Ready());
	CHECK(UIAssets_Peek(h) == NULL);      /* pinned handles die too */
	CHECK(UIAssets_Acquire(h) == NULL);
	CHECK(!UIAssets_Poll());              /* loading fully stopped */
	{
		int reads = mem.readCalls;
		UIAssets_Poll();
		CHECK(mem.readCalls == reads);    /* and never touches the device */
	}
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_USE_BNR);

	/* Device comes back: re-Init reuses the arena and works. */
	src = memSource(pack, len);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	requestOne("GALE01");
	pollAll();
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_EXACT);
	CHECK(UIAssets_Peek(h) != NULL);
	free(pack);
}

static void test_sync_validation_policy(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	uiAssetsSync_t lockOnly = { fakeLock, NULL, NULL };
	uiAssetsSync_t unlockOnly = { NULL, fakeUnlock, NULL };
	uiAssetsSync_t noCallbacks = { NULL, NULL, NULL };

	/* A half-pair is rejected before any pack I/O or allocation in every
	 * build, including the deliberately synchronization-free host mode. */
	CHECK(UIAssets_Init(&src, &lockOnly) == UI_ASSETS_ERR_STATE);
	CHECK(UIAssets_Init(&src, &unlockOnly) == UI_ASSETS_ERR_STATE);
	CHECK(mem.readCalls == 0);
	CHECK(UIAssets_MemoryFootprint() == 0);

#ifdef UI_ASSETS_HOST_REQUIRE_SYNC
	/* This host build exercises the target policy without requiring libogc:
	 * NULL and an all-zero pair are both illegal on-console. */
	CHECK(UIAssets_Init(&src, NULL) == UI_ASSETS_ERR_STATE);
	CHECK(UIAssets_Init(&src, &noCallbacks) == UI_ASSETS_ERR_STATE);
	CHECK(mem.readCalls == 0);
#else
	/* Normal host tests may deliberately run single-threaded with no lock.
	 * NULL and the identical all-zero triple are the same bound identity. */
	CHECK(UIAssets_Init(&src, NULL) == UI_ASSETS_OK);
	UIAssets_CancelForDeviceChange();
	CHECK(UIAssets_Init(&src, &noCallbacks) == UI_ASSETS_OK);
	UIAssets_CancelForDeviceChange();
	CHECK(UIAssets_DisposeAfterVideoStop() == UI_ASSETS_OK);
#endif
	free(pack);
}

static void test_sync_binding_lifecycle(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	trackedMutex_t first = { 0, 0, 0 };
	trackedMutex_t second = { 0, 0, 0 };
	uiAssetsSync_t bound = { trackedLock, trackedUnlock, &first };
	uiAssetsSync_t same = { trackedLock, trackedUnlock, &first };
	uiAssetsSync_t changedCtx = { trackedLock, trackedUnlock, &second };
	uiAssetsSync_t changedCallbacks = {
		alternateLock, alternateUnlock, &first
	};
	uiAssetsSync_t lockOnly = { trackedLock, NULL, &first };
	uiAssetsSync_t unlockOnly = { NULL, trackedUnlock, &first };
	int reads;

	CHECK(UIAssets_Init(&src, &bound) == UI_ASSETS_OK);
	UIAssets_CancelForDeviceChange();

	/* The identical triple survives Cancel and is accepted on re-Init. */
	CHECK(UIAssets_Init(&src, &same) == UI_ASSETS_OK);
	UIAssets_CancelForDeviceChange();

	/* Context, callback identity, and completeness are immutable until the
	 * final disposal. Rejections happen before any source read. */
	reads = mem.readCalls;
	CHECK(UIAssets_Init(&src, &changedCtx) == UI_ASSETS_ERR_STATE);
	CHECK(UIAssets_Init(&src, &changedCallbacks) == UI_ASSETS_ERR_STATE);
	CHECK(UIAssets_Init(&src, &lockOnly) == UI_ASSETS_ERR_STATE);
	CHECK(UIAssets_Init(&src, &unlockOnly) == UI_ASSETS_ERR_STATE);
	CHECK(mem.readCalls == reads);
	CHECK(!UIAssets_Ready());

	/* Failed replacements do not disturb the original binding. */
	CHECK(UIAssets_Init(&src, &bound) == UI_ASSETS_OK);
	UIAssets_CancelForDeviceChange();
	CHECK(UIAssets_DisposeAfterVideoStop() == UI_ASSETS_OK);
	free(pack);
}

static void test_dispose_requires_cancel(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	u32 footprint;
	int allocations;
	int calls;

	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	footprint = UIAssets_MemoryFootprint();
	allocations = liveAllocs;
	calls = lockCalls;
	CHECK(UIAssets_DisposeAfterVideoStop() == UI_ASSETS_ERR_STATE);
	CHECK(lockCalls == calls); /* even the error path is lock-free */
	CHECK(UIAssets_Ready());
	CHECK(UIAssets_MemoryFootprint() == footprint);
	CHECK(liveAllocs == allocations);
	UIAssets_CancelForDeviceChange();
	CHECK(UIAssets_DisposeAfterVideoStop() == UI_ASSETS_OK);
	free(pack);
}

static void test_dispose_after_mutex_destroyed(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	trackedMutex_t mutex = { 0, 0, 0 };
	uiAssetsSync_t sync = { trackedLock, trackedUnlock, &mutex };
	int locks, unlocks, allLocks;

	CHECK(UIAssets_Init(&src, &sync) == UI_ASSETS_OK);
	UIAssets_CancelForDeviceChange(); /* mutex is still live here */
	locks = mutex.lockCalls;
	unlocks = mutex.unlockCalls;
	allLocks = lockCalls;
	mutex.destroyed = 1;
	CHECK(UIAssets_DisposeAfterVideoStop() == UI_ASSETS_OK);
	CHECK(mutex.lockCalls == locks);
	CHECK(mutex.unlockCalls == unlocks);
	CHECK(lockCalls == allLocks);
	CHECK(UIAssets_MemoryFootprint() == 0);
	CHECK(liveAllocs == 0);
	free(pack);
}

static void test_dispose_releases_everything(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	uiPosterHandle_t h;
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	requestOne("GALE01");
	pollAll();
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_EXACT);
	CHECK(UIAssets_Peek(h) != NULL);
	UIAssets_CancelForDeviceChange();
	CHECK(UIAssets_DisposeAfterVideoStop() == UI_ASSETS_OK);
	CHECK(UIAssets_MemoryFootprint() == 0);
	CHECK(liveAllocs == 0);
	CHECK(liveBytes == 0);
	CHECK(UIAssets_Peek(h) == NULL);
	/* Handles from before disposal stay stale even after re-Init. */
	src = memSource(pack, len);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	requestOne("GALE01");
	pollAll();
	CHECK(UIAssets_Peek(h) == NULL);
	free(pack);
}

static void test_quarantine_blocks_rewrite(void) {
	int count = 20;
	testRec_t *recs = makeRecs(count);
	size_t len;
	u8 *pack = buildPack(recs, count, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	char ids[UI_ASSETS_WINDOW][8];
	int reads;

	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	makeIds(ids, 0, UI_ASSETS_WINDOW);
	UIAssets_RequestWindow(ids, UI_ASSETS_WINDOW, 0);
	pollAll();
	reads = mem.readCalls;

	/* Evict everything, immediately rewindow: texels must not be
	 * rewritten until a frame has passed (the GPU may still sample). */
	makeIds(ids, 10, UI_ASSETS_WINDOW);
	UIAssets_RequestWindow(ids, UI_ASSETS_WINDOW, 0);
	CHECK(UIAssets_Poll());               /* work queued... */
	CHECK(mem.readCalls == reads);        /* ...but no rewrite yet */
	fakeNowValue += UI_ASSETS_EVICT_QUARANTINE_MS / 2;
	UIAssets_Poll();
	CHECK(mem.readCalls == reads);        /* still inside quarantine */
	fakeNowValue += UI_ASSETS_EVICT_QUARANTINE_MS;
	UIAssets_Poll();
	CHECK(mem.readCalls == reads + 1);    /* quarantine passed: loads */
	free(pack);
	free(recs);
}

static void test_scale_100_and_250(void) {
	int sizes[2] = { 100, 250 };
	int s;
	for (s = 0; s < 2; s++) {
		int count = sizes[s];
		testRec_t *recs = makeRecs(count);
		size_t len;
		u8 *pack = buildPack(recs, count, &len, 0);
		uiAssetsSource_t src = memSource(pack, len);
		char ids[UI_ASSETS_WINDOW][8];
		int pos;
		uiPosterHandle_t h;

		CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
		for (pos = 0; pos + UI_ASSETS_WINDOW <= count; pos += 13) {
			UIAssets_RequestWindow(ids, 0, 0); /* degenerate call is a no-op */
			makeIds(ids, pos, UI_ASSETS_WINDOW);
			UIAssets_RequestWindow(ids, UI_ASSETS_WINDOW, 3);
			pollAll();
			CHECK(UIAssets_Query(ids[3], 6, true, &h) == UI_POSTER_EXACT);
			CHECK(UIAssets_Peek(h) != NULL);
			if (UIAssets_Peek(h)) {
				u8 expect[16];
				u8 full[UI_ASSETS_POSTER_BYTES];
				fillPayload(full, ids[3]);
				memcpy(expect, full, 16);
				CHECK(memcmp(UIAssets_Peek(h)->data, expect, 16) == 0);
			}
			CHECK(UIAssets_MemoryFootprint() ==
			      UI_ASSETS_SLOTS * UI_ASSETS_POSTER_BYTES +
			      (u32)count * REC);
		}
		UIAssets_CancelForDeviceChange();
		CHECK(UIAssets_DisposeAfterVideoStop() == UI_ASSETS_OK);
		free(pack);
		free(recs);
	}
}

static void test_max_records_boundary(void) {
	int count = UI_ASSETS_MAX_RECORDS;
	testRec_t *recs = makeRecs(count);
	size_t len;
	u8 *pack = buildPack(recs, count, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	/* Ceiling holds at the largest legal pack: arena + full 32 KiB index. */
	CHECK(UIAssets_MemoryFootprint() ==
	      UI_ASSETS_SLOTS * UI_ASSETS_POSTER_BYTES +
	      (u32)UI_ASSETS_MAX_RECORDS * REC);
	{
		char ids[UI_ASSETS_WINDOW][8];
		makeIds(ids, count - UI_ASSETS_WINDOW, UI_ASSETS_WINDOW);
		UIAssets_RequestWindow(ids, UI_ASSETS_WINDOW, 3);
		pollAll();
		checkLoaded(ids[3]); /* deep-index record loads correctly */
	}
	UIAssets_CancelForDeviceChange();
	CHECK(UIAssets_DisposeAfterVideoStop() == UI_ASSETS_OK);
	/* count = 1025 must be rejected before anything else is trusted. */
	wbe32(pack + 0x0C, UI_ASSETS_MAX_RECORDS + 1);
	src = memSource(pack, len);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_ERR_FORMAT);
	free(pack);
	free(recs);
}

static void test_memory_budget(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	int allocsBefore;
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	/* Arena: 25 x 43,648 = 1,091,200 bytes, the Grid layout's five rows of
	 * five (the carousels use 7 of them); index <= 32 KiB on top. */
	CHECK(UI_ASSETS_SLOTS * UI_ASSETS_POSTER_BYTES == 1091200);
	CHECK(UIAssets_MemoryFootprint() <= 1091200 + 32768);
	allocsBefore = allocCalls;
	allocsFrozen = 1;
	requestOne("GALE01");
	pollAll();
	{
		uiPosterHandle_t h;
		UIAssets_Query("GALE01", 6, true, &h);
		UIAssets_Peek(h);
		UIAssets_Acquire(h);
		UIAssets_Release(h);
		UIAssets_DominantColor("GALE01", 6, NULL, NULL, NULL);
	}
	allocsFrozen = 0;
	CHECK(allocCalls == allocsBefore); /* zero allocation after Init */
	free(pack);
}

static void test_dominant_color(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	u8 r = 0, g = 0, b = 0;
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	CHECK(UIAssets_DominantColor("GALE01", 6, &r, &g, &b));
	CHECK(r == 10 && g == 20 && b == 30);
	CHECK(UIAssets_DominantColor("GZLE69", 6, &r, &g, &b)); /* via universal */
	CHECK(r == 40 && g == 50 && b == 60);
	CHECK(!UIAssets_DominantColor("XXXX00", 6, NULL, NULL, NULL));
	free(pack);
}

static void test_bad_ids_and_args(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	uiPosterHandle_t h;
	char ids[9][8];
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	CHECK(UIAssets_Query(NULL, 6, true, &h) == UI_POSTER_USE_BNR);
	CHECK(UIAssets_Query("gale01", 6, true, &h) == UI_POSTER_USE_BNR);
	CHECK(UIAssets_Query("GA LE1", 6, false, &h) == UI_POSTER_PROCEDURAL_CARD);
	CHECK(UIAssets_Query("GALE01", 6, true, NULL) == UI_POSTER_EXACT);
	memset(ids, 0, sizeof(ids));
	memcpy(ids[0], "GALE01", 6);
	UIAssets_RequestWindow(NULL, 3, 0);
	UIAssets_RequestWindow(ids, 9, 42);   /* clamped, no crash */
	UIAssets_RequestWindow(ids, -1, -5);
	pollAll();
	{
		uiPosterHandle_t bogus = { 0, 0xFFFF, 0 };
		CHECK(UIAssets_Peek(bogus) == NULL);
		bogus.slot = 3;
		bogus.generation = 0xFFFF;
		CHECK(UIAssets_Peek(bogus) == NULL);
		UIAssets_Release(bogus);
	}
	free(pack);
}

static void test_uninitialized_api_is_inert(void) {
	uiPosterHandle_t h = { 0, 0, 0 };
	char ids[1][8];
	memset(ids, 0, sizeof(ids));
	CHECK(!UIAssets_Ready());
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_USE_BNR);
	CHECK(UIAssets_Peek(h) == NULL);
	CHECK(!UIAssets_Poll());
	CHECK(UIAssets_MemoryFootprint() == 0);
	UIAssets_RequestWindow(ids, 1, 0);
	UIAssets_CancelForDeviceChange();
	CHECK(UIAssets_DisposeAfterVideoStop() == UI_ASSETS_OK);
	CHECK(liveAllocs == 0);
}

static void test_poll_lock_phase_split(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	int calls, reads;

	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	requestOne("GALE01");
	fakeNowValue += UI_ASSETS_EVICT_QUARANTINE_MS + 1;
	calls = lockCalls;
	UIAssets_Poll(); /* loads: capture section + publish section */
	CHECK(lockCalls == calls + 2);
	{
		uiPosterHandle_t h;
		CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_EXACT);
		CHECK(UIAssets_Peek(h) != NULL);
	}
	calls = lockCalls;
	CHECK(!UIAssets_Poll()); /* idle: one section, no read */
	CHECK(lockCalls == calls + 1);

	/* Quarantined pending work: one section, and provably no read. */
	requestOne("GC6E01"); /* evicts GALE01, assigns into quarantined slot */
	calls = lockCalls;
	reads = mem.readCalls;
	CHECK(UIAssets_Poll());
	CHECK(lockCalls == calls + 1);
	CHECK(mem.readCalls == reads);
	free(pack);
}

static void test_video_apis_never_lock(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	uiPosterHandle_t h;
	int calls;
	u8 r, g, b;

	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	requestOne("GALE01");
	pollAll();
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_EXACT);
	/* The EV_GAMEFLOW render pass calls these while it already owns the
	 * video mutex: they must never take the lock themselves. */
	calls = lockCalls;
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_EXACT);
	CHECK(UIAssets_Peek(h) != NULL);
	CHECK(UIAssets_Query("GZLE69", 6, true, NULL) == UI_POSTER_UNIVERSAL);
	CHECK(UIAssets_DominantColor("GALE01", 6, &r, &g, &b));
	CHECK(lockCalls == calls);
	free(pack);
}

static void test_menu_apis_single_section(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	uiPosterHandle_t h;
	GXTexObj *tex;
	int calls;
	char ids[1][8];

	calls = lockCalls;
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	CHECK(lockCalls == calls + 1); /* staging is unlocked; publish locked */
	requestOne("GALE01");
	pollAll();
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_EXACT);

	memset(ids, 0, sizeof(ids));
	memcpy(ids[0], "GALE01", 6);
	calls = lockCalls;
	UIAssets_RequestWindow(ids, 1, 0);
	CHECK(lockCalls == calls + 1);
	calls = lockCalls;
	tex = UIAssets_Acquire(h);
	CHECK(tex != NULL);
	CHECK(lockCalls == calls + 1);
	calls = lockCalls;
	UIAssets_Release(h);
	CHECK(lockCalls == calls + 1);
	calls = lockCalls;
	UIAssets_CancelForDeviceChange();
	CHECK(lockCalls == calls + 1);
	/* Final disposal follows the live-mutex Cancel above and must never
	 * invoke the synchronization callbacks after the video thread stops. */
	calls = lockCalls;
	CHECK(UIAssets_DisposeAfterVideoStop() == UI_ASSETS_OK);
	CHECK(lockCalls == calls);
	free(pack);
}

static void chaosCancel(void) {
	memReadHook = NULL; /* fire once */
	UIAssets_CancelForDeviceChange();
}

static void test_cancel_inside_read_drops_publication(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	uiPosterHandle_t h;

	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	requestOne("GALE01");
	fakeNowValue += UI_ASSETS_EVICT_QUARANTINE_MS + 1;
	/* The device disappears while Poll's unlocked read is in flight (the
	 * hook runs from inside the read callback, where the lock is NOT
	 * held). The completed job must be dropped, not published. */
	memReadHook = chaosCancel;
	CHECK(!UIAssets_Poll());
	CHECK(!UIAssets_Ready());
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_USE_BNR);

	/* Re-open the pack: the stale job must not have leaked a READY state
	 * -- the poster still needs a real load. */
	src = memSource(pack, len);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	requestOne("GALE01");
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_EXACT);
	CHECK(UIAssets_Peek(h) == NULL); /* pending, not stale-published */
	pollAll();
	checkLoaded("GALE01");
	free(pack);
}

static u8 *chaosPack;
static size_t chaosPackLen;

static void chaosCancelAndReinit(void) {
	char ids[1][8];
	uiAssetsSource_t src;
	memReadHook = NULL; /* fire once */
	UIAssets_CancelForDeviceChange();
	src = memSource(chaosPack, chaosPackLen);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	memset(ids, 0, sizeof(ids));
	memcpy(ids[0], "GBLE01", 6);
	UIAssets_RequestWindow(ids, 1, 0);
}

static void test_reinit_inside_read_no_stale_publish(void) {
	static const testRec_t PACK_B[] = {
		{ "GBLE01", 0, { 7, 7, 7 } },
	};
	size_t lenA;
	u8 *packA = buildPack(BASIC, 3, &lenA, 0);
	uiAssetsSource_t src;
	uiPosterHandle_t h;

	chaosPack = buildPack(PACK_B, 1, &chaosPackLen, 0);
	src = memSource(packA, lenA);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	requestOne("GALE01");
	fakeNowValue += UI_ASSETS_EVICT_QUARANTINE_MS + 1;
	/* Mid-read, the whole pack is swapped (cancel + re-Init + rewindow).
	 * The in-flight job -- which now reads from the NEW backing store at
	 * the OLD offset -- must never publish into the new owner's slot. */
	memReadHook = chaosCancelAndReinit;
	UIAssets_Poll();
	CHECK(UIAssets_Ready());
	CHECK(UIAssets_Query("GBLE01", 6, true, &h) == UI_POSTER_EXACT);
	CHECK(UIAssets_Peek(h) == NULL); /* still pending: stale job dropped */
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_USE_BNR);
	pollAll();
	checkLoaded("GBLE01");
	free(packA);
	free(chaosPack);
	chaosPack = NULL;
}

static void test_generation_u32_wrap(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	uiPosterHandle_t preWrap, postWrap;

	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	requestOne("GALE01");
	pollAll();
	CHECK(UIAssets_Query("GALE01", 6, true, &preWrap) == UI_POSTER_EXACT);
	CHECK(UIAssets_Peek(preWrap) != NULL);

	/* Park the slot at the wrap boundary, then hand ownership over: the
	 * next assignment increments 0xFFFFFFFF -> 0. */
	UIAssetsTest_ForceGeneration(preWrap.slot, 0xFFFFFFFFu);
	CHECK(UIAssets_Query("GALE01", 6, true, &preWrap) == UI_POSTER_EXACT);
	CHECK(preWrap.generation == 0xFFFFFFFFu);
	requestOne("GC6E01"); /* evicts GALE01's slot, reassigns it */
	pollAll();
	CHECK(UIAssets_Query("GC6E01", 6, true, &postWrap) == UI_POSTER_EXACT);
	CHECK(postWrap.slot == preWrap.slot);
	CHECK(postWrap.generation == 0);
	CHECK(UIAssets_Peek(postWrap) != NULL);
	/* The boundary handle is stale across the wrap, not resurrected. */
	CHECK(UIAssets_Peek(preWrap) == NULL);
	CHECK(UIAssets_Acquire(preWrap) == NULL);
	free(pack);
}

static void test_short_id_rejected_before_read(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	uiPosterHandle_t h;
	char *shortId;

	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	/* Exactly-5-byte heap buffer: under ASan, touching byte 6 aborts, so
	 * passing this proves rejection happens before any read. */
	shortId = malloc(5);
	memcpy(shortId, "GALE0", 5);
	CHECK(UIAssets_Query(shortId, 5, true, &h) == UI_POSTER_USE_BNR);
	CHECK(UIAssets_Query(shortId, 0, false, &h) == UI_POSTER_PROCEDURAL_CARD);
	CHECK(!UIAssets_DominantColor(shortId, 5, NULL, NULL, NULL));
	free(shortId);
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_EXACT);
	CHECK(UIAssets_Query("GALE01xyz", 9, true, &h) == UI_POSTER_EXACT);
	free(pack);
}

static char chaosWindowIds[UI_ASSETS_WINDOW][8];

static void chaosRewindow(void) {
	memReadHook = NULL; /* fire once */
	UIAssets_RequestWindow(chaosWindowIds, UI_ASSETS_WINDOW, 0);
}

static void test_request_window_mid_read_no_stale_publish(void) {
	/* Pure eviction/reassignment between Poll's capture and publish, with
	 * NO cancel: the loading slot is evicted and reassigned to a new
	 * record while its read is in flight. The stale job must not publish
	 * into the new owner. */
	int count = 20;
	testRec_t *recs = makeRecs(count);
	size_t len;
	u8 *pack = buildPack(recs, count, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	char ids[UI_ASSETS_WINDOW][8];
	uiPosterHandle_t h;
	int i;

	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	makeIds(ids, 0, UI_ASSETS_WINDOW);       /* window A: 0..6 */
	makeIds(chaosWindowIds, 10, UI_ASSETS_WINDOW); /* window B: 10..16 */
	UIAssets_RequestWindow(ids, UI_ASSETS_WINDOW, 0);
	fakeNowValue += UI_ASSETS_EVICT_QUARANTINE_MS + 1;
	memReadHook = chaosRewindow;
	UIAssets_Poll(); /* loads G000E0; mid-read the window jumps to B */
	/* The in-flight G000E0 job must not have published anywhere. */
	CHECK(UIAssets_Query("G000E0", 6, true, &h) == UI_POSTER_EXACT);
	CHECK(UIAssets_Peek(h) == NULL);
	for (i = 0; i < UI_ASSETS_WINDOW; i++) {
		CHECK(UIAssets_Query(chaosWindowIds[i], 6, true, &h) ==
		      UI_POSTER_EXACT);
		CHECK(UIAssets_Peek(h) == NULL); /* pending, never stale-published */
	}
	pollAll();
	for (i = 0; i < UI_ASSETS_WINDOW; i++)
		checkLoaded(chaosWindowIds[i]); /* correct content lands after */
	free(pack);
	free(recs);
}

static void test_acquire_pin_saturation(void) {
	size_t len;
	u8 *pack = buildPack(BASIC, 3, &len, 0);
	uiAssetsSource_t src = memSource(pack, len);
	uiPosterHandle_t h;
	GXTexObj *tex;
	int i;

	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	requestOne("GALE01");
	pollAll();
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_EXACT);
	tex = UIAssets_Peek(h);
	CHECK(tex != NULL);
	for (i = 0; i < 255; i++)
		CHECK(UIAssets_Acquire(h) == tex);
	/* At the cap, Acquire must refuse (an uncounted pin could later be
	 * cancelled out by paired Releases, leaving a holder unprotected). */
	CHECK(UIAssets_Acquire(h) == NULL);
	for (i = 0; i < 255; i++)
		UIAssets_Release(h);
	/* Pins balanced back to zero: the slot is evictable again. */
	requestOne("GC6E01");
	CHECK(UIAssets_Peek(h) == NULL);
	free(pack);
}

static void test_real_pack(const char *path) {
	FILE *f = fopen(path, "rb");
	u8 *data;
	long len;
	uiAssetsSource_t src;
	uiPosterHandle_t h;

	currentTest = "test_real_pack";
	if (!f) {
		fprintf(stderr, "FAIL cannot open real pack %s\n", path);
		failures++;
		return;
	}
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	data = malloc((size_t)len);
	if (fread(data, 1, (size_t)len, f) != (size_t)len) {
		fclose(f);
		free(data);
		failures++;
		return;
	}
	fclose(f);

	resetWorld();
	src = memSource(data, (size_t)len);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	/* fixture_pack.py writes GALE01 (universal), GC6E01, GM4E01. */
	requestOne("GALE01");
	pollAll();
	CHECK(UIAssets_Query("GALE01", 6, true, &h) == UI_POSTER_EXACT);
	CHECK(UIAssets_Peek(h) != NULL);
	CHECK(UIAssets_Query("GALE69", 6, true, &h) == UI_POSTER_UNIVERSAL);
	CHECK(UIAssets_Query("GC6E69", 6, true, &h) == UI_POSTER_USE_BNR);
	UIAssets_CancelForDeviceChange();
	CHECK(UIAssets_DisposeAfterVideoStop() == UI_ASSETS_OK);
	free(data);
	printf("  %-44s %s\n", "test_real_pack", failures ? "see above" : "ok");
}

static void test_exact_pack(const char *path, int idCount, char **gameIds) {
	FILE *f = fopen(path, "rb");
	u8 *data;
	long len;
	uiAssetsSource_t src;
	uiPosterHandle_t handle;
	char ids[UI_ASSETS_WINDOW][8] = {{0}};
	int i;

	currentTest = "test_exact_pack";
	if (!f || idCount < 1 || idCount > UI_ASSETS_WINDOW) {
		fprintf(stderr, "FAIL exact-pack needs a readable pack and 1..%d IDs\n",
		        UI_ASSETS_WINDOW);
		if (f)
			fclose(f);
		failures++;
		return;
	}
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	data = malloc((size_t)len);
	if (!data || fread(data, 1, (size_t)len, f) != (size_t)len) {
		fclose(f);
		free(data);
		failures++;
		return;
	}
	fclose(f);

	for (i = 0; i < idCount; i++) {
		if (strlen(gameIds[i]) != UI_ASSETS_ID_LEN) {
			fprintf(stderr, "FAIL exact-pack ID must be six bytes: %s\n",
			        gameIds[i]);
			free(data);
			failures++;
			return;
		}
		memcpy(ids[i], gameIds[i], UI_ASSETS_ID_LEN);
	}

	resetWorld();
	src = memSource(data, (size_t)len);
	CHECK(UIAssets_Init(&src, &testSync) == UI_ASSETS_OK);
	UIAssets_RequestWindow(ids, idCount, 0);
	pollAll();
	for (i = 0; i < idCount; i++) {
		CHECK(UIAssets_Query(ids[i], UI_ASSETS_ID_LEN, false, &handle) ==
		      UI_POSTER_EXACT);
		CHECK(UIAssets_Peek(handle) != NULL);
	}
	UIAssets_CancelForDeviceChange();
	CHECK(UIAssets_DisposeAfterVideoStop() == UI_ASSETS_OK);
	free(data);
	printf("  %-44s %s\n", "test_exact_pack", failures ? "see above" : "ok");
}

int main(int argc, char **argv) {
	int failuresBefore = 0;

	RUN(test_init_valid);
	RUN(test_exact_hit);
	RUN(test_universal_fallback);
	RUN(test_no_implicit_fallback);
	RUN(test_duplicate_and_unsorted_rejected);
	RUN(test_ambiguous_universal_rejected);
	RUN(test_missing_or_truncated_pack);
	RUN(test_header_field_corruption);
	RUN(test_malformed_record_offsets);
	RUN(test_index_crc_mismatch);
	RUN(test_byte_flip_fuzz);
	RUN(test_field_validation_behind_valid_crc);
	RUN(test_index_read_failure);
	RUN(test_poster_payload_corrupt);
	RUN(test_short_read_fails_poster);
	RUN(test_nth_id_boundaries);
	RUN(test_window_prefetch_loads_all_seven);
	RUN(test_window_eviction_and_reversal);
	RUN(test_max_records_boundary);
	RUN(test_stale_handle_after_eviction);
	RUN(test_pin_retention_through_detail_launch);
	RUN(test_grid_window_then_carousel);
	RUN(test_cancel_for_device_change);
	RUN(test_sync_validation_policy);
	RUN(test_sync_binding_lifecycle);
	RUN(test_dispose_requires_cancel);
	RUN(test_dispose_after_mutex_destroyed);
	RUN(test_dispose_releases_everything);
	RUN(test_quarantine_blocks_rewrite);
	RUN(test_scale_100_and_250);
	RUN(test_memory_budget);
	RUN(test_dominant_color);
	RUN(test_bad_ids_and_args);
	RUN(test_uninitialized_api_is_inert);
	RUN(test_poll_lock_phase_split);
	RUN(test_video_apis_never_lock);
	RUN(test_menu_apis_single_section);
	RUN(test_cancel_inside_read_drops_publication);
	RUN(test_reinit_inside_read_no_stale_publish);
	RUN(test_generation_u32_wrap);
	RUN(test_short_id_rejected_before_read);
	RUN(test_request_window_mid_read_no_stale_publish);
	RUN(test_acquire_pin_saturation);

	if (argc > 2 && strcmp(argv[1], "--exact-pack") == 0)
		test_exact_pack(argv[2], argc - 3, &argv[3]);
	else if (argc > 1)
		test_real_pack(argv[1]);

	resetWorld();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
