#include <time.h>
#include <network.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/system.h>

#include "swiss.h"
#include "time_sync.h"
#include "ui_time_sync.h"

#define TIME_SYNC_TRIES 3	/* a second each: the first can go before ARP has an answer */

/* One SNTP request, a second's wait for the reply, three tries. The reply is
 * the server's time; half the round trip and the time since bring it to now. */
static bool time_sync_ask(struct in_addr server, int64_t *utcMicroseconds)
{
	struct sockaddr_in address = {
		.sin_family = AF_INET, .sin_port = htons(123), .sin_addr = server,
	};
	s32 fd = net_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	bool answered = false;

	if(fd < 0) {
		return false;
	}
	if(net_connect(fd, (struct sockaddr *)&address, sizeof(address)) >= 0) {
		for(int i = 0; i < TIME_SYNC_TRIES && !answered; i++) {
			u8 packet[UI_TIME_SYNC_PACKET];
			u64 sent = gettime();
			UITimeSync_Request(packet, sent);
			if(net_send(fd, packet, sizeof(packet), 0) < 0) {
				break;
			}
			while(!answered) {
				struct timeval wait = {1, 0};
				fd_set readable;
				FD_ZERO(&readable);
				FD_SET(fd, &readable);
				if(net_select(fd + 1, &readable, NULL, NULL, &wait) <= 0) {
					break;
				}
				s32 length = net_recv(fd, packet, sizeof(packet), 0);
				if(length < 0) {
					break;
				}
				u64 received = gettime();
				answered = UITimeSync_Reply(packet, (size_t)length, sent, utcMicroseconds);
				if(answered) {
					*utcMicroseconds += (int64_t)(ticks_to_microsecs(diff_ticks(sent, received)) / 2 +
						ticks_to_microsecs(diff_ticks(received, gettime())));
				}
			}
		}
	}
	net_close(fd);
	return answered;
}

/* The console keeps local time with no zone, and so does the IPL: the time base
 * while it runs, and after a restart the RTC's seconds plus the SRAM's counter
 * bias. Both move, so games and the IPL read the new time too. */
void time_sync_network(void)
{
	struct in_addr server;
	int64_t utc;
	int32_t east;
	u32 rtc;

	if(!swissSettings.ntpSync || !inet_aton(swissSettings.ntpHostIp, &server) ||
		!time_sync_ask(server, &utc)) {
		return;
	}
	int64_t utcSeconds = utc / 1000000;
	if(swissSettings.timeZone[0] == '\0') {
		east = UITimeSync_KeptOffset(time(NULL), utcSeconds);
	}
	else if(!UITimeSync_ZoneOffset(swissSettings.timeZone, utcSeconds, &east)) {
		return;
	}
	int64_t local = utc + (int64_t)east * 1000000;
	struct timespec now = {
		.tv_sec = (time_t)(local / 1000000), .tv_nsec = (long)(local % 1000000) * 1000,
	};
	clock_settime(CLOCK_REALTIME, &now);
	if(__SYS_GetRTC(&rtc)) {
		SYS_SetCounterBias((u32)(now.tv_sec - DIFF_SEC_1970_2000) - rtc);
		while(!__SYS_SyncSram());
	}
}
