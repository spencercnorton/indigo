#include <stddef.h>

#include "ui_home_safety.h"

void UIHomeSafety_RecordSource(uiHomeSourceLifecycle_t *lifecycle,
	const void *handler, uiHomeSourceMountState_t state)
{
	if(lifecycle == NULL) {
		return;
	}
	lifecycle->handler = handler;
	lifecycle->state = handler != NULL ? state :
		UI_HOME_SOURCE_MOUNT_ABSENT;
}

bool UIHomeSafety_SourceMounted(const uiHomeSourceLifecycle_t *lifecycle,
	const void *currentHandler)
{
	return lifecycle != NULL &&
		lifecycle->state == UI_HOME_SOURCE_MOUNT_MOUNTED &&
		lifecycle->handler != NULL &&
		lifecycle->handler == currentHandler;
}

bool UIHomeSafety_SourceReady(const uiHomeSourceLifecycle_t *lifecycle,
	const void *currentHandler, bool available)
{
	return available &&
		UIHomeSafety_SourceMounted(lifecycle, currentHandler);
}

bool UIHomeSafety_ShouldForceRecentInit(
	const uiHomeSourceLifecycle_t *lifecycle, const void *currentHandler,
	const void *targetHandler, bool available)
{
	return targetHandler != NULL && targetHandler == currentHandler &&
		!UIHomeSafety_SourceReady(lifecycle, currentHandler, available);
}

bool UIHomeSafety_SelectorReleasePending(uint32_t heldButtons,
	uint32_t selectorButtons, int stickX, int stickY, int deadzone)
{
	if((heldButtons & selectorButtons) != 0u) {
		return true;
	}
	if(deadzone <= 0) {
		return true;
	}
	return stickX <= -deadzone || stickX >= deadzone ||
		stickY <= -deadzone || stickY >= deadzone;
}

bool UIHomeSafety_RestartReleasePending(uint32_t heldButtons,
	uint32_t confirmationButtons)
{
	return (heldButtons & confirmationButtons) != 0u;
}

bool UIHomeSafety_AccumulateRestartCancel(bool cancelRequested,
	uint32_t heldButtons, uint32_t backButton)
{
	return cancelRequested || (heldButtons & backButton) != 0u;
}
