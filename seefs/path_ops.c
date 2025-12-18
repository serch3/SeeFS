/**
 * path_ops.c - Directory hierarchy and path resolution.
 *
 * Implements the directory structure, path parsing, and FUSE inode operations
 * (getattr, readdir) by reading from /proc.
 */

#include "include/seefs.h"

#include <limits.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef S_IFDIR
#define S_IFDIR 0040000
#endif

#ifndef S_IFREG
#define S_IFREG 0100000
#endif

/* ========================================================================
 * Stat Helpers
 * ======================================================================== */

static void seefs_set_dir_attr(struct stat *stbuf)
{
	stbuf->st_mode = S_IFDIR | 0555;
	stbuf->st_nlink = 2;
}

static void seefs_set_file_attr(struct stat *stbuf, size_t size)
{
	stbuf->st_mode = S_IFREG | 0444;
	stbuf->st_nlink = 1;
	stbuf->st_size = size;
}

/* ========================================================================
 * Dynamic String Vector
 * 
 * Used for collecting unique directory entries (usernames, groups, PIDs)
 * during process enumeration. Grows automatically as items are added.
 * ======================================================================== */

struct seefs_string_vec {
	char **items;
	size_t count;
	size_t capacity;
};

static void seefs_vec_init(struct seefs_string_vec *vec)
{
	vec->capacity = 8;
	vec->items = malloc(8 * sizeof(char *));
	vec->count = 0;
	if (!vec->items) {
		vec->capacity = 0;
	}
}

static void seefs_vec_free(struct seefs_string_vec *vec)
{
	for (size_t i = 0; i < vec->count; ++i)
		free(vec->items[i]);
	free(vec->items);
	vec->items = NULL;
	vec->count = 0;
	vec->capacity = 0;
}

static int seefs_vec_grow(struct seefs_string_vec *vec)
{
	size_t new_cap = vec->capacity ? vec->capacity * 2 : 8;
	char **new_items = realloc(vec->items, new_cap * sizeof(char *));
	if (!new_items)
		return -ENOMEM;
	vec->items = new_items;
	vec->capacity = new_cap;
	return 0;
}

static int seefs_vec_append_unique(struct seefs_string_vec *vec,
                                   const char *value)
{
	for (size_t i = 0; i < vec->count; ++i) {
		if (strcmp(vec->items[i], value) == 0)
			return 0;
	}

	if (vec->count == vec->capacity) {
		int rc = seefs_vec_grow(vec);
		if (rc != 0)
			return rc;
	}

	char *copy = strdup(value);
	if (!copy)
		return -ENOMEM;

	vec->items[vec->count++] = copy;
	return 0;
}

static int seefs_string_cmp(const void *a, const void *b)
{
	const char *const *sa = a;
	const char *const *sb = b;
	return strcmp(*sa, *sb);
}

/* ========================================================================
 * Process Iteration Callback Contexts
 * 
 * These structs are passed to seefs_proc_iterate() to collect or validate
 * specific directory entries (usernames, groups, PIDs).
 * ======================================================================== */

// Forward declaration
static bool seefs_branch_matches(const struct seefs_proc_info *info,
                                 enum seefs_branch_type branch);

// Context for checking if a username exists in /proc
struct seefs_user_lookup_ctx {
	const char *username;
	bool found;
};

// Context for checking if a group exists under a specific user/branch
struct seefs_group_lookup_ctx {
	const char *username;
	enum seefs_branch_type branch;
	const char *group_name;
	bool found;
};

// Context for collecting unique group names under a user/branch
struct seefs_group_collect_ctx {
	const char *username;
	enum seefs_branch_type branch;
	struct seefs_string_vec *vec;
};

// Context for collecting PIDs under a user/branch/group
struct seefs_pid_collect_ctx {
	const char *username;
	enum seefs_branch_type branch;
	const char *group_name;
	struct seefs_string_vec *vec;
};

/* ========================================================================
 * Validation Helpers
 * ======================================================================== */

static int seefs_require_user(const struct seefs_path_info *info)
{
	return seefs_user_exists(info->username);
}

static int seefs_require_group(const struct seefs_path_info *info)
{
	return seefs_group_exists(info->username, info->branch, info->group);
}

