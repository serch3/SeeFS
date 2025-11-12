// proc_data.c - Readers for /proc process information.
//
// Provides functions to iterate /proc, fetch process metadata, and read
// data files like cmdline and status.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define _POSIX_C_SOURCE 200809L

#include "include/seefs.h"

/* ========================================================================
 * String Utilities
 * ======================================================================== */

/**
 * Sanitize a process name for use as a directory component.
 * Replaces slashes and control characters with underscores.
 */
static void seefs_sanitize_component(const char *input, bool drop_brackets,
                                     char *out, size_t out_size)
{
	size_t out_pos = 0;

	for (size_t i = 0; input[i] != '\0' && out_pos + 1 < out_size; ++i) {
		char c = input[i];

		if (drop_brackets) {
			if ((i == 0 && c == '[') || (input[i + 1] == '\0' && c == ']'))
				continue;
		}

		if (c == '/' || iscntrl((unsigned char) c))
			c = '_';

		out[out_pos++] = c;
	}

	out[out_pos] = '\0';

	if (out_pos == 0)
		seefs_copy_string(out, out_size, "unknown");
}

/* ========================================================================
 * /proc File Reading
 * ======================================================================== */

/**
 * Read an entire file into a dynamically allocated buffer.
 * The caller must free the buffer.
 */
static int seefs_read_file_into_buffer(const char *path, char **buf,
                                       size_t *len)
{
	FILE *src = fopen(path, "re");
	if (!src)
		return -errno;

	*buf = NULL;
	*len = 0;

	FILE *mem = open_memstream(buf, len);
	if (!mem) {
		int err = -errno;
		fclose(src);
		return err;
	}

	char chunk[SEEFS_INIT_BUF_SIZE];
	int rc = 0;

	while (!feof(src)) {
		size_t n = fread(chunk, 1, sizeof(chunk), src);
		if (n > 0) {
			if (fwrite(chunk, 1, n, mem) != n) {
				rc = -errno;
				break;
			}
		}
		if (n < sizeof(chunk)) {
			if (ferror(src) && rc == 0)
				rc = -errno;
			break;
		}
	}

	if (fclose(src) != 0 && rc == 0)
		rc = -errno;

	if (fclose(mem) != 0 && rc == 0)
		rc = -errno;

	if (rc != 0) {
		free(*buf);
		*buf = NULL;
		*len = 0;
	}

	return rc;
}

/* ========================================================================
 * Process Information Fetching
 * ======================================================================== */

/**
 * Fetch detailed information for a specific PID.
 * Gathers ownership, command name, and kernel thread status.
 */
int seefs_proc_info_fetch(pid_t pid, struct seefs_proc_info *info)
{
	char proc_path[PATH_MAX];
	struct stat st;

	snprintf(proc_path, sizeof(proc_path), "%s/%d", SEEFS_PROC_ROOT, pid);
	if (stat(proc_path, &st) == -1)
		return -errno;

	info->pid = pid;
	info->uid = st.st_uid;

	struct passwd pwd;
	struct passwd *pwd_result = NULL;
	char pw_buf[SEEFS_PW_BUF_SIZE];

	if (getpwuid_r(st.st_uid, &pwd, pw_buf, sizeof(pw_buf), &pwd_result) == 0 &&
	    pwd_result) {
		seefs_copy_string(info->username, sizeof(info->username),
		                  pwd_result->pw_name);
	} else {
		snprintf(info->username, sizeof(info->username), "%u",
		         (unsigned int) st.st_uid);
	}

	snprintf(proc_path, sizeof(proc_path), "%s/%d/comm", SEEFS_PROC_ROOT, pid);
	FILE *fp = fopen(proc_path, "r");
	if (!fp)
		return -errno;

	char *comm_line = NULL;
	size_t comm_cap = 0;
	ssize_t comm_len = getline(&comm_line, &comm_cap, fp);
	int read_errno = errno;
	fclose(fp);

	if (comm_len < 0) {
		free(comm_line);
		return read_errno ? -read_errno : -ENOENT;
	}

	if (comm_len > 0 && comm_line[comm_len - 1] == '\n')
		comm_line[comm_len - 1] = '\0';

	seefs_copy_string(info->comm, sizeof(info->comm), comm_line);
	free(comm_line);

	size_t comm_len_clean = strlen(info->comm);
	info->is_kernel_thread =
	    (comm_len_clean >= 2 && info->comm[0] == '[' &&
	     info->comm[comm_len_clean - 1] == ']');

	seefs_sanitize_component(info->comm, info->is_kernel_thread,
	                         info->group_name, sizeof(info->group_name));

	return 0;
}

/**
 * Iterate over all running processes in /proc.
 * Invokes a callback for each process found.
 */
int seefs_proc_iterate(int (*cb)(const struct seefs_proc_info *info,
                                 void *ctx),
                       void *ctx)
{
	DIR *dir = opendir(SEEFS_PROC_ROOT);
	if (!dir)
		return -errno;

	struct dirent *dent;
	int ret = 0;

	while ((dent = readdir(dir)) != NULL) {
		if (!isdigit((unsigned char) dent->d_name[0]))
			continue;

		char *endptr = NULL;
		long pid_val = strtol(dent->d_name, &endptr, 10);
		if (!endptr || *endptr != '\0' || pid_val <= 0)
			continue;

		struct seefs_proc_info info;
		if (seefs_proc_info_fetch((pid_t) pid_val, &info) != 0)
			continue;

		ret = cb(&info, ctx);
		if (ret)
			break;
	}

	closedir(dir);
	return ret;
}

/* ========================================================================
 * Process Data File Readers
 * ======================================================================== */

/**
 * Read and format the command line for a process.
 * Replaces null separators with spaces and ensures a trailing newline.
 */
int seefs_proc_read_cmdline(pid_t pid, char **buf, size_t *len)
{
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/%d/cmdline", SEEFS_PROC_ROOT, pid);

	int rc = seefs_read_file_into_buffer(path, buf, len);
	if (rc != 0)
		return rc;

	if (*len == 0) {
		static const char placeholder[] = "\n";
		free(*buf);
		*buf = malloc(sizeof(placeholder));
		if (!*buf)
			return -ENOMEM;
		memcpy(*buf, placeholder, sizeof(placeholder));
		*len = sizeof(placeholder) - 1;
		return 0;
	}

	size_t actual_len = *len;
	for (size_t i = 0; i < actual_len; ++i) {
		if ((*buf)[i] == '\0')
			(*buf)[i] = ' ';
	}

	while (actual_len > 0 && (*buf)[actual_len - 1] == ' ')
		--actual_len;

	if (actual_len + 1 >= *len) {
		char *extended = realloc(*buf, actual_len + 2);
		if (!extended) {
			free(*buf);
			return -ENOMEM;
		}
		*buf = extended;
	}

	(*buf)[actual_len++] = '\n';
	(*buf)[actual_len] = '\0';
	*len = actual_len;

	return 0;
}

/**
 * Read the raw status file for a process.
 */
int seefs_proc_read_status(pid_t pid, char **buf, size_t *len)
{
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/%d/status", SEEFS_PROC_ROOT, pid);
	return seefs_read_file_into_buffer(path, buf, len);
}
