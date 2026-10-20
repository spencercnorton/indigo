/* Fuzz the console lab's request reading (gui/ui_lab.c), which takes bytes
 * from anyone on the LAN: any request parses without a sanitizer finding, a
 * query stays inside the request, and a press it accepts names a button
 * and holds for a time within bounds. The bytes are copied to exactly their
 * size, so a read past the request is a finding. The same bytes, cut at the
 * first NUL, are tried as a LogHost setting. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ui_lab.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char *copy = malloc(size ? size : 1);
	char host[32];
	unsigned char address[4];
	unsigned short port;
	uiLabRequest_t request;
	uiLabPress_t press;

	if(!copy)
		return 0;
	memcpy(copy, data, size);
	request = UILab_ParseRequest(copy, size);
	if(request.route > UI_LAB_BAD_REQUEST)
		abort();
	if(request.query != NULL &&
	   (request.query < copy || request.queryLength > size ||
	    (size_t)(request.query - copy) > size - request.queryLength))
		abort();
	if(request.query != NULL && UILab_ParsePress(request.query, request.queryLength, &press)) {
		if(press.buttons == 0u || press.buttons >= (UI_LAB_RIGHT << 1) ||
		   press.holdMs == 0u || press.holdMs > UI_LAB_HOLD_MAX_MS)
			abort();
	}
	free(copy);

	size_t n = size < sizeof(host) - 1u ? size : sizeof(host) - 1u;
	memcpy(host, data, n);
	host[n] = '\0';
	if(UILab_ParseHost(host, address, &port) && port == 0u)
		abort();
	return 0;
}
