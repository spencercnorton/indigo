#ifndef TIME_SYNC_H
#define TIME_SYNC_H

/* Settings › Network's Sync Clock: from the network thread, once the network
 * is up, sets the console's clock from the NTP server. */
void time_sync_network(void);

#endif
