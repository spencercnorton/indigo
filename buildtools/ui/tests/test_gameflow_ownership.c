/*
 * Host test command (run from repository root):
 * cc -std=c11 -Wall -Wextra -Werror -Wconversion -Wsign-conversion \
 *   -pedantic -Icube/swiss/source/gui \
 *   buildtools/ui/tests/test_gameflow_ownership.c \
 *   cube/swiss/source/gui/ui_gameflow_ownership.c \
 *   -o /tmp/test_gameflow_ownership && /tmp/test_gameflow_ownership
 */

#include <stdio.h>
#include <stdlib.h>

#include "ui_gameflow_ownership.h"

#define CHECK(condition) do { \
	if(!(condition)) { \
		fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
			#condition); \
		exit(EXIT_FAILURE); \
	} \
} while(0)

typedef struct {
	unsigned int rootMarker;
	unsigned int released[8];
	unsigned int *payload[8];
} fakeSession_t;

static void releaseChild(size_t index, void *opaque)
{
	fakeSession_t *session = opaque;

	CHECK(session != NULL);
	CHECK(index < 8u);
	CHECK(session->payload[index] != NULL);
	CHECK(*session->payload[index] == (unsigned int)(index + 100u));
	CHECK(session->released[index] == 0u);
	++session->released[index];
	free(session->payload[index]);
	session->payload[index] = NULL;
}

int main(void)
{
	fakeSession_t session = {.rootMarker = 0x47524f4fu};
	size_t i;

	for(i = 0u; i < 8u; ++i) {
		session.payload[i] = malloc(sizeof(*session.payload[i]));
		CHECK(session.payload[i] != NULL);
		*session.payload[i] = (unsigned int)(i + 100u);
	}
	CHECK(UIGameflowOwnership_ReleaseChildren(8u, releaseChild, &session));
	for(i = 0u; i < 8u; ++i) {
		CHECK(session.released[i] == 1u);
		CHECK(session.payload[i] == NULL);
	}
	CHECK(session.rootMarker == 0x47524f4fu);
	CHECK(UIGameflowOwnership_ReleaseChildren(0u, NULL, NULL));
	CHECK(!UIGameflowOwnership_ReleaseChildren(1u, NULL, &session));
	puts("gameflow ownership tests passed");
	return EXIT_SUCCESS;
}
