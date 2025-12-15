#ifndef SEEFS_HISTORY_H
#define SEEFS_HISTORY_H

#include <sys/types.h>
#include <time.h>

// Initialize the history tracking subsystem
void seefs_history_init(void);

// Shutdown the history tracking subsystem
void seefs_history_shutdown(void);

// Get a list of available timestamps for a given PID
// Returns 0 on success, negative errno on failure
// timestamps is allocated and must be freed by caller (along with strings)
int seefs_history_get_timestamps(pid_t pid, char ***timestamps, size_t *count);

// Get the content of a file for a specific PID and timestamp
// Returns 0 on success, negative errno on failure
// buffer is allocated and must be freed by caller
int seefs_history_get_data(pid_t pid, const char *timestamp, const char *filename, char **buffer, size_t *size);

#endif
