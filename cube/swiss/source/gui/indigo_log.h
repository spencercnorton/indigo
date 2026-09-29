#ifndef INDIGO_LOG_H
#define INDIGO_LOG_H

#include <stddef.h>

/*
 * What Indigo notes as it runs, kept in memory until someone saves it
 * (System > System Information > About, X): every print_debug line, which
 * upstream sends only to a USB Gecko, and the startup stages. The first
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

/* What startup is doing, shown under the veil until the menu appears.
 * stage must stay valid (a string literal or a device's name); NULL clears
 * it. The serial moves on every change, so a reader can time each stage. */
void IndigoLog_SetStage(const char *stage);
const char *IndigoLog_Stage(unsigned *serial);

/* Tests only: forget everything. */
void IndigoLog_Reset(void);

#endif
