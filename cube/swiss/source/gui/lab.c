#include <gccore.h>
#include <malloc.h>
#include <network.h>
#include <ogc/lwp_threads.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "FrameBufferMagic.h"
#include "indigo_log.h"
#include "input.h"
#include "lab.h"
#include "menuaudio.h"
#include "mp3.h"
#include "swiss.h"
#include "ui_lab.h"
#include "video.h"

#define LAB_SEND_PIECE 16384u
#define LAB_LOG_DATAGRAM 1024u

static s32 logSocket = INVALID_SOCKET;
static mutex_t logMutex = LWP_MUTEX_NULL;
static lwp_t labThread = LWP_THREAD_NULL;
static lwp_t mainThread = LWP_THREAD_NULL;

__attribute((constructor))
static void lab_init(void)
{
	mainThread = LWP_GetSelf();
}

void lab_log(const char *line, size_t length)
{
	IndigoLog_Append(line, length);
	/* A sender waits for the network thread, which no interrupt may do. */
	if(logSocket != INVALID_SOCKET && !__lwp_isr_in_progress()) {
		LWP_MutexLock(logMutex);
		net_send(logSocket, line, length, 0);
		LWP_MutexUnlock(logMutex);
	}
}

/* The lines from before the network came up, in datagrams that end on a
 * line where one fits. */
static void lab_log_replay(s32 fd)
{
	char *text = malloc(INDIGO_LOG_COPY_BYTES);
	size_t length, at = 0;

	if(text == NULL) return;
	length = IndigoLog_Copy(text, INDIGO_LOG_COPY_BYTES);
	while(at < length) {
		size_t n = length - at;
		if(n > LAB_LOG_DATAGRAM) {
			n = LAB_LOG_DATAGRAM;
			while(n > 1u && text[at + n - 1u] != '\n') n--;
			if(text[at + n - 1u] != '\n') n = LAB_LOG_DATAGRAM;
		}
		net_send(fd, text + at, n, 0);
		at += n;
	}
	free(text);
}

static void lab_log_open(const unsigned char address[4], unsigned short port)
{
	struct sockaddr_in to = {.sin_family = AF_INET, .sin_port = htons(port)};
	s32 fd = net_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

	memcpy(&to.sin_addr.s_addr, address, 4);
	if(fd == INVALID_SOCKET) return;
	if(net_connect(fd, (struct sockaddr *)&to, sizeof(to)) < 0 ||
	   LWP_MutexInit(&logMutex, false) < 0) {
		net_close(fd);
		return;
	}
	lab_log_replay(fd);
	logSocket = fd;
}

static bool lab_send(s32 client, const void *data, size_t length)
{
	const u8 *at = data;

	while(length > 0u) {
		s32 sent = net_send(client, at, length > LAB_SEND_PIECE ? LAB_SEND_PIECE : length, 0);
		if(sent <= 0) return false;
		at += sent;
		length -= (size_t)sent;
	}
	return true;
}

static void lab_reply(s32 client, const char *status, const char *type, const void *body, size_t length)
{
	char head[160];
	int n = snprintf(head, sizeof(head), "HTTP/1.0 %s\r\nContent-Type: %s\r\nContent-Length: %u\r\n\r\n",
		status, type, (unsigned)length);

	if(n > 0 && lab_send(client, head, (size_t)n)) lab_send(client, body, length);
}

static void lab_text(s32 client, const char *status, const char *text)
{
	lab_reply(client, status, "text/plain", text, strlen(text));
}

/* The frame on screen, copied just after a retrace: the next one is drawn
 * into the other framebuffer and shown only at the next retrace. */
static void lab_xfb(s32 client)
{
	GXRModeObj *mode = getVideoMode();
	u32 size = VIDEO_GetFrameBufferSize(mode);
	u8 *frame = memalign(32, size);
	void *shown;
	char head[192];
	int n;

	if(frame == NULL) {
		lab_text(client, "503 Service Unavailable", "no memory for a frame\n");
		return;
	}
	VIDEO_WaitVSync();
	shown = VIDEO_GetCurrentFramebuffer();
	if(shown == NULL) {
		free(frame);
		lab_text(client, "503 Service Unavailable", "no framebuffer yet\n");
		return;
	}
	memcpy(frame, MEM_PHYSICAL_TO_K1(MEM_VIRTUAL_TO_PHYSICAL(shown)), size);
	n = snprintf(head, sizeof(head), "HTTP/1.0 200 OK\r\nContent-Type: application/octet-stream\r\n"
		"Content-Length: %u\r\nX-Size: %ux%u\r\nX-Format: yuyv422\r\n\r\n",
		(unsigned)size, (unsigned)(size / 2u / mode->xfbHeight), (unsigned)mode->xfbHeight);
	if(n > 0 && lab_send(client, head, (size_t)n)) lab_send(client, frame, size);
	free(frame);
}

static void lab_log_reply(s32 client)
{
	char *text = malloc(INDIGO_LOG_COPY_BYTES);

	if(text == NULL) {
		lab_text(client, "503 Service Unavailable", "no memory for the log\n");
		return;
	}
	lab_reply(client, "200 OK", "text/plain; charset=utf-8", text, IndigoLog_Copy(text, INDIGO_LOG_COPY_BYTES));
	free(text);
}

