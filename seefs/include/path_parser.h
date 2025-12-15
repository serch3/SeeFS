#ifndef PATH_PARSER_H
#define PATH_PARSER_H

#include <sys/types.h>
#include <stdbool.h>

#define SEEFS_NAME_MAX 256

// Represents the type of a path component in the SeeFS hierarchy.
enum seefs_node_type {
	SEEFS_NODE_INVALID = 0,
	SEEFS_NODE_ROOT,
	SEEFS_NODE_HELLO,
	SEEFS_NODE_USERS,
	SEEFS_NODE_USER,
	SEEFS_NODE_BRANCH,
	SEEFS_NODE_GROUP,
	SEEFS_NODE_PID,
	SEEFS_NODE_HISTORY,
	SEEFS_NODE_TIMESTAMP,
	SEEFS_NODE_HISTORY_FILE,
	SEEFS_NODE_DATA_FILE,
};

// Distinguishes between user-space applications and kernel threads.
enum seefs_branch_type {
	SEEFS_BRANCH_NONE = 0,
	SEEFS_BRANCH_APPLICATIONS,
	SEEFS_BRANCH_KERNEL_THREADS,
};

struct seefs_path_info {
	enum seefs_node_type type;
	enum seefs_branch_type branch;
	char username[SEEFS_NAME_MAX];
	char group[SEEFS_NAME_MAX];
	pid_t pid;
	char timestamp[32];
	char data_file[SEEFS_NAME_MAX];
};

bool seefs_parse_path(const char *path, struct seefs_path_info *info);

#endif
