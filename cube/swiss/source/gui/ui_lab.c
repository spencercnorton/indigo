#include <string.h>

#include "ui_lab.h"

static const struct {
	const char *name;
	unsigned bit;
} labButtons[] = {
	{"a", UI_LAB_A}, {"b", UI_LAB_B}, {"x", UI_LAB_X}, {"y", UI_LAB_Y},
	{"z", UI_LAB_Z}, {"l", UI_LAB_L}, {"r", UI_LAB_R}, {"start", UI_LAB_START},
	{"up", UI_LAB_UP}, {"down", UI_LAB_DOWN}, {"left", UI_LAB_LEFT}, {"right", UI_LAB_RIGHT},
};

static bool labIs(const char *text, size_t length, const char *word)
{
	return strlen(word) == length && memcmp(text, word, length) == 0;
}

/* word is lower case; text may be any case. */
static bool labIsNoCase(const char *text, size_t length, const char *word)
{
	if(strlen(word) != length) return false;
	for(size_t i = 0; i < length; i++) {
		char c = text[i];
		if(c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
		if(c != word[i]) return false;
	}
	return true;
}

/* Decimal digits only, at most five of them, no more than max. */
static bool labNumber(const char *text, size_t length, unsigned max, unsigned *value)
{
	unsigned n = 0;

	if(length == 0u || length > 5u) return false;
	for(size_t i = 0; i < length; i++) {
		if(text[i] < '0' || text[i] > '9') return false;
		n = n * 10u + (unsigned)(text[i] - '0');
	}
	if(n > max) return false;
	*value = n;
	return true;
}

/* A header line after the request line is named X-Lab. The blank line that
 * ends the headers, or the end of what arrived, ends the search. */
static bool labHasHeader(const char *at, const char *end)
{
	while(at < end) {
		const char *eol = memchr(at, '\n', (size_t)(end - at));
		size_t n = (size_t)((eol != NULL ? eol : end) - at);
		if(n > 0u && at[n - 1u] == '\r') n--;
		if(n == 0u) return false;
		const char *colon = memchr(at, ':', n);
		if(colon != NULL && labIsNoCase(at, (size_t)(colon - at), "x-lab")) return true;
		if(eol == NULL) return false;
		at = eol + 1;
	}
	return false;
}

uiLabRequest_t UILab_ParseRequest(const char *request, size_t length)
{
	uiLabRequest_t out = {UI_LAB_BAD_REQUEST, NULL, 0u};
	const char *eol, *space, *target, *targetEnd, *question;
	size_t methodLength, pathLength;
	bool post;

	if(request == NULL) return out;
	/* METHOD SP target SP HTTP/1.x, all in the first line. */
	eol = memchr(request, '\n', length);
	if(eol == NULL) return out;
	space = memchr(request, ' ', (size_t)(eol - request));
	if(space == NULL) return out;
	methodLength = (size_t)(space - request);
	target = space + 1;
	targetEnd = memchr(target, ' ', (size_t)(eol - target));
	if(targetEnd == NULL || targetEnd == target ||
	   (size_t)(eol - targetEnd) < 6u || memcmp(targetEnd + 1, "HTTP/", 5) != 0) return out;

	post = labIs(request, methodLength, "POST");
	if(!post && !labIs(request, methodLength, "GET")) {
		out.route = UI_LAB_NOT_FOUND;
		return out;
	}
	question = memchr(target, '?', (size_t)(targetEnd - target));
	pathLength = (size_t)((question != NULL ? question : targetEnd) - target);
	if(question != NULL) {
		out.query = question + 1;
		out.queryLength = (size_t)(targetEnd - out.query);
	}

	out.route = UI_LAB_NOT_FOUND;
	if(!post) {
		if(labIs(target, pathLength, "/xfb")) out.route = UI_LAB_XFB;
		else if(labIs(target, pathLength, "/log")) out.route = UI_LAB_LOG;
		else if(labIs(target, pathLength, "/pad")) out.route = UI_LAB_PAD_STATUS;
	}
	else if(labIs(target, pathLength, "/pad")) out.route = UI_LAB_PAD_PRESS;
	else if(labIs(target, pathLength, "/reset")) out.route = UI_LAB_RESET;
	if(post && out.route != UI_LAB_NOT_FOUND && !labHasHeader(eol + 1, request + length))
		out.route = UI_LAB_FORBIDDEN;
	return out;
}

static bool labButtonNames(const char *text, size_t length, unsigned *buttons)
{
	size_t start = 0;

	*buttons = 0u;
	for(size_t i = 0; i <= length; i++) {
		unsigned bit = 0u;
		if(i < length && text[i] != ',' && text[i] != '+') continue;
		for(size_t b = 0; b < sizeof(labButtons) / sizeof(labButtons[0]); b++) {
			if(labIsNoCase(text + start, i - start, labButtons[b].name)) bit = labButtons[b].bit;
		}
		if(bit == 0u) return false;
		*buttons |= bit;
		start = i + 1u;
	}
	return true;
}

bool UILab_ParsePress(const char *query, size_t length, uiLabPress_t *press)
{
	uiLabPress_t out = {0u, UI_LAB_HOLD_DEFAULT_MS};
	const char *at = query, *end;

	if(query == NULL || press == NULL) return false;
	end = query + length;
	while(at < end) {
		const char *amp = memchr(at, '&', (size_t)(end - at));
		const char *pairEnd = amp != NULL ? amp : end;
		const char *equals = memchr(at, '=', (size_t)(pairEnd - at));
		const char *value;
		size_t keyLength, valueLength;

		if(equals == NULL) return false;
		value = equals + 1;
		keyLength = (size_t)(equals - at);
		valueLength = (size_t)(pairEnd - value);
		if(labIs(at, keyLength, "press")) {
			if(!labButtonNames(value, valueLength, &out.buttons)) return false;
		}
		else if(labIs(at, keyLength, "hold")) {
			if(!labNumber(value, valueLength, UI_LAB_HOLD_MAX_MS, &out.holdMs) ||
			   out.holdMs == 0u) return false;
		}
		else return false;
		at = amp != NULL ? amp + 1 : end;
	}
	if(out.buttons == 0u) return false;
	*press = out;
	return true;
}

bool UILab_ParseHost(const char *text, unsigned char address[4], unsigned short *port)
{
	unsigned char parts[4];
	unsigned value;
	size_t length, at = 0;

	if(text == NULL || address == NULL || port == NULL) return false;
	length = strlen(text);
	for(int part = 0; part < 4; part++) {
		size_t start = at;
		while(at < length && text[at] >= '0' && text[at] <= '9') at++;
		if(at - start > 3u || !labNumber(text + start, at - start, 255u, &value)) return false;
		parts[part] = (unsigned char)value;
		if(part < 3) {
			if(at >= length || text[at] != '.') return false;
			at++;
		}
	}
	value = UI_LAB_LOG_PORT;
	if(at < length) {
		if(text[at] != ':' ||
		   !labNumber(text + at + 1, length - at - 1u, 65535u, &value) || value == 0u) return false;
	}
	memcpy(address, parts, sizeof(parts));
	*port = (unsigned short)value;
	return true;
}
