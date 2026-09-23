#include "ui_gameflow_ownership.h"

bool UIGameflowOwnership_ReleaseChildren(size_t childCount,
	uiGameflowOwnershipReleaseFn releaseChild, void *context)
{
	size_t i;

	if(childCount != 0u && releaseChild == NULL) {
		return false;
	}
	for(i = 0u; i < childCount; ++i) {
		releaseChild(i, context);
	}
	return true;
}
