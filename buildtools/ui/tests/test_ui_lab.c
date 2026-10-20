#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_lab.h"

static unsigned long checks;

#define CHECK(condition) do { \
	checks++; \
	if(!(condition)) { \
		fprintf(stderr, "check failed at %s:%d: %s\n", \
			__FILE__, __LINE__, #condition); \
		exit(1); \
	} \
} while(0)

static uiLabRequest_t request(const char *text)
{
	return UILab_ParseRequest(text, strlen(text));
}

static bool queryIs(uiLabRequest_t parsed, const char *query)
{
	return parsed.query != NULL && parsed.queryLength == strlen(query) &&
		memcmp(parsed.query, query, parsed.queryLength) == 0;
}

static void readsNeedNoHeader(void)
{
	CHECK(request("GET /xfb HTTP/1.1\r\nHost: cube\r\n\r\n").route == UI_LAB_XFB);
	CHECK(request("GET /log HTTP/1.0\r\n\r\n").route == UI_LAB_LOG);
	CHECK(request("GET /pad HTTP/1.1\n\n").route == UI_LAB_PAD_STATUS);
	CHECK(request("GET /xfb?fresh=1 HTTP/1.1\r\n").route == UI_LAB_XFB);
	CHECK(queryIs(request("GET /xfb?fresh=1 HTTP/1.1\r\n"), "fresh=1"));
	CHECK(request("GET /xfb HTTP/1.1\r\n").query == NULL);
}

/* A POST acts only with an X-Lab header, in any case, among the headers. */
static void actionsNeedTheHeader(void)
{
	uiLabRequest_t press = request("POST /pad?press=A,Right&hold=150 HTTP/1.1\r\n"
		"Host: cube\r\nX-Lab: 1\r\n\r\n");

	CHECK(press.route == UI_LAB_PAD_PRESS);
	CHECK(queryIs(press, "press=A,Right&hold=150"));
	CHECK(request("POST /reset HTTP/1.1\r\nx-lab: yes\r\n\r\n").route == UI_LAB_RESET);
	CHECK(request("POST /reset HTTP/1.1\nX-LAB:\n\n").route == UI_LAB_RESET);
	CHECK(request("POST /reset HTTP/1.1\r\n\r\n").route == UI_LAB_FORBIDDEN);
	CHECK(request("POST /pad?press=A HTTP/1.1\r\nHost: cube\r\n").route == UI_LAB_FORBIDDEN);
	/* After the blank line it is the body, not a header. */
	CHECK(request("POST /reset HTTP/1.1\r\n\r\nX-Lab: 1\r\n").route == UI_LAB_FORBIDDEN);
	/* A header named only like it is not it. */
	CHECK(request("POST /reset HTTP/1.1\r\nX-Labs: 1\r\nAccept: x-lab\r\n\r\n").route == UI_LAB_FORBIDDEN);
}

static void otherRequestsAreNotFound(void)
{
	CHECK(request("GET /reset HTTP/1.1\r\nX-Lab: 1\r\n").route == UI_LAB_NOT_FOUND);
	CHECK(request("POST /xfb HTTP/1.1\r\nX-Lab: 1\r\n").route == UI_LAB_NOT_FOUND);
	CHECK(request("PUT /pad HTTP/1.1\r\nX-Lab: 1\r\n").route == UI_LAB_NOT_FOUND);
	CHECK(request("GET / HTTP/1.1\r\n").route == UI_LAB_NOT_FOUND);
	CHECK(request("GET /xfbx HTTP/1.1\r\n").route == UI_LAB_NOT_FOUND);
	CHECK(request("get /xfb HTTP/1.1\r\n").route == UI_LAB_NOT_FOUND);
}

