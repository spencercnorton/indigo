#ifndef LAB_H
#define LAB_H

#include <stddef.h>

/* The console lab: ui_lab.h says what it answers and why. */

/* The network is up: LogHost's sender and LabServer's port start here. */
void lab_start(void);

/* One print_debug line: into the log GET /log serves, and to LogHost. */
void lab_log(const char *line, size_t length);

#endif
