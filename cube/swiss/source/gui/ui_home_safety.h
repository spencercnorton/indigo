#ifndef UI_HOME_SAFETY_H
#define UI_HOME_SAFETY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
	UI_HOME_SOURCE_MOUNT_UNKNOWN = 0,
	UI_HOME_SOURCE_MOUNT_ABSENT,
	UI_HOME_SOURCE_MOUNT_UNMOUNTED,
	UI_HOME_SOURCE_MOUNT_MOUNTED
} uiHomeSourceMountState_t;

typedef struct {
	const void *handler;
	uiHomeSourceMountState_t state;
} uiHomeSourceLifecycle_t;

void UIHomeSafety_RecordSource(uiHomeSourceLifecycle_t *lifecycle,
	const void *handler, uiHomeSourceMountState_t state);
bool UIHomeSafety_SourceMounted(const uiHomeSourceLifecycle_t *lifecycle,
	const void *currentHandler);
bool UIHomeSafety_SourceReady(const uiHomeSourceLifecycle_t *lifecycle,
	const void *currentHandler, bool available);
bool UIHomeSafety_ShouldForceRecentInit(
	const uiHomeSourceLifecycle_t *lifecycle, const void *currentHandler,
	const void *targetHandler, bool available);

bool UIHomeSafety_SelectorReleasePending(uint32_t heldButtons,
	uint32_t selectorButtons, int stickX, int stickY, int deadzone);
bool UIHomeSafety_RestartReleasePending(uint32_t heldButtons,
	uint32_t confirmationButtons);
bool UIHomeSafety_AccumulateRestartCancel(bool cancelRequested,
	uint32_t heldButtons, uint32_t backButton);

#endif
