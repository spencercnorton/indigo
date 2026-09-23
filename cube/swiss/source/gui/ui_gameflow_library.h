#ifndef UI_GAMEFLOW_LIBRARY_H
#define UI_GAMEFLOW_LIBRARY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui_gameflow.h"

#define UI_GAMEFLOW_LIBRARY_WINDOW 7u

typedef enum {
	UI_GAMEFLOW_LIBRARY_NONE = 0,
	UI_GAMEFLOW_LIBRARY_IMAGE_FILES,
	UI_GAMEFLOW_LIBRARY_GAME_FOLDERS
} uiGameflowLibraryMode_t;

typedef enum {
	UI_GAMEFLOW_LIBRARY_ENTRY_OTHER = 0,
	UI_GAMEFLOW_LIBRARY_ENTRY_SPECIAL,
	UI_GAMEFLOW_LIBRARY_ENTRY_FILE,
	UI_GAMEFLOW_LIBRARY_ENTRY_DIRECTORY
} uiGameflowLibraryEntryType_t;

typedef enum {
	UI_GAMEFLOW_LIBRARY_LOCATION_NONE = 0,
	UI_GAMEFLOW_LIBRARY_LOCATION_ROOT,
	UI_GAMEFLOW_LIBRARY_LOCATION_STRICT_LEAF
} uiGameflowLibraryLocation_t;

typedef struct {
	uiGameflowLibraryLocation_t location;
	uiGameflowLibraryMode_t mode;
	uint32_t entryCount;
	bool valid;
	bool hasGame;
} uiGameflowLibraryClassifier_t;

typedef struct {
	uint32_t index;
	int8_t relativeSlot;
} uiGameflowLibraryWindowSlot_t;

typedef enum {
	UI_GAMEFLOW_LIBRARY_ART_POSTER = 0,
	UI_GAMEFLOW_LIBRARY_ART_BANNER,
	UI_GAMEFLOW_LIBRARY_ART_EMBLEM
} uiGameflowLibraryArtwork_t;

/* Normalized authored fallback-card layout. The renderer maps these values
 * through the current perspective quad, keeping BNR and procedural cards on
 * the same full-card composition at every carousel pose. */
typedef struct {
	float contentInset;
	float bannerLeft;
	float bannerTop;
	float bannerRight;
	float bannerBottom;
	float motifCenterX;
	float motifCenterY;
	float motifRadius;
	float identityTop;
	float idBaseline;
	float regionBaseline;
	float identityAlpha;
} uiGameflowLibraryFallbackLayout_t;

bool UIGameflowLibrary_IsGameImageName(const char *name);

/*
 * Parse an exact "Title [ABC123]" leaf name. IDs are deliberately limited to
 * six uppercase ASCII letters/digits so an arbitrary directory cannot enter
 * the game-only surface. Output buffers may be NULL when only validating.
 */
bool UIGameflowLibrary_ParseGameFolderName(const char *name, char gameId[7],
	char *title, size_t titleSize);

/* Optional parent is permitted only at index zero; every other entry must
 * match the homogeneous mode selected by the caller. */
bool UIGameflowLibrary_EntryEligible(uiGameflowLibraryMode_t mode,
	uint32_t index, uiGameflowLibraryEntryType_t type, const char *name);

/* Pure path + post-scan classification seam used by production dispatch and
 * the legacy-profile regression. The classifier consumes the exact sorted
 * entries that Swiss will render, including an optional parent at index 0. */
uiGameflowLibraryLocation_t UIGameflowLibrary_Locate(
	const char *gamesRoot, const char *currentPath);

/* Home's B LIBRARY route may promote an exact root /games directory from an
 * already-scanned device root. Files, lookalike names, and deeper paths are
 * rejected before production copies a directory handle. */
bool UIGameflowLibrary_IsGamesRootEntry(const char *gamesRoot,
	uiGameflowLibraryEntryType_t type, const char *entryPath);

/* Explicit Autoload and Recent=On requests retain Swiss's existing direct
 * startup behavior. Normal, Lazy, and empty-Recent boots reveal Home. */
bool UIGameflowLibrary_ShouldStartHome(bool hasAutoload, bool hasRecent,
	bool recentAutoEnabled);

void UIGameflowLibrary_ClassifierInit(uiGameflowLibraryClassifier_t *state,
	uiGameflowLibraryLocation_t location);
bool UIGameflowLibrary_ClassifierAdd(uiGameflowLibraryClassifier_t *state,
	uiGameflowLibraryEntryType_t type, const char *name);
uiGameflowLibraryMode_t UIGameflowLibrary_ClassifierFinish(
	const uiGameflowLibraryClassifier_t *state);

/* Eligible strict libraries always use the retained presentation. Unknown or
 * mixed layouts preserve the caller's requested legacy browser. Browser
 * values remain opaque here so this pure policy is host-testable. */
int UIGameflowLibrary_SelectBrowser(uiGameflowLibraryMode_t mode,
	int requestedBrowser, int retainedBrowser);

/* Both supported library layouts own Game Detail: strict roots expose folder
 * cards, while Swiss's default games-directory flattening exposes their child
 * images directly. */
bool UIGameflowLibrary_UsesRetainedDetail(uiGameflowLibraryMode_t mode,
	uiGameflowLibraryEntryType_t type);

/* A ready retail cover always wins. Missing, corrupt, or still-loading cover
 * data falls back to a valid BNR and then to the code-native emblem. */
uiGameflowLibraryArtwork_t UIGameflowLibrary_ChooseArtwork(
	bool posterReady, bool bannerReady);

/* Build the near-card fallback composition. Returns false for non-finite-like
 * or far slots that the renderer deliberately does not decorate. */
bool UIGameflowLibrary_BuildFallbackLayout(float visualSlot,
	uiGameflowLibraryFallbackLayout_t *layout);

/* Derive a short, presentation-only video-region label from a validated
 * six-character GameCube ID. Unknown or incomplete IDs stay truthful. */
const char *UIGameflowLibrary_RegionLabel(const char *gameId);

/* Selected, -1, +1, -2, +2, -3, +3 with duplicate indices removed.
 * On a two-item ring, the sole neighbor is oriented to the side implied by
 * directionHint so relativeSlot + carouselTravel keeps the old card centered. */
size_t UIGameflowLibrary_BuildWindow(uint32_t itemCount,
	uint32_t selectedIndex, uiGameflowDirection_t directionHint,
	uiGameflowLibraryWindowSlot_t slots[7]);

#endif
