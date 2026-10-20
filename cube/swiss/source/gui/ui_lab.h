#ifndef UI_LAB_H
#define UI_LAB_H

#include <stdbool.h>
#include <stddef.h>

/*
 * The console lab: a test run on the LAN drives Indigo on a real console.
 * With LabServer=Yes, TCP UI_LAB_PORT answers
 *   GET  /xfb     the framebuffer on screen, raw YUYV (X-Size: width x height)
 *   GET  /log     the print_debug lines so far (gui/indigo_log.c)
 *   GET  /pad     how long since a real controller's button went down, as JSON
 *   POST /pad?press=A,Right&hold=150   buttons on a virtual pad, held for ms
 *   POST /reset   restarts the console
 * A POST needs an X-Lab header. A browser sends a header of its own only
 * after a preflight this server never answers, so no web page can press or
 * restart. LogHost=<address>[:port] sends each print_debug line there over
 * UDP. This file only reads the text; gui/lab.c does the rest.
 */
#define UI_LAB_PORT 4300
#define UI_LAB_LOG_PORT 4405
#define UI_LAB_HOLD_DEFAULT_MS 100u
#define UI_LAB_HOLD_MAX_MS 5000u

typedef enum {
	UI_LAB_NOT_FOUND = 0,
	UI_LAB_XFB,
	UI_LAB_LOG,
	UI_LAB_PAD_STATUS,
	UI_LAB_PAD_PRESS,
	UI_LAB_RESET,
	UI_LAB_FORBIDDEN,	/* a POST without the X-Lab header */
	UI_LAB_BAD_REQUEST
} uiLabRoute_t;

typedef struct {
	uiLabRoute_t route;
	const char *query;	/* after '?', not NUL-terminated; NULL when there is none */
	size_t queryLength;
} uiLabRequest_t;

/* The buttons a press can name; gui/lab.c maps them onto the pad's bits. */
enum {
	UI_LAB_A = 1u << 0,
	UI_LAB_B = 1u << 1,
	UI_LAB_X = 1u << 2,
	UI_LAB_Y = 1u << 3,
	UI_LAB_Z = 1u << 4,
	UI_LAB_L = 1u << 5,
	UI_LAB_R = 1u << 6,
	UI_LAB_START = 1u << 7,
	UI_LAB_UP = 1u << 8,
	UI_LAB_DOWN = 1u << 9,
	UI_LAB_LEFT = 1u << 10,
	UI_LAB_RIGHT = 1u << 11
};

typedef struct {
	unsigned buttons;
	unsigned holdMs;
} uiLabPress_t;

/* What the first length bytes of a request ask for. */
uiLabRequest_t UILab_ParseRequest(const char *request, size_t length);

/* press=<button names, comma or + separated>[&hold=<1..UI_LAB_HOLD_MAX_MS>]:
 * false when a key, a name or a number is not understood. */
bool UILab_ParsePress(const char *query, size_t length, uiLabPress_t *press);

/* "a.b.c.d" or "a.b.c.d:port"; the port is UI_LAB_LOG_PORT when absent. */
bool UILab_ParseHost(const char *text, unsigned char address[4], unsigned short *port);

#endif