static void brokenRequestsAreBad(void)
{
	CHECK(UILab_ParseRequest(NULL, 12u).route == UI_LAB_BAD_REQUEST);
	CHECK(request("").route == UI_LAB_BAD_REQUEST);
	CHECK(request("GET /xfb HTTP/1.1").route == UI_LAB_BAD_REQUEST);      /* no line end yet */
	CHECK(request("GET /xfb\r\n").route == UI_LAB_BAD_REQUEST);
	CHECK(request("GET  HTTP/1.1\r\n").route == UI_LAB_BAD_REQUEST);
	CHECK(request("GET /xfb FTP/1.1\r\n").route == UI_LAB_BAD_REQUEST);
	CHECK(request("GET /xfb HTTP\r\n").route == UI_LAB_BAD_REQUEST);
	/* Only the bytes given count: the line end is past them. */
	CHECK(UILab_ParseRequest("GET /xfb HTTP/1.1\r\n", 17u).route == UI_LAB_BAD_REQUEST);
}

static bool press(const char *query, uiLabPress_t *out)
{
	return UILab_ParsePress(query, strlen(query), out);
}

static void pressesNameButtons(void)
{
	uiLabPress_t out;

	CHECK(press("press=A", &out) && out.buttons == UI_LAB_A && out.holdMs == UI_LAB_HOLD_DEFAULT_MS);
	CHECK(press("press=a,Right+START&hold=250", &out));
	CHECK(out.buttons == (UI_LAB_A | UI_LAB_RIGHT | UI_LAB_START) && out.holdMs == 250u);
	CHECK(press("hold=5000&press=down", &out) && out.buttons == UI_LAB_DOWN && out.holdMs == 5000u);
	CHECK(press("press=b,x,y,z,l,r,up,left", &out));
	CHECK(out.buttons == (UI_LAB_B | UI_LAB_X | UI_LAB_Y | UI_LAB_Z | UI_LAB_L | UI_LAB_R |
		UI_LAB_UP | UI_LAB_LEFT));
	CHECK(press("press=A&", &out) && out.buttons == UI_LAB_A);
}

static void badPressesChangeNothing(void)
{
	uiLabPress_t out = {77u, 77u};
	static const char *const bad[] = {
		"", "press", "press=", "press=A,,B", "press=Q", "press=AB", "press=A,",
		"hold=0&press=A", "hold=5001&press=A", "hold=abc&press=A", "hold=&press=A",
		"hold=100", "press=A&light=1", "press=A&&hold=1", "=A",
	};

	for(size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
		CHECK(!press(bad[i], &out));
	}
	CHECK(!UILab_ParsePress(NULL, 4u, &out));
	CHECK(!UILab_ParsePress("press=A", 7u, NULL));
	/* Only the bytes given count. */
	CHECK(!UILab_ParsePress("press=A", 6u, &out));
	CHECK(out.buttons == 77u && out.holdMs == 77u);
}

static void hostsAreDottedQuads(void)
{
	unsigned char address[4] = {9, 9, 9, 9};
	unsigned short port = 9;
	static const char *const bad[] = {
		"", "192.168.1", "192.168.1.256", "1.2.3.4:", "1.2.3.4:0", "1.2.3.4:65536",
		"1.2.3.4:12a", "1.2.3.4 ", " 1.2.3.4", "a.b.c.d", "1.2.3.4.5", "0001.2.3.4",
		"1..3.4", "1.2.3.4:123456",
	};

	CHECK(UILab_ParseHost("192.168.1.57", address, &port));
	CHECK(address[0] == 192 && address[1] == 168 && address[2] == 1 && address[3] == 57);
	CHECK(port == UI_LAB_LOG_PORT);
	CHECK(UILab_ParseHost("10.0.0.255:5140", address, &port));
	CHECK(address[0] == 10 && address[3] == 255 && port == 5140);
	for(size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
		CHECK(!UILab_ParseHost(bad[i], address, &port));
	}
	CHECK(address[0] == 10 && address[3] == 255 && port == 5140);
	CHECK(!UILab_ParseHost(NULL, address, &port));
}

int main(void)
{
	readsNeedNoHeader();
	actionsNeedTheHeader();
	otherRequestsAreNotFound();
	brokenRequestsAreBad();
	pressesNameButtons();
	badPressesChangeNothing();
	hostsAreDottedQuads();
	printf("console lab: %lu checks passed\n", checks);
	return 0;
}
