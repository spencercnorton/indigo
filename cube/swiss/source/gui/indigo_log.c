#include <stdio.h>
#include <string.h>

#include "indigo_log.h"

static char head[INDIGO_LOG_HEAD_BYTES];
static char tail[INDIGO_LOG_TAIL_BYTES];
static unsigned written;          /* bytes ever appended */
static const char *stage;
static unsigned stageSerial;

void IndigoLog_Append(const char *text, size_t length)
{
	unsigned at;

	if(text == NULL || length == 0u) return;
	at = __atomic_fetch_add(&written, (unsigned)length, __ATOMIC_RELAXED);
	for(size_t i = 0; i < length; i++, at++) {
		if(at < INDIGO_LOG_HEAD_BYTES) head[at] = text[i];
		else tail[(at - INDIGO_LOG_HEAD_BYTES) % INDIGO_LOG_TAIL_BYTES] = text[i];
	}
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
		int n = snprintf(note, sizeof(note), "\n[... %u bytes not kept ...]\n",
			inTail - INDIGO_LOG_TAIL_BYTES);
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

void IndigoLog_SetStage(const char *next)
{
	__atomic_store_n(&stage, next, __ATOMIC_RELEASE);
	__atomic_fetch_add(&stageSerial, 1u, __ATOMIC_RELEASE);
}

const char *IndigoLog_Stage(unsigned *serial)
{
	if(serial != NULL) *serial = __atomic_load_n(&stageSerial, __ATOMIC_ACQUIRE);
	return __atomic_load_n(&stage, __ATOMIC_ACQUIRE);
}

void IndigoLog_Reset(void)
{
	__atomic_store_n(&written, 0u, __ATOMIC_RELAXED);
	IndigoLog_SetStage(NULL);
}
