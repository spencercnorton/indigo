#ifndef UI_GAMEFLOW_OWNERSHIP_H
#define UI_GAMEFLOW_OWNERSHIP_H

#include <stdbool.h>
#include <stddef.h>

typedef void (*uiGameflowOwnershipReleaseFn)(size_t index, void *context);

/* Visit each private immediate child exactly once. The root folder is
 * deliberately absent from this API, preventing a caller from accidentally
 * closing/freeing the borrowed root-directory entry. */
bool UIGameflowOwnership_ReleaseChildren(size_t childCount,
	uiGameflowOwnershipReleaseFn releaseChild, void *context);

#endif