static u32 lab_pad_bits(unsigned buttons)
{
	u32 bits = 0;

	if(buttons & UI_LAB_A) bits |= PAD_BUTTON_A;
	if(buttons & UI_LAB_B) bits |= PAD_BUTTON_B;
	if(buttons & UI_LAB_X) bits |= PAD_BUTTON_X;
	if(buttons & UI_LAB_Y) bits |= PAD_BUTTON_Y;
	if(buttons & UI_LAB_Z) bits |= BUTTON_Z;
	if(buttons & UI_LAB_L) bits |= BUTTON_L | PAD_TRIGGER_L;
	if(buttons & UI_LAB_R) bits |= BUTTON_R | PAD_TRIGGER_R;
	if(buttons & UI_LAB_START) bits |= BUTTON_START;
	if(buttons & UI_LAB_UP) bits |= BUTTON_UP;
	if(buttons & UI_LAB_DOWN) bits |= BUTTON_DOWN;
	if(buttons & UI_LAB_LEFT) bits |= BUTTON_LEFT;
	if(buttons & UI_LAB_RIGHT) bits |= BUTTON_RIGHT;
	return bits;
}

static void lab_suspend_main(void *unused)
{
	(void)unused;
	LWP_SuspendThread(mainThread);
}

/* As an upload does: the main thread stops where it is and the menu audio
 * stops, then the console restarts into its boot program. */
static void lab_restart(void)
{
	DrawWithVideoLocked(lab_suspend_main, NULL);
	MP3Player_Stop();
	menuaudio_shutdown();
	DrawShutdown();
	SYS_ResetSystem(SYS_HOTRESET, 0, FALSE);
	__builtin_unreachable();
}

/* The request's head: up to its blank line, a full buffer, or 5 s of quiet. */
static s32 lab_read_head(s32 client, char *request, size_t capacity)
{
	size_t got = 0;

	while(got < capacity) {
		fd_set readable;
		struct timeval wait = {5, 0};
		s32 n;

		FD_ZERO(&readable);
		FD_SET(client, &readable);
		if(net_select(client + 1, &readable, NULL, NULL, &wait) <= 0) break;
		n = net_recv(client, request + got, capacity - got, 0);
		if(n <= 0) break;
		got += (size_t)n;
		for(size_t i = 1; i < got; i++) {
			if(request[i] == '\n' && (request[i - 1] == '\n' ||
			   (i >= 2u && request[i - 1] == '\r' && request[i - 2] == '\n')))
				return (s32)got;
		}
	}
	return (s32)got;
}

static void lab_serve(s32 client)
{
	char request[1024], status[48];
	s32 got = lab_read_head(client, request, sizeof(request));
	uiLabRequest_t parsed;
	uiLabPress_t press;

	if(got <= 0) return;
	parsed = UILab_ParseRequest(request, (size_t)got);
	switch(parsed.route) {
		case UI_LAB_XFB:
			lab_xfb(client);
			break;
		case UI_LAB_LOG:
			lab_log_reply(client);
			break;
		case UI_LAB_PAD_STATUS:
			snprintf(status, sizeof(status), "{\"real_idle_ms\":%u}\n", (unsigned)padsRealIdleMs());
			lab_reply(client, "200 OK", "application/json", status, strlen(status));
			break;
		case UI_LAB_PAD_PRESS:
			if(!UILab_ParsePress(parsed.query, parsed.queryLength, &press)) {
				lab_text(client, "400 Bad Request",
					"press=<A,B,X,Y,Z,L,R,Start,Up,Down,Left,Right>[&hold=<1-5000 ms>]\n");
				break;
			}
			padsVirtualPress(lab_pad_bits(press.buttons), press.holdMs);
			lab_text(client, "200 OK", "pressed\n");
			break;
		case UI_LAB_RESET:
			print_debug("Console lab: restart requested\n");
			lab_text(client, "200 OK", "restarting\n");
			net_close(client);
			usleep(200000);	/* the reply leaves before the restart */
			lab_restart();
			break;
		case UI_LAB_FORBIDDEN:
			lab_text(client, "403 Forbidden", "a POST needs an X-Lab header\n");
			break;
		case UI_LAB_BAD_REQUEST:
			lab_text(client, "400 Bad Request", "not an HTTP request\n");
			break;
		default:
			lab_text(client, "404 Not Found", "GET /xfb, GET /log, GET /pad, POST /pad?press=..., POST /reset\n");
			break;
	}
}

static void *lab_thread_func(void *arg)
{
	struct sockaddr_in me = {.sin_family = AF_INET, .sin_port = htons(UI_LAB_PORT),
		.sin_addr.s_addr = INADDR_ANY};
	s32 server = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

	(void)arg;
	if(server == INVALID_SOCKET || net_bind(server, (struct sockaddr *)&me, sizeof(me)) < 0 ||
	   net_listen(server, 2) < 0) {
		print_debug("Console lab: cannot listen on port %d\n", UI_LAB_PORT);
		if(server != INVALID_SOCKET) net_close(server);
		return NULL;
	}
	print_debug("Console lab: listening on port %d\n", UI_LAB_PORT);
	for(;;) {
		struct sockaddr_in from;
		socklen_t length = sizeof(from);
		s32 client = net_accept(server, (struct sockaddr *)&from, &length);
		if(client < 0) {
			usleep(100000);
			continue;
		}
		lab_serve(client);
		net_close(client);
	}
	return NULL;
}

void lab_start(void)
{
	unsigned char address[4];
	unsigned short port;

	if(swissSettings.logHost[0] != '\0' && logSocket == INVALID_SOCKET) {
		if(UILab_ParseHost(swissSettings.logHost, address, &port)) lab_log_open(address, port);
		else print_debug("Console lab: LogHost %s is not an address[:port]\n", swissSettings.logHost);
	}
	if(swissSettings.labServer && labThread == LWP_THREAD_NULL)
		LWP_CreateThread(&labThread, lab_thread_func, NULL, NULL, 16 * 1024, LWP_PRIO_NORMAL);
}