static int seefs_require_pid(const struct seefs_path_info *info)
{
	return seefs_pid_matches(info, NULL);
}

/* ========================================================================
 * Process Iteration Callbacks
 * 
 * These functions are invoked for each process during /proc enumeration.
 * ======================================================================== */

static int seefs_username_collect_cb(const struct seefs_proc_info *info,
					  void *ctx)
{
	struct seefs_string_vec *vec = ctx;
	return seefs_vec_append_unique(vec, info->username);
}

static int seefs_user_iter_cb(const struct seefs_proc_info *info, void *ctx)
{
	struct seefs_user_lookup_ctx *user_ctx = ctx;
	if (strcmp(info->username, user_ctx->username) == 0) {
		user_ctx->found = true;
		return 1;
	}
	return 0;
}

static int seefs_group_iter_cb(const struct seefs_proc_info *info, void *ctx)
{
	struct seefs_group_lookup_ctx *group_ctx = ctx;
	if (strcmp(info->username, group_ctx->username) != 0)
		return 0;
	if (!seefs_branch_matches(info, group_ctx->branch))
		return 0;
	if (strcmp(info->group_name, group_ctx->group_name) != 0)
		return 0;
	group_ctx->found = true;
	return 1;
}

static int seefs_group_collect_cb(const struct seefs_proc_info *info,
                                  void *ctx)
{
	struct seefs_group_collect_ctx *collect_ctx = ctx;
	if (strcmp(info->username, collect_ctx->username) != 0)
		return 0;
	if (!seefs_branch_matches(info, collect_ctx->branch))
		return 0;
	return seefs_vec_append_unique(collect_ctx->vec, info->group_name);
}

static int seefs_pid_collect_cb(const struct seefs_proc_info *info, void *ctx)
{
	struct seefs_pid_collect_ctx *collect_ctx = ctx;
	if (strcmp(info->username, collect_ctx->username) != 0)
		return 0;
	if (!seefs_branch_matches(info, collect_ctx->branch))
		return 0;
	if (strcmp(info->group_name, collect_ctx->group_name) != 0)
		return 0;

	char pid_str[SEEFS_PID_STR_SIZE];
	snprintf(pid_str, sizeof(pid_str), "%d", (int) info->pid);
	return seefs_vec_append_unique(collect_ctx->vec, pid_str);
}

/* ========================================================================
 * Path Parsing
 * ======================================================================== */

/* ========================================================================
 * Path Validation Functions
 * ======================================================================== */

static bool seefs_branch_matches(const struct seefs_proc_info *info,
                                 enum seefs_branch_type branch)
{
	if (branch == SEEFS_BRANCH_APPLICATIONS)
		return !info->is_kernel_thread;
	if (branch == SEEFS_BRANCH_KERNEL_THREADS)
		return info->is_kernel_thread;
	return false;
}

int seefs_user_exists(const char *username)
{
	struct seefs_user_lookup_ctx ctx = {
		.username = username,
		.found = false,
	};

	int rc = seefs_proc_iterate(seefs_user_iter_cb, &ctx);

	if (rc < 0)
		return rc;
	return ctx.found ? 0 : -ENOENT;
}

int seefs_group_exists(const char *username, enum seefs_branch_type branch,
                      const char *group_name)
{
	struct seefs_group_lookup_ctx ctx = {
		.username = username,
		.branch = branch,
		.group_name = group_name,
		.found = false,
	};

	int rc = seefs_proc_iterate(seefs_group_iter_cb, &ctx);

	if (rc < 0)
		return rc;
	return ctx.found ? 0 : -ENOENT;
}

/**
 * Validate that a PID matches the expected user/branch/group hierarchy.
 *
 * Ensures that the process with the given PID actually belongs to the
 * specified user, branch, and group from the path.
 */
int seefs_pid_matches(const struct seefs_path_info *info,
                      struct seefs_proc_info *proc_out)
{
	struct seefs_proc_info proc;
	int rc = seefs_proc_info_fetch(info->pid, &proc);
	if (rc != 0)
		return rc;

	if (strcmp(proc.username, info->username) != 0)
		return -ENOENT;
	if (!seefs_branch_matches(&proc, info->branch))
		return -ENOENT;
	if (strcmp(proc.group_name, info->group) != 0)
		return -ENOENT;

