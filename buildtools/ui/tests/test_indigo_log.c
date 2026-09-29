#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "indigo_log.h"

static unsigned long checks;

#define CHECK(condition) do { \
	checks++; \
	if(!(condition)) { \
		fprintf(stderr, "check failed at %s:%d: %s\n", \
			__FILE__, __LINE__, #condition); \
		exit(1); \
	} \
} while(0)

static char copy[INDIGO_LOG_COPY_BYTES];

static void appendText(const char *text)
{
	IndigoLog_Append(text, strlen(text));
}

static void keepsWhatFits(void)
{
	IndigoLog_Reset();
	CHECK(IndigoLog_Copy(copy, sizeof(copy)) == 0u && copy[0] == '\0');
	appendText("[    1.000] first\n");
	appendText("[    1.250] second\n");
	CHECK(IndigoLog_Copy(copy, sizeof(copy)) == 37u);
	CHECK(strcmp(copy, "[    1.000] first\n[    1.250] second\n") == 0);
}

/* The start of the session survives any amount of later output, and the
 * newest output is always there: a line per number, far past both parts. */
static void keepsTheStartAndTheLatest(void)
{
	char line[32];
	unsigned n;
	size_t length;

	IndigoLog_Reset();
	appendText("START\n");
	for(n = 0; n < 20000u; n++) {
		snprintf(line, sizeof(line), "line %05u\n", n);
		appendText(line);
	}
	length = IndigoLog_Copy(copy, sizeof(copy));
	CHECK(length == strlen(copy));
	CHECK(strncmp(copy, "START\nline 00000\n", 17) == 0);
	CHECK(strstr(copy, "bytes not kept ...]\n") != NULL);
	CHECK(length < INDIGO_LOG_COPY_BYTES);
	CHECK(length > INDIGO_LOG_HEAD_BYTES + INDIGO_LOG_TAIL_BYTES - 1u);
	CHECK(strcmp(copy + length - 11, "line 19999\n") == 0);
	CHECK(strstr(copy, "line 10000\n") == NULL);  /* dropped between the parts */
}

static void aShortBufferGetsAClippedCopy(void)
{
	char small[8];

	IndigoLog_Reset();
	appendText("0123456789\n");
	CHECK(IndigoLog_Copy(small, sizeof(small)) == 7u);
	CHECK(strcmp(small, "0123456") == 0);
	CHECK(IndigoLog_Copy(small, 0u) == 0u);
	CHECK(IndigoLog_Copy(NULL, 8u) == 0u);
	IndigoLog_Append(NULL, 4u);
	IndigoLog_Append("x", 0u);
	CHECK(IndigoLog_Copy(copy, sizeof(copy)) == 11u);
}

static void stagesCountEveryChange(void)
{
	unsigned first, second;

	IndigoLog_Reset();
	CHECK(IndigoLog_Stage(&first) == NULL);
	IndigoLog_SetStage("Reading settings");
	CHECK(strcmp(IndigoLog_Stage(&second), "Reading settings") == 0);
	CHECK(second != first);
	IndigoLog_SetStage("Reading settings");
	CHECK(IndigoLog_Stage(&first) != NULL && first != second);
	IndigoLog_SetStage(NULL);
	CHECK(IndigoLog_Stage(NULL) == NULL);
}

int main(void)
{
	keepsWhatFits();
	keepsTheStartAndTheLatest();
	aShortBufferGetsAClippedCopy();
	stagesCountEveryChange();
	printf("indigo log: %lu checks passed\n", checks);
	return 0;
}
