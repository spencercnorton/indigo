#ifndef UI_ASSETS_H
#define UI_ASSETS_H

/*
 * ui_assets -- bounded poster cache for the future persistent game-flow
 * renderer (Phase 4A: service + tests only, nothing draws yet).
 *
 * Serves 192x256 poster content from an offline-generated
 * /swiss/ui/posters.pak (format: docs/ui-redesign/POSTER_PACK_FORMAT.md).
 * Posters are stored on a 256x256 GX_TF_CMPR canvas with a 5-level mip
 * chain because GX requires power-of-two dimensions for mipmapping; the
 * renderer samples s in [0, UI_ASSETS_CONTENT_W / UI_ASSETS_CANVAS_W].
 *
 * SYNCHRONIZATION CONTRACT (mirrors ui_gameflow: one external critical
 * section, in practice FrameBufferMagic's _videomutex, supplied by the
 * retained-event wrapper as lock/unlock callbacks at Init):
 *
 *   Menu-thread APIs -- Init, RequestWindow, Poll, Acquire, Release, and
 *   CancelForDeviceChange -- must be called WITHOUT the lock
 *   held; they enter the critical section internally for every metadata
 *   mutation and publication. Poll is split so its bounded device read,
 *   CRC, and cache flush run OUTSIDE the lock; only job capture and final
 *   GXTexObj/failure publication run inside it. The lock is never held
 *   across source.read, CRC over a poster, allocation, or free.
 *
 *   Video-thread APIs -- Query, Peek, DominantColor -- are strictly
 *   read-only and must be called WITH the same lock already held by the
 *   caller (the EV_GAMEFLOW render pass owns it); they never lock, so a
 *   non-recursive mutex is safe. Pointers they return (GXTexObj, or
 *   anything derived from the pack index) must not be retained after the
 *   caller releases the lock, except a texture pointer that the menu
 *   thread has pinned via Acquire, which stays valid until Release plus
 *   eviction, CancelForDeviceChange, or final disposal.
 *
 *   Target Init requires both callbacks. A NULL sync (or an all-zero
 *   callback pair) degrades to no synchronization only in single-threaded
 *   host unit tests; a half-pair is rejected in every build. The exact
 *   lock/unlock/context triple is bound by the first successful Init,
 *   survives CancelForDeviceChange, and cannot change until final disposal.
 *
 * Lifetime rules enforced by that contract:
 *   - The pack index is only freed after it is unpublished inside the
 *     critical section, so no locked reader can ever observe freed memory
 *     (fixes the Query/DominantColor vs CancelForDeviceChange UAF).
 *   - Slot recordPos/generation/state and every GXTexObj are only written
 *     inside the critical section (fixes the RequestWindow/Poll
 *     publication race). The 40 ms eviction quarantine remains a separate
 *     guarantee for GPU texel reuse; the lock protects CPU metadata.
 *   - Handle generations are u32 and increment exactly once per new slot
 *     ownership; a stale handle yields NULL, never a dangling pointer.
 *   - CancelForDeviceChange must run while the video mutex is still live,
 *     before the device is deinitialized. DisposeAfterVideoStop is a
 *     separate lock-free final step after the video thread has joined; it
 *     is safe even if the mutex has already been destroyed.
 *
 * Integration sketch for the persistent EV_GAMEFLOW renderer:
 *
 *   // once, when the retained event is created (menu thread):
 *   uiAssetsSync_t sync;
 *   UIAssets_SyncFromMutex(videoMutex, &sync);  // the _videomutex owner
 *   UIAssets_Init(&source, &sync);
 *   // menu thread, selection moved (WITHOUT the lock):
 *   UIAssets_RequestWindow(ids, count, selectedPos);
 *   // menu thread, once per tick while UIAssets_Poll() returns true:
 *   UIAssets_Poll();
 *   // video thread, per frame per visible card (lock ALREADY HELD):
 *   uiPosterHandle_t h;
 *   switch (UIAssets_Query(id, 6, meta->banner != NULL, &h)) {
 *   case UI_POSTER_EXACT:
 *   case UI_POSTER_UNIVERSAL: {
 *       GXTexObj *tex = UIAssets_Peek(h);   // NULL while still loading
 *       if (tex) drawPosterCard(tex);       // s in [0, 0.75] of the canvas
 *       else drawBnrCard();                 // banner until the poster lands
 *       break; }
 *   case UI_POSTER_USE_BNR:         drawBnrCard(); break;
 *   case UI_POSTER_PROCEDURAL_CARD: drawTintedCard(); break;  // DominantColor
 *   case UI_POSTER_CORRUPT_OR_UNAVAILABLE: drawBnrCard(); break;
 *   }
 *   // entering Game Detail (menu thread, WITHOUT the lock):
 *   GXTexObj *kept = UIAssets_Acquire(h);   // Release(h) on the way back
 *   // before load_game()/device deinit (menu thread):
 *   UIAssets_CancelForDeviceChange();       // every handle now stale
 *   // final teardown, after the video thread has stopped:
 *   UIAssets_DisposeAfterVideoStop();        // lock-free; releases arena
 */

