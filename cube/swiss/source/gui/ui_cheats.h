#ifndef UI_CHEATS_H
#define UI_CHEATS_H

#include <stdbool.h>
#include <stddef.h>

#define UI_CHEATS_VISIBLE_ROWS 6
#define UI_CHEATS_TEXT_CAPACITY 128u
#define UI_CHEATS_SOURCE_LIMIT 1023u

typedef bool (*uiCheatsEnabledFn)(int index, const void *context);
typedef int (*uiCheatsMeasureFn)(const char *text);

/* Display copies only: these helpers never alter catalog identity or flags. */
void UICheats_GameTitle(char *out, size_t capacity, const char *source);
void UICheats_Name(char *out, size_t capacity, const char *source);
void UICheats_Fit(char *out, size_t capacity, const char *source,
    int maxWidth, float scale, uiCheatsMeasureFn measure);
void UICheats_Wrap(char lines[3][UI_CHEATS_TEXT_CAPACITY],
    const char *source, int maxWidth, float scale, uiCheatsMeasureFn measure);

int UICheats_Count(int total, bool enabledOnly, uiCheatsEnabledFn enabled,
    const void *context);
int UICheats_Index(int total, bool enabledOnly, int visibleIndex,
    uiCheatsEnabledFn enabled, const void *context);
/* Select the same underlying entry, or the next survivor after disabling it. */
int UICheats_Preserve(int total, bool enabledOnly, int underlyingIndex,
    uiCheatsEnabledFn enabled, const void *context);
int UICheats_Move(int selected, int count, int direction, bool page);
int UICheats_WindowStart(int selected, int count);

#endif
