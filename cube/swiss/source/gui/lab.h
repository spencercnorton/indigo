#ifndef LAB_H
#define LAB_H

#include <stddef.h>

/* The console lab: ui_lab.h says what it answers and why. */

/* The network is up: LogHost's sender and LabServer's port start here. */
void lab_start(void);

/* One print_debug line: into the log GET /log serves, and to LogHost. */
void lab_log(const char *line, size_t length);

/* wiiload.c: waits out an upload under way, then holds the next off for
 * good. POST /reset runs an upload's hand-off, so it takes this first. */
void wiiload_hold(void);

#endif