#include <stddef.h>

#ifdef UI_ASSETS_HOST_BUILD

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int16_t s16;
typedef int32_t s32;

/* Host stand-in capturing GX_InitTexObj* arguments for test assertions. */
typedef struct {
	void *data;
	u16 width;
	u16 height;
	u8 format;
	u8 mipmap;
	float minlod;
	float maxlod;
	u8 lodConfigured;
} GXTexObj;

#else
#include <gccore.h>
#endif

#include <stdbool.h>

#define UI_ASSETS_WINDOW 7
#define UI_ASSETS_CANVAS_W 256
#define UI_ASSETS_CANVAS_H 256
#define UI_ASSETS_CONTENT_W 192
#define UI_ASSETS_CONTENT_H 256
#define UI_ASSETS_MIP_LEVELS 5
#define UI_ASSETS_POSTER_BYTES 43648
#define UI_ASSETS_MAX_RECORDS 1024
#define UI_ASSETS_ID_LEN 6
/* One video frame must pass between evicting a slot and rewriting its
 * texels so the GPU never samples a half-written poster; 40 ms covers a
 * 50 Hz frame with margin. This guards GPU texel reuse -- the external
 * critical section guards CPU metadata. */
#define UI_ASSETS_EVICT_QUARANTINE_MS 40

#define UI_ASSETS_OK 0
#define UI_ASSETS_ERR_IO (-1)
#define UI_ASSETS_ERR_FORMAT (-2)
#define UI_ASSETS_ERR_CRC (-3)
#define UI_ASSETS_ERR_NOMEM (-4)
#define UI_ASSETS_ERR_STATE (-5)

typedef enum {
	UI_POSTER_EXACT = 0,
	UI_POSTER_UNIVERSAL,
	UI_POSTER_USE_BNR,
	UI_POSTER_PROCEDURAL_CARD,
	UI_POSTER_CORRUPT_OR_UNAVAILABLE
} uiPosterResult_t;

/* Value token; never a pointer. generation is u32 and advances exactly
 * once per new slot ownership, so a collision needs 2^32 assignment
 * cycles of one slot while the caller retains the handle. */
typedef struct {
	u32 generation;
	u16 slot;
	u16 reserved;
} uiPosterHandle_t;

/*
 * External critical section (in practice the video mutex). lock/unlock
 * must be non-recursive-safe as used here: menu APIs call them in
 * strictly paired, non-nested fashion; video APIs never call them.
 */
typedef struct {
	void (*lock)(void *ctx);
	void (*unlock)(void *ctx);
	void *ctx;
} uiAssetsSync_t;

/*
 * Data source abstraction. On target wrap a file_handle with
 * UIAssets_SourceFromFileHandle; host tests inject fakes.
 * read() returns bytes read (== len on success) or negative on error; it
 * is always invoked OUTSIDE the critical section (from Init before
 * publication, and from Poll's unlocked phase).
 * nowMs must be monotonic; NULL selects the target timebase (host builds
 * must inject one).
 */
typedef struct {
	s32 (*read)(void *ctx, u32 offset, void *dst, u32 len);
	u32 size;
	void *ctx;
	u32 (*nowMs)(void);
} uiAssetsSource_t;

/* Menu thread, lock NOT held. Reads and fully validates header+index
 * (CRC-checked before any poster data is read) into staging memory, then
 * publishes the new pack inside the critical section. Idempotent across
 * CancelForDeviceChange: the poster arena and bound sync survive while the
 * index is rebuilt. Re-Init must pass the identical lock/unlock/context
 * triple. sync may be NULL only for single-threaded host-test use; target
 * builds require both callbacks, and every build rejects a half-pair. */
s32 UIAssets_Init(const uiAssetsSource_t *source, const uiAssetsSync_t *sync);

/* Menu thread only, lock NOT held. Do not call concurrently with a menu
 * mutation or from the video thread; Query is the render-side classifier. */
