#ifndef INDIGO_LOG_H
#define INDIGO_LOG_H

#include <stddef.h>

/*
 * Every print_debug line, kept in memory: upstream sends them only to a USB
 * Gecko. The console lab serves them (GET /log, gui/lab.c). The first
 * INDIGO_LOG_HEAD_BYTES stay for good, so the start of the session always
 * survives; after them a ring keeps the latest INDIGO_LOG_TAIL_BYTES.
 *
 * Any thread may append at any time: a writer reserves its bytes with one
 * atomic add, so writers never share a byte and nobody waits. A copy taken
 * while another thread writes can show that line half written.
 */
#define INDIGO_LOG_HEAD_BYTES (16u * 1024u)
#define INDIGO_LOG_TAIL_BYTES (48u * 1024u)
/* What IndigoLog_Copy may need: both parts and the note between them. */
#define INDIGO_LOG_COPY_BYTES (INDIGO_LOG_HEAD_BYTES + INDIGO_LOG_TAIL_BYTES + 64u)

void IndigoLog_Append(const char *text, size_t length);

/* The log so far, NUL-terminated; a note marks what the ring let go.
 * Returns the length written. */
size_t IndigoLog_Copy(char *out, size_t capacity);

/* What was appended since *from (0: the start), up to capacity bytes and
 * ending on a line where one fits; *from moves past it. A *from the ring
 * has let go of gets IndigoLog_Copy's note instead and moves to the oldest
 * byte kept. Not NUL-terminated. Returns 0 when nothing is new, and while
 * an append is still being written: its bytes may not be there yet. */
size_t IndigoLog_Read(unsigned *from, char *out, size_t capacity);

/* Tests only: forget everything. */
void IndigoLog_Reset(void);

#endif
