#ifndef UI_GAMEFLOW_RESOLVER_H
#define UI_GAMEFLOW_RESOLVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui_gameflow_library.h"

#define UI_GAMEFLOW_RESOLVER_ID_LENGTH 6u
#define UI_GAMEFLOW_RESOLVER_NAME_CAPACITY 256u
#define UI_GAMEFLOW_RESOLVER_NO_SOURCE UINT32_MAX

typedef enum {
	UI_GAMEFLOW_RESOLVE_OK = 0,
	UI_GAMEFLOW_RESOLVE_INVALID_ARGUMENT,
	UI_GAMEFLOW_RESOLVE_NO_IMAGE,
	UI_GAMEFLOW_RESOLVE_AMBIGUOUS,
	UI_GAMEFLOW_RESOLVE_ID_MISMATCH,
	UI_GAMEFLOW_RESOLVE_INVALID_METADATA
} uiGameflowResolveStatus_t;

/* Fixed-copy input for one immediate child of a strict game folder. The
 * caller supplies only the leaf name, never a borrowed file_handle pointer.
 * headerValid means gameId/discNumber/version came from a validated image
 * header. sourceIndex is the caller-owned private child-array index. */
typedef struct {
	uiGameflowLibraryEntryType_t type;
	uint32_t sourceIndex;
	char name[UI_GAMEFLOW_RESOLVER_NAME_CAPACITY];
	char gameId[UI_GAMEFLOW_RESOLVER_ID_LENGTH + 1u];
	bool headerValid;
	uint8_t discNumber;
	uint8_t version;
	uint8_t discCount;
} uiGameflowResolverEntry_t;

typedef struct {
	char gameId[UI_GAMEFLOW_RESOLVER_ID_LENGTH + 1u];
} uiGameflowResolverFolder_t;

/* Pointer-free result: source indices remain valid only while the caller owns
 * its private child array. An optional opposite disc always has the same
 * six-byte ID and version as the primary and a 0/1 disc number opposite it. */
typedef struct {
	uint32_t primarySourceIndex;
	uint32_t oppositeDiscSourceIndex;
	char gameId[UI_GAMEFLOW_RESOLVER_ID_LENGTH + 1u];
	bool hasOppositeDisc;
	uint8_t primaryDiscNumber;
	uint8_t version;
} uiGameflowResolverResult_t;

/* Resolve a strict "Title [GAMEID]" folder without I/O or allocation.
 *
 * Policy:
 * - ignore parents, directories, special entries, and unsupported files;
 * - reject malformed/invalid supported images and any folder/image ID mix;
 * - prefer one unambiguous case-insensitive game.<fdi|gcm|iso|tgc>;
 * - otherwise accept one image, or one disc 0 plus at most one matching disc 1;
 * - reject every remaining ambiguous layout.
 *
 * entries may be NULL only when entryCount is zero. result is reset to the
 * NO_SOURCE sentinel on every call for which it is non-NULL. */
uiGameflowResolveStatus_t UIGameflowResolver_Resolve(
	const uiGameflowResolverFolder_t *folder,
	const uiGameflowResolverEntry_t *entries, size_t entryCount,
	uiGameflowResolverResult_t *result);

/* Header-only opposite-disc predicate for borrowed flattened entries. It
 * performs no metadata allocation and requires matching ID/version plus the
 * other 0/1 disc number. */
bool UIGameflowResolver_IsOppositeDisc(
	const uiGameflowResolverEntry_t *primary,
	const uiGameflowResolverEntry_t *candidate);

#endif