	if (proc_out)
		*proc_out = proc;
	return 0;
}

/* ========================================================================
 * Directory Population Functions
 * ======================================================================== */

/**
 * Populate /users directory with all unique usernames from /proc.
 */
static int seefs_fill_usernames(void *buf, fuse_fill_dir_t filler)
{
	struct seefs_string_vec vec;
	seefs_vec_init(&vec);

	int rc = seefs_proc_iterate(seefs_username_collect_cb, &vec);

	if (rc < 0) {
		seefs_vec_free(&vec);
		return rc;
	}

	qsort(vec.items, vec.count, sizeof(char *), seefs_string_cmp);

	for (size_t i = 0; i < vec.count; ++i)
		filler(buf, vec.items[i], NULL, 0);

	seefs_vec_free(&vec);
	return 0;
}

/**
 * Populate branch directory with all unique group names.
 */
static int seefs_fill_groups(const char *username, enum seefs_branch_type branch,
                             void *buf, fuse_fill_dir_t filler)
{
	struct seefs_string_vec vec;
	seefs_vec_init(&vec);

	struct seefs_group_collect_ctx ctx = {
		.username = username,
		.branch = branch,
		.vec = &vec,
	};

	int rc = seefs_proc_iterate(seefs_group_collect_cb, &ctx);

	if (rc < 0) {
		seefs_vec_free(&vec);
		return rc;
	}

	qsort(vec.items, vec.count, sizeof(char *), seefs_string_cmp);
	for (size_t i = 0; i < vec.count; ++i)
		filler(buf, vec.items[i], NULL, 0);

	seefs_vec_free(&vec);
	return 0;
}

/**
 * Populate group directory with all PIDs belonging to the group.
 */
static int seefs_fill_pids(const char *username, enum seefs_branch_type branch,
                           const char *group_name, void *buf,
                           fuse_fill_dir_t filler)
{
	struct seefs_string_vec vec;
	seefs_vec_init(&vec);

	struct seefs_pid_collect_ctx ctx = {
		.username = username,
		.branch = branch,
		.group_name = group_name,
		.vec = &vec,
	};

	int rc = seefs_proc_iterate(seefs_pid_collect_cb, &ctx);

	if (rc < 0) {
		seefs_vec_free(&vec);
		return rc;
	}

	qsort(vec.items, vec.count, sizeof(char *), seefs_string_cmp);
	for (size_t i = 0; i < vec.count; ++i)
		filler(buf, vec.items[i], NULL, 0);

	seefs_vec_free(&vec);
	return 0;
}

/**
 * Add standard "." and ".." entries to directory listing.
 */
static void seefs_fill_common_directory_entries(void *buf, fuse_fill_dir_t filler)
{
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
}

/* ========================================================================
 * FUSE Callback Implementations
 * ======================================================================== */

/**
 * FUSE getattr callback: return file/directory attributes.
 *
 * Validates that the path corresponds to an existing entry in the /proc-based
 * hierarchy and returns its attributes (mode, size, etc.).
 */
