#include <string.h>

#include "ui_presentation.h"

#define UI_PRESENTATION_SOURCE_LIMIT 1024u

static bool kindValid(uiPresentationKind_t kind)
{
	return kind >= UI_PRESENTATION_EMPTY &&
		kind < UI_PRESENTATION_KIND_COUNT;
}

static bool sourceLength(const char *source, bool required, size_t *length)
{
	size_t i;

	if(length == NULL) {
		return false;
	}
	*length = 0u;
	if(source == NULL) {
		return !required;
	}
	for(i = 0u; i < UI_PRESENTATION_SOURCE_LIMIT; ++i) {
		unsigned char value = (unsigned char)source[i];

		if(value == 0u) {
			*length = i;
			return !required || i > 0u;
		}
		/* Every retained field is a single authored line. Reject hidden
		 * control characters instead of letting them alter panel layout. */
		if(value < 0x20u || value == 0x7fu) {
			return false;
		}
	}
	return false;
}

static bool copyText(char *destination, size_t capacity, const char *source,
	bool required)
{
	size_t length;
	size_t copyLength;

	if(destination == NULL || capacity == 0u ||
		!sourceLength(source, required, &length)) {
		return false;
	}
	destination[0] = '\0';
	if(source == NULL || length == 0u) {
		return true;
	}
	if(length < capacity) {
		memcpy(destination, source, length + 1u);
		return true;
	}
	if(capacity < 4u) {
		return false;
	}
	copyLength = capacity - 4u;
	memcpy(destination, source, copyLength);
	memcpy(&destination[copyLength], "...", 4u);
	return true;
}

static bool fieldValid(const char *text, size_t capacity, bool required)
{
	size_t i;

	if(text == NULL || capacity == 0u) {
		return false;
	}
	for(i = 0u; i < capacity; ++i) {
		unsigned char value = (unsigned char)text[i];

		if(value == 0u) {
			return !required || i > 0u;
		}
		if(value < 0x20u || value == 0x7fu) {
			return false;
		}
	}
	return false;
}

bool UIPresentation_Build(uiPresentationSnapshot_t *snapshot,
	uiPresentationKind_t kind, const char *title, const char *message,
	const char *detail, const char *action)
{
	uiPresentationSnapshot_t candidate;
	bool dismissible;

	if(snapshot == NULL) {
		return false;
	}
	memset(snapshot, 0, sizeof(*snapshot));
	memset(&candidate, 0, sizeof(candidate));
	if(!kindValid(kind)) {
		return false;
	}
	dismissible = kind != UI_PRESENTATION_LOADING;
	if((dismissible && (action == NULL || action[0] == '\0')) ||
		(!dismissible && action != NULL && action[0] != '\0')) {
		return false;
	}
	candidate.kind = kind;
	if(!copyText(candidate.title, sizeof(candidate.title), title, true) ||
		!copyText(candidate.message, sizeof(candidate.message), message, true) ||
		!copyText(candidate.detail, sizeof(candidate.detail), detail, false) ||
		!copyText(candidate.action, sizeof(candidate.action), action,
			dismissible)) {
		return false;
	}
	if(!UIPresentation_Valid(&candidate)) {
		return false;
	}
	*snapshot = candidate;
	return true;
}

bool UIPresentation_Valid(const uiPresentationSnapshot_t *snapshot)
{
	bool dismissible;

	if(snapshot == NULL || !kindValid(snapshot->kind)) {
		return false;
	}
	dismissible = snapshot->kind != UI_PRESENTATION_LOADING;
	return fieldValid(snapshot->title, sizeof(snapshot->title), true) &&
		fieldValid(snapshot->message, sizeof(snapshot->message), true) &&
		fieldValid(snapshot->detail, sizeof(snapshot->detail), false) &&
		fieldValid(snapshot->action, sizeof(snapshot->action), dismissible) &&
		(dismissible || snapshot->action[0] == '\0');
}

bool UIPresentation_Dismissible(const uiPresentationSnapshot_t *snapshot)
{
	return UIPresentation_Valid(snapshot) &&
		snapshot->kind != UI_PRESENTATION_LOADING;
}

bool UIPresentation_AcceptsInput(const uiPresentationSnapshot_t *snapshot,
	uint32_t input)
{
	const uint32_t dismiss = UI_PRESENTATION_INPUT_A |
		UI_PRESENTATION_INPUT_B;

	return UIPresentation_Dismissible(snapshot) && (input & dismiss) != 0u;
}

const char *UIPresentation_KindLabel(uiPresentationKind_t kind)
{
	switch(kind) {
		case UI_PRESENTATION_EMPTY:
			return "NOTHING HERE YET";
		case UI_PRESENTATION_LOADING:
			return "WORKING";
		case UI_PRESENTATION_RECOVERABLE_ERROR:
			return "NEEDS ATTENTION";
		case UI_PRESENTATION_INFORMATION:
			return "INFORMATION";
		default:
			return "";
	}
}
