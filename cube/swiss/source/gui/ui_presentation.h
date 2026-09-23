#ifndef UI_PRESENTATION_H
#define UI_PRESENTATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UI_PRESENTATION_TITLE_CAPACITY 64u
#define UI_PRESENTATION_MESSAGE_CAPACITY 128u
#define UI_PRESENTATION_DETAIL_CAPACITY 96u
#define UI_PRESENTATION_ACTION_CAPACITY 64u

typedef enum {
	UI_PRESENTATION_EMPTY = 0,
	UI_PRESENTATION_LOADING,
	UI_PRESENTATION_RECOVERABLE_ERROR,
	UI_PRESENTATION_INFORMATION,
	UI_PRESENTATION_KIND_COUNT
} uiPresentationKind_t;

typedef enum {
	UI_PRESENTATION_INPUT_NONE = 0u,
	UI_PRESENTATION_INPUT_A = 1u << 0,
	UI_PRESENTATION_INPUT_B = 1u << 1
} uiPresentationInput_t;

/* Fixed, pointer-free payload shared by menu and video threads. Each field is
 * one authored line; presentation construction fits it once before publish. */
typedef struct {
	uiPresentationKind_t kind;
	char title[UI_PRESENTATION_TITLE_CAPACITY];
	char message[UI_PRESENTATION_MESSAGE_CAPACITY];
	char detail[UI_PRESENTATION_DETAIL_CAPACITY];
	char action[UI_PRESENTATION_ACTION_CAPACITY];
} uiPresentationSnapshot_t;

/* Copies every borrowed input and visibly ellipsizes bounded overflow. Title
 * and message are required. Loading is non-dismissible and accepts no action;
 * Empty, recoverable error, and information require an A/B action label. */
bool UIPresentation_Build(uiPresentationSnapshot_t *snapshot,
	uiPresentationKind_t kind, const char *title, const char *message,
	const char *detail, const char *action);

bool UIPresentation_Valid(const uiPresentationSnapshot_t *snapshot);
bool UIPresentation_Dismissible(const uiPresentationSnapshot_t *snapshot);
bool UIPresentation_AcceptsInput(const uiPresentationSnapshot_t *snapshot,
	uint32_t input);
const char *UIPresentation_KindLabel(uiPresentationKind_t kind);

#endif