int seefs_inode_getattr(const char *path, struct stat *stbuf)
{
	struct seefs_path_info info;
	if (!seefs_parse_path(path, &info))
		return -ENOENT;

	memset(stbuf, 0, sizeof(*stbuf));

	switch (info.type) {
	case SEEFS_NODE_ROOT:
		seefs_set_dir_attr(stbuf);
		return 0;
	case SEEFS_NODE_HELLO:
		seefs_set_file_attr(stbuf, strlen(SEEFS_HELLO_CONTENT));
		return 0;
	case SEEFS_NODE_USERS:
		seefs_set_dir_attr(stbuf);
		return 0;
	case SEEFS_NODE_USER: {
		int rc = seefs_require_user(&info);
		if (rc != 0)
			return rc;
		seefs_set_dir_attr(stbuf);
		return 0;
	}
	case SEEFS_NODE_BRANCH: {
		int rc = seefs_require_user(&info);
		if (rc != 0)
			return rc;
		seefs_set_dir_attr(stbuf);
		return 0;
	}
	case SEEFS_NODE_GROUP: {
		int rc = seefs_require_group(&info);
		if (rc != 0)
			return rc;
		seefs_set_dir_attr(stbuf);
		return 0;
	}
	case SEEFS_NODE_PID: {
		int rc = seefs_require_pid(&info);
		if (rc != 0)
			return rc;
		seefs_set_dir_attr(stbuf);
		return 0;
	}
	case SEEFS_NODE_DATA_FILE: {
		int rc = seefs_require_pid(&info);
		if (rc != 0)
			return rc;
		// Report a non-zero size so applications will try to read it.
		seefs_set_file_attr(stbuf, 4096);
		return 0;
	}
	case SEEFS_NODE_HISTORY: {
		int rc = seefs_require_pid(&info);
		if (rc != 0)
			return rc;
		seefs_set_dir_attr(stbuf);
		return 0;
	}
	case SEEFS_NODE_TIMESTAMP: {
		int rc = seefs_require_pid(&info);
		if (rc != 0)
			return rc;
		seefs_set_dir_attr(stbuf);
		return 0;
	}
	case SEEFS_NODE_HISTORY_FILE: {
		int rc = seefs_require_pid(&info);
		if (rc != 0)
			return rc;
		seefs_set_file_attr(stbuf, 4096);
		return 0;
	}
	default:
		break;
	}

	return -ENOENT;
}

/**
 * FUSE readdir callback: populate directory listings.
 *
 * Fills the directory listing based on the path type, either with static
 * entries or by dynamically collecting entries from /proc.
 */
int seefs_inode_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	struct seefs_path_info info;
	if (!seefs_parse_path(path, &info))
		return -ENOENT;

	switch (info.type) {
	case SEEFS_NODE_ROOT:
		seefs_fill_common_directory_entries(buf, filler);
		filler(buf, SEEFS_HELLO_PATH + 1, NULL, 0);
		filler(buf, "users", NULL, 0);
		return 0;
	case SEEFS_NODE_USERS:
		seefs_fill_common_directory_entries(buf, filler);
		return seefs_fill_usernames(buf, filler);
	case SEEFS_NODE_USER: {
		int rc = seefs_require_user(&info);
		if (rc != 0)
			return rc;
		seefs_fill_common_directory_entries(buf, filler);
		filler(buf, "applications", NULL, 0);
		filler(buf, "kernel_threads", NULL, 0);
		return 0;
	}
	case SEEFS_NODE_BRANCH: {
		int rc = seefs_require_user(&info);
		if (rc != 0)
			return rc;
		seefs_fill_common_directory_entries(buf, filler);
		return seefs_fill_groups(info.username, info.branch, buf, filler);
	}
	case SEEFS_NODE_GROUP: {
		int rc = seefs_require_group(&info);
		if (rc != 0)
			return rc;
		seefs_fill_common_directory_entries(buf, filler);
		return seefs_fill_pids(info.username, info.branch, info.group, buf,
		                       filler);
	}
	case SEEFS_NODE_PID: {
		int rc = seefs_require_pid(&info);
		if (rc != 0)
			return rc;
		seefs_fill_common_directory_entries(buf, filler);
		filler(buf, "cmdline", NULL, 0);
		filler(buf, "status", NULL, 0);
		filler(buf, "history", NULL, 0);
		return 0;
	}
	case SEEFS_NODE_HISTORY: {
		int rc = seefs_require_pid(&info);
		if (rc != 0)
			return rc;

		seefs_fill_common_directory_entries(buf, filler);
		char **timestamps = NULL;
		size_t count = 0;
		rc = seefs_history_get_timestamps(info.pid, &timestamps, &count);
		if (rc != 0)
			return rc;
		{
			for (size_t i = 0; i < count; ++i) {
				filler(buf, timestamps[i], NULL, 0);
				free(timestamps[i]);
			}
			free(timestamps);
		}
		return 0;
	}
	case SEEFS_NODE_TIMESTAMP: {
		int rc = seefs_require_pid(&info);
		if (rc != 0)
			return rc;
		seefs_fill_common_directory_entries(buf, filler);
		filler(buf, "cmdline", NULL, 0);
		filler(buf, "status", NULL, 0);
		return 0;
	}
	default:
		break;
	}

	return -ENOTDIR;
}
