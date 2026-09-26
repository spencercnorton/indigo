#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_presentation.h"

static unsigned int checks;

#define CHECK(condition) do { \
	++checks; \
	if(!(condition)) { \
		fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
			#condition); \
		exit(EXIT_FAILURE); \
	} \
} while(0)

static void checkEndsWithEllipsis(const char *text, size_t capacity)
{
	size_t length = strlen(text);

	CHECK(length == capacity - 1u);
	CHECK(length >= 3u);
	CHECK(strcmp(&text[length - 3u], "...") == 0);
}

static void testKindPolicies(void)
{
	uiPresentationSnapshot_t snapshot;

	CHECK(UIPresentation_Build(&snapshot, UI_PRESENTATION_LOADING,
		"Opening Game", "Checking this folder.", "Library stays ready.",
		NULL));
	CHECK(UIPresentation_Valid(&snapshot));
	CHECK(!UIPresentation_Dismissible(&snapshot));
	CHECK(!UIPresentation_AcceptsInput(&snapshot,
		UI_PRESENTATION_INPUT_A | UI_PRESENTATION_INPUT_B));
	CHECK(snapshot.action[0] == '\0');
	CHECK(strcmp(UIPresentation_KindLabel(snapshot.kind), "WORKING") == 0);

	CHECK(UIPresentation_Build(&snapshot, UI_PRESENTATION_EMPTY,
		"No Games Found", "This library has no supported titles.", NULL,
		"A / B  RETURN"));
	CHECK(UIPresentation_Dismissible(&snapshot));
	CHECK(UIPresentation_AcceptsInput(&snapshot, UI_PRESENTATION_INPUT_A));
	CHECK(UIPresentation_AcceptsInput(&snapshot, UI_PRESENTATION_INPUT_B));
	CHECK(!UIPresentation_AcceptsInput(&snapshot, UINT32_C(1) << 12));
	CHECK(strcmp(UIPresentation_KindLabel(snapshot.kind),
		"NOTHING HERE YET") == 0);

	CHECK(UIPresentation_Build(&snapshot,
		UI_PRESENTATION_RECOVERABLE_ERROR, "Can't Open This Game",
		"The image ID does not match.", "Nothing changed.",
		"A / B  RETURN TO LIBRARY"));
	CHECK(UIPresentation_AcceptsInput(&snapshot,
		UI_PRESENTATION_INPUT_A | (UINT32_C(1) << 30)));
	CHECK(strcmp(UIPresentation_KindLabel(snapshot.kind),
		"NEEDS ATTENTION") == 0);

	CHECK(UIPresentation_Build(&snapshot, UI_PRESENTATION_INFORMATION,
		"Poster Pack", "Retail art is unavailable.",
		"Swiss will use the built-in cover.", "A / B  CONTINUE"));
	CHECK(UIPresentation_Dismissible(&snapshot));
	CHECK(strcmp(UIPresentation_KindLabel(snapshot.kind),
		"INFORMATION") == 0);
	CHECK(strcmp(UIPresentation_KindLabel(UI_PRESENTATION_KIND_COUNT), "") == 0);
}

static void testPointerFreeCopyAndBounds(void)
{
	char title[] = "Borrowed Title";
	char message[] = "Borrowed message";
	char detail[] = "Borrowed detail";
	char action[] = "A / B  RETURN";
	char longTitle[160];
	char longMessage[256];
	char longDetail[192];
	char longAction[128];
	uiPresentationSnapshot_t snapshot;

	CHECK(UIPresentation_Build(&snapshot, UI_PRESENTATION_INFORMATION,
		title, message, detail, action));
	memset(title, 'x', sizeof(title) - 1u);
	memset(message, 'y', sizeof(message) - 1u);
	memset(detail, 'z', sizeof(detail) - 1u);
	memset(action, 'q', sizeof(action) - 1u);
	CHECK(strcmp(snapshot.title, "Borrowed Title") == 0);
	CHECK(strcmp(snapshot.message, "Borrowed message") == 0);
	CHECK(strcmp(snapshot.detail, "Borrowed detail") == 0);
	CHECK(strcmp(snapshot.action, "A / B  RETURN") == 0);

	memset(longTitle, 'T', sizeof(longTitle));
	memset(longMessage, 'M', sizeof(longMessage));
	memset(longDetail, 'D', sizeof(longDetail));
	memset(longAction, 'A', sizeof(longAction));
	longTitle[sizeof(longTitle) - 1u] = '\0';
	longMessage[sizeof(longMessage) - 1u] = '\0';
	longDetail[sizeof(longDetail) - 1u] = '\0';
	longAction[sizeof(longAction) - 1u] = '\0';
	CHECK(UIPresentation_Build(&snapshot, UI_PRESENTATION_INFORMATION,
		longTitle, longMessage, longDetail, longAction));
	checkEndsWithEllipsis(snapshot.title, sizeof(snapshot.title));
	checkEndsWithEllipsis(snapshot.message, sizeof(snapshot.message));
	checkEndsWithEllipsis(snapshot.detail, sizeof(snapshot.detail));
	checkEndsWithEllipsis(snapshot.action, sizeof(snapshot.action));
	CHECK(sizeof(snapshot) < 512u);
}

static void testInvalidInputsFailClosed(void)
{
	char unterminated[1024];
	uiPresentationSnapshot_t snapshot;

	memset(unterminated, 'x', sizeof(unterminated));
	memset(&snapshot, 0xff, sizeof(snapshot));
	CHECK(!UIPresentation_Build(NULL, UI_PRESENTATION_EMPTY, "Title",
		"Message", NULL, "A  RETURN"));
	CHECK(!UIPresentation_Build(&snapshot, UI_PRESENTATION_KIND_COUNT,
		"Title", "Message", NULL, "A  RETURN"));
	CHECK(snapshot.title[0] == '\0');
	CHECK(!UIPresentation_Build(&snapshot, UI_PRESENTATION_INFORMATION,
		NULL, "Message", NULL, "A  RETURN"));
	CHECK(!UIPresentation_Build(&snapshot, UI_PRESENTATION_INFORMATION,
		"Title", NULL, NULL, "A  RETURN"));
	CHECK(!UIPresentation_Build(&snapshot, UI_PRESENTATION_INFORMATION,
		"Title\nInjected", "Message", NULL, "A  RETURN"));
	CHECK(!UIPresentation_Build(&snapshot, UI_PRESENTATION_LOADING,
		"Title", "Message", NULL, "A  CANCEL"));
	CHECK(!UIPresentation_Build(&snapshot, UI_PRESENTATION_EMPTY,
		"Title", "Message", NULL, NULL));
	CHECK(!UIPresentation_Build(&snapshot, UI_PRESENTATION_INFORMATION,
		unterminated, "Message", NULL, "A  RETURN"));

	CHECK(UIPresentation_Build(&snapshot, UI_PRESENTATION_INFORMATION,
		"Title", "Message", NULL, "A  RETURN"));
	snapshot.kind = UI_PRESENTATION_KIND_COUNT;
	CHECK(!UIPresentation_Valid(&snapshot));
	snapshot.kind = UI_PRESENTATION_INFORMATION;
	memset(snapshot.action, 'x', sizeof(snapshot.action));
	CHECK(!UIPresentation_Valid(&snapshot));
	CHECK(!UIPresentation_AcceptsInput(&snapshot,
		UI_PRESENTATION_INPUT_A));
}

int main(void)
{
	testKindPolicies();
	testPointerFreeCopyAndBounds();
	testInvalidInputsFailClosed();
	printf("ui_presentation: %u checks passed\n", checks);
	return EXIT_SUCCESS;
}
