#include <limits.h>
#include "include/seefs.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static bool seefs_branch_from_string(const char *token,
                                     enum seefs_branch_type *branch)
{
	if (strcmp(token, "applications") == 0) {
		*branch = SEEFS_BRANCH_APPLICATIONS;
		return true;
	}
	if (strcmp(token, "kernel_threads") == 0) {
		*branch = SEEFS_BRANCH_KERNEL_THREADS;
		return true;
	}
	return false;
}

/**
 * @brief Parses a path string into a structured seefs_path_info.
 * 
 * Expected path formats:
 * - /
 * - /hello
 * - /users
 * - /users/<username>
 * - /users/<username>/<branch>
 * - /users/<username>/<branch>/<group>
 * - /users/<username>/<branch>/<group>/<pid>
 * - /users/<username>/<branch>/<group>/<pid>/<data_file>
 */
bool seefs_parse_path(const char *path, struct seefs_path_info *info)
{
	memset(info, 0, sizeof(*info));
	info->type = SEEFS_NODE_INVALID;
	info->branch = SEEFS_BRANCH_NONE;

	if (!path || path[0] != '/')
		return false;

	if (strcmp(path, "/") == 0) {
		info->type = SEEFS_NODE_ROOT;
		return true;
	}

	if (strcmp(path, SEEFS_HELLO_PATH) == 0) {
		info->type = SEEFS_NODE_HELLO;
		return true;
	}

size_t path_len = strlen(path);
        if (path_len >= PATH_MAX)
                return false;

        // Copy path for tokenization (skip leading slash)
        char temp[PATH_MAX];
        memcpy(temp, path + 1, path_len); // Copies null byte too

	char *segments[8];
	size_t seg_count = 0;
	char *save_ptr = NULL;
	char *token = strtok_r(temp, "/", &save_ptr);
	while (token && seg_count < sizeof(segments) / sizeof(segments[0])) {
		segments[seg_count++] = token;
		token = strtok_r(NULL, "/", &save_ptr);
	}
	if (token != NULL) // path depth exceeds expectation
		return false;

	if (seg_count == 0)
		return false;

	if (strcmp(segments[0], "users") != 0)
		return false;

	if (seg_count == 1) {
		info->type = SEEFS_NODE_USERS;
		return true;
	}

	seefs_copy_string(info->username, sizeof(info->username), segments[1]);

	if (seg_count == 2) {
		info->type = SEEFS_NODE_USER;
		return true;
	}

	if (!seefs_branch_from_string(segments[2], &info->branch))
		return false;

	if (seg_count == 3) {
		info->type = SEEFS_NODE_BRANCH;
		return true;
	}

	seefs_copy_string(info->group, sizeof(info->group), segments[3]);

	if (seg_count == 4) {
		info->type = SEEFS_NODE_GROUP;
		return true;
	}

	char *endptr = NULL;
	long pid_val = strtol(segments[4], &endptr, 10);
	if (!endptr || *endptr != '\0' || pid_val <= 0 || pid_val > INT_MAX)
		return false;

	info->pid = (pid_t) pid_val;

	if (seg_count == 5) {
		info->type = SEEFS_NODE_PID;
		return true;
	}

	if (seg_count == 6) {
		if (strcmp(segments[5], "history") == 0) {
			info->type = SEEFS_NODE_HISTORY;
			return true;
		}
		if (strcmp(segments[5], "cmdline") == 0 || strcmp(segments[5], "status") == 0) {
			seefs_copy_string(info->data_file, sizeof(info->data_file), segments[5]);
			info->type = SEEFS_NODE_DATA_FILE;
			return true;
		}
		return false;
	}

	if (seg_count == 7) {
		if (strcmp(segments[5], "history") == 0) {
			seefs_copy_string(info->timestamp, sizeof(info->timestamp), segments[6]);
			info->type = SEEFS_NODE_TIMESTAMP;
			return true;
		}
		return false;
	}

	if (seg_count == 8) {
		if (strcmp(segments[5], "history") == 0) {
			seefs_copy_string(info->timestamp, sizeof(info->timestamp), segments[6]);
			if (strcmp(segments[7], "cmdline") == 0 || strcmp(segments[7], "status") == 0) {
				seefs_copy_string(info->data_file, sizeof(info->data_file), segments[7]);
				info->type = SEEFS_NODE_HISTORY_FILE;
				return true;
			}
		}
		return false;
	}

	return false;
}
