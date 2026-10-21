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

/* LogHost's sender reads in turn: the pieces add up to what Copy shows,
 * gap note included, each ends on a line where one fits, and then only what
 * is new comes. */
static void readsInPiecesAsCopyShowsIt(void)
{
	static char stream[INDIGO_LOG_COPY_BYTES];
	char piece[1024], line[32];
	unsigned at = 0, unended = 0;
	size_t length = 0, n;

	IndigoLog_Reset();
	CHECK(IndigoLog_Read(&at, piece, sizeof(piece)) == 0u);
	appendText("START\n");
	for(n = 0; n < 20000u; n++) {
		snprintf(line, sizeof(line), "line %05u\n", (unsigned)n);
		appendText(line);
	}
	while((n = IndigoLog_Read(&at, piece, sizeof(piece))) > 0u) {
		CHECK(length + n < sizeof(stream));
		memcpy(stream + length, piece, n);
		length += n;
		unended += piece[n - 1u] != '\n';
	}
	stream[length] = '\0';
	CHECK(unended == 1u);  /* the head's last piece: the gap note follows */
	CHECK(length == IndigoLog_Copy(copy, sizeof(copy)));
	CHECK(strcmp(stream, copy) == 0);
	appendText("after\n");
	CHECK(IndigoLog_Read(&at, piece, sizeof(piece)) == 6u);
	CHECK(memcmp(piece, "after\n", 6) == 0);
	CHECK(IndigoLog_Read(&at, piece, sizeof(piece)) == 0u);
}

/* A line longer than the buffer comes in pieces; a position past the end
 * reads nothing. */
static void aLongLineComesInPieces(void)
{
	char small[8];
	unsigned at = 0;

	IndigoLog_Reset();
	appendText("0123456789\n");
	CHECK(IndigoLog_Read(&at, small, sizeof(small)) == 8u && at == 8u);
	CHECK(memcmp(small, "01234567", 8) == 0);
	CHECK(IndigoLog_Read(&at, small, sizeof(small)) == 3u && at == 11u);
	CHECK(memcmp(small, "89\n", 3) == 0);
	at = 50u;
	CHECK(IndigoLog_Read(&at, small, sizeof(small)) == 0u && at == 50u);
	CHECK(IndigoLog_Read(NULL, small, sizeof(small)) == 0u);
}

int main(void)
{
	keepsWhatFits();
	keepsTheStartAndTheLatest();
	aShortBufferGetsAClippedCopy();
	readsInPiecesAsCopyShowsIt();
	aLongLineComesInPieces();
	printf("indigo log: %lu checks passed\n", checks);
	return 0;
}
