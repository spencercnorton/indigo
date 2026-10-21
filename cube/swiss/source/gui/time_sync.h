#ifndef TIME_SYNC_H
#define TIME_SYNC_H

/* The NTP server when NTP Server IP is empty: time.cloudflare.com's address,
 * since the console has no DNS. */
#define TIME_SYNC_DEFAULT_SERVER "162.159.200.123"

/* Settings › Network's Sync Clock: from the network thread, once the network
 * is up, sets the console's clock from the NTP server. */
void time_sync_network(void);

#endif
