// seefs.h - Shared definitions and API for the SeeFS FUSE filesystem.
// https://libfuse.github.io/doxygen/structfuse__operations.html

#ifndef SEEFS_H
#define SEEFS_H

#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "history.h"

/* ========================================================================
 * Configuration Constants
 * ======================================================================== */

#define SEEFS_HELLO_PATH     "/hello"
#define SEEFS_HELLO_CONTENT  "Hello, World!\n"
#define SEEFS_PROC_ROOT      "/proc"

#include "path_parser.h"

// Buffer size constants used when reading from /proc and formatting strings.
#define SEEFS_PW_BUF_SIZE    1024
#define SEEFS_PID_STR_SIZE   32
#define SEEFS_INIT_BUF_SIZE  4096
#define SEEFS_BUF_THRESHOLD  2048
#define SEEFS_BUF_GROW_FACTOR 2

/* ========================================================================
 * Core Data Structures
 * ======================================================================== */

/**
 * @brief Information about a running process retrieved from /proc.
 */
struct seefs_proc_info {
	pid_t pid;
	uid_t uid;
	char username[SEEFS_NAME_MAX];
	char comm[SEEFS_NAME_MAX];
	char group_name[SEEFS_NAME_MAX];
	bool is_kernel_thread;
};

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

/**
 * @brief Safely copy a string with guaranteed null-termination.
 */
static inline void seefs_copy_string(char *dst, size_t dst_size, const char *src)
{
	if (dst_size == 0)
		return;

	if (!src) {
		dst[0] = '\0';
		return;
	}

	strncpy(dst, src, dst_size - 1);
	dst[dst_size - 1] = '\0';
}

/* ========================================================================
 * Path Parsing and Validation API
 * ======================================================================== */

int seefs_user_exists(const char *username);

int seefs_group_exists(const char *username, enum seefs_branch_type branch,
                       const char *group_name);

int seefs_pid_matches(const struct seefs_path_info *info,
                      struct seefs_proc_info *proc_out);

/* ========================================================================
 * FUSE Operation Handlers
 * ======================================================================== */

int seefs_inode_getattr(const char *path, struct stat *stbuf);

int seefs_inode_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi);

int seefs_file_open(const char *path, struct fuse_file_info *fi);

int seefs_file_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi);

const struct fuse_operations *seefs_get_operations(void);

/* ========================================================================
 * Process Data API
 * ======================================================================== */

int seefs_proc_iterate(int (*cb)(const struct seefs_proc_info *info,
                                 void *ctx),
                       void *ctx);

int seefs_proc_info_fetch(pid_t pid, struct seefs_proc_info *info);

int seefs_proc_read_cmdline(pid_t pid, char **buf, size_t *len);

int seefs_proc_read_status(pid_t pid, char **buf, size_t *len);

#endif

