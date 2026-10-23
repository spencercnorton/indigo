#include <stdio.h>
#include <string.h>

#include "indigo_log.h"

static char head[INDIGO_LOG_HEAD_BYTES];
static char tail[INDIGO_LOG_TAIL_BYTES];
static unsigned written;          /* bytes ever appended */
static unsigned committed;        /* bytes whose appends have finished */

#define INDIGO_LOG_GAP "\n[... %u bytes not kept ...]\n"

void IndigoLog_Append(const char *text, size_t length)
{
	unsigned at;

	if(text == NULL || length == 0u) return;
	at = __atomic_fetch_add(&written, (unsigned)length, __ATOMIC_RELAXED);
	for(size_t i = 0; i < length; i++, at++) {
		if(at < INDIGO_LOG_HEAD_BYTES) head[at] = text[i];
		else tail[(at - INDIGO_LOG_HEAD_BYTES) % INDIGO_LOG_TAIL_BYTES] = text[i];
	}
	__atomic_fetch_add(&committed, (unsigned)length, __ATOMIC_RELEASE);
}

size_t IndigoLog_Copy(char *out, size_t capacity)
{
	unsigned total = __atomic_load_n(&written, __ATOMIC_RELAXED);
	unsigned inTail, start;
	size_t length = 0;

	if(out == NULL || capacity == 0u) return 0;
#define PUT(bytes, count) do { size_t n_ = (count); \
		if(n_ > capacity - 1u - length) n_ = capacity - 1u - length; \
		memcpy(out + length, (bytes), n_); length += n_; } while(0)
	PUT(head, total < INDIGO_LOG_HEAD_BYTES ? total : INDIGO_LOG_HEAD_BYTES);
	inTail = total > INDIGO_LOG_HEAD_BYTES ? total - INDIGO_LOG_HEAD_BYTES : 0u;
	if(inTail > INDIGO_LOG_TAIL_BYTES) {
		char note[64];
		int n = snprintf(note, sizeof(note), INDIGO_LOG_GAP, inTail - INDIGO_LOG_TAIL_BYTES);
		PUT(note, n > 0 ? (size_t)n : 0u);
		start = inTail % INDIGO_LOG_TAIL_BYTES;
		PUT(tail + start, INDIGO_LOG_TAIL_BYTES - start);
		PUT(tail, start);
	}
	else {
		PUT(tail, inTail);
	}
#undef PUT
	out[length] = '\0';
	return length;
}

size_t IndigoLog_Read(unsigned *from, char *out, size_t capacity)
{
	/* committed first: when written still matches it, every append so far
	 * has finished. */
	unsigned done = __atomic_load_n(&committed, __ATOMIC_ACQUIRE);
	unsigned at, end, kept;
	size_t length = 0;

	if(from == NULL || out == NULL || capacity == 0u || *from >= done ||
	   done != __atomic_load_n(&written, __ATOMIC_RELAXED)) return 0;
	at = *from;
	/* The oldest byte the ring still has; the head's stay for good. */
	kept = done > INDIGO_LOG_HEAD_BYTES + INDIGO_LOG_TAIL_BYTES ?
		done - INDIGO_LOG_TAIL_BYTES : INDIGO_LOG_HEAD_BYTES;
	if(at >= INDIGO_LOG_HEAD_BYTES && at < kept) {
		int n = snprintf(out, capacity, INDIGO_LOG_GAP, kept - at);
		*from = kept;
		if(n < 0) return 0;
		return (size_t)n < capacity ? (size_t)n : capacity - 1u;
	}
	end = at < INDIGO_LOG_HEAD_BYTES && kept > INDIGO_LOG_HEAD_BYTES ? INDIGO_LOG_HEAD_BYTES : done;
	while(at != end && length < capacity) {
		out[length++] = at < INDIGO_LOG_HEAD_BYTES ? head[at] :
			tail[(at - INDIGO_LOG_HEAD_BYTES) % INDIGO_LOG_TAIL_BYTES];
		at++;
	}
	if(at != end) {
		size_t line = length;
		while(line > 0u && out[line - 1u] != '\n') line--;
		if(line > 0u) {
			at -= (unsigned)(length - line);
			length = line;
		}
	}
	*from = at;
	return length;
}

void IndigoLog_Reset(void)
{
	__atomic_store_n(&written, 0u, __ATOMIC_RELAXED);
	__atomic_store_n(&committed, 0u, __ATOMIC_RELAXED);
}