bool UIAssets_Ready(void);

/* Menu thread, lock NOT held. ids: count entries of at least 7 bytes each
 * (6-char ID, NUL-terminated); count <= UI_ASSETS_WINDOW; selected indexes
 * into ids. Slots already holding a requested poster are kept; others are
 * evicted unless pinned. Loading proceeds selected-outward. */
void UIAssets_RequestWindow(const char (*ids)[8], int count, int selected);

/* Menu thread, lock NOT held. At most one bounded read + CRC per call,
 * performed outside the critical section; publication of the resulting
 * texture or failure re-enters it and revalidates slot ownership first
 * (a job captured before CancelForDeviceChange/Init/eviction can never
 * publish afterwards). Returns true while more loading work is queued. */
bool UIAssets_Poll(void);

/* Video thread, lock ALREADY HELD by the caller; never locks. Classifies
 * gameId (gameIdLen is the number of bytes readable at gameId; inputs
 * shorter than UI_ASSETS_ID_LEN are rejected before any read, and only
 * the first UI_ASSETS_ID_LEN bytes are examined). *out receives a handle
 * only when a cache slot is currently assigned to the resolved record;
 * a record that exists but has not been windowed in yet leaves *out at
 * the invalid sentinel, so re-Query after RequestWindow/Poll rather than
 * caching an early handle. bnrAvailable tells the classifier whether the
 * caller can fall back to opening.bnr art. */
uiPosterResult_t UIAssets_Query(const char *gameId, size_t gameIdLen,
                                bool bnrAvailable, uiPosterHandle_t *out);

/* Video thread, lock ALREADY HELD; never locks. Per-frame borrow: the
 * pointer is valid only until the caller releases the lock (unless the
 * menu thread holds a pin on the same slot). NULL while loading, after
 * eviction/cancel/final disposal, or on a stale generation. */
GXTexObj *UIAssets_Peek(uiPosterHandle_t handle);

/* Menu thread, lock NOT held (locks internally; do NOT call from inside
 * the video pass -- the mutex is not recursive). Pins the poster so it
 * survives window shifts (Library -> Detail -> Launch). Returns the
 * texture or NULL if stale/not ready. Pair with Release. Pinned slots
 * still go stale on CancelForDeviceChange/final disposal. */
GXTexObj *UIAssets_Acquire(uiPosterHandle_t handle);
void UIAssets_Release(uiPosterHandle_t handle);

/* Video thread, lock ALREADY HELD; never locks. Pack-declared dominant
 * color for procedural cards. gameIdLen as for Query. False when the ID
 * has no pack record (caller picks its own tint). */
bool UIAssets_DominantColor(const char *gameId, size_t gameIdLen,
                            u8 *r, u8 *g, u8 *b);

/* Menu thread, lock NOT held. Unpublishes the index and invalidates every
 * slot and handle (pinned included) inside the critical section, then
 * frees the index outside it -- no locked reader can ever observe freed
 * memory. Poster texels stay resident so a frame already in flight
 * samples stale-but-intact data. Call before the device is deinitialized;
 * re-Init afterwards to resume. */
void UIAssets_CancelForDeviceChange(void);

/* Final teardown, ONLY after CancelForDeviceChange has completed with the
 * video mutex still live AND the video thread has stopped (no concurrent
 * readers or menu operations can exist). This function never invokes the
 * bound lock/unlock callbacks, so the mutex may already be destroyed. It
 * returns UI_ASSETS_ERR_STATE without changing anything if a pack/index is
 * still published; otherwise it frees the arena and clears source/cache/
 * sync state. */
s32 UIAssets_DisposeAfterVideoStop(void);

/* Menu thread only, lock NOT held; do not call concurrently with mutation.
 * Total bytes held (arena + index copy); 0 before Init/after disposal. */
u32 UIAssets_MemoryFootprint(void);

#ifdef UI_ASSETS_HOST_BUILD
/* Host-test seam: force a slot's generation counter (e.g. to the wrap
 * boundary). Not compiled for the target. */
void UIAssetsTest_ForceGeneration(u16 slot, u32 generation);
#else
#include "deviceHandler.h"
/* Wrap an open, stat-able file_handle (menu thread only). */
s32 UIAssets_SourceFromFileHandle(file_handle *file, uiAssetsSource_t *out);
/* Bind an existing non-recursive LWP mutex (e.g. the video mutex) as the
 * external critical section. */
void UIAssets_SyncFromMutex(mutex_t mutex, uiAssetsSync_t *out);
#endif

#endif
