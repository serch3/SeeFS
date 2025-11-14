// file.c - FUSE file operations (open, read) for SeeFS (e.g. /hello, /users/..../cmdline).

#include "include/seefs.h"

/**
 * Copy a slice from a buffer honoring the requested offset and size.
 */
static int seefs_copy_slice(const char *src, size_t src_len, char *dst,
			       size_t dst_len, off_t offset)
{
	if ((size_t) offset >= src_len)
		return 0;

	size_t slice = dst_len;
	size_t available = src_len - (size_t) offset;
	if (slice > available)
		slice = available;

	memcpy(dst, src + offset, slice);
	return (int) slice;
}

/**
 * Read from a static string buffer.
 */
static int seefs_read_static(const char *content, char *buf, size_t size,
                             off_t offset)
{
	return seefs_copy_slice(content, strlen(content), buf, size, offset);
}

/**
 * Route process data file reads to the appropriate /proc reader.
 */
static int seefs_read_process_node(const struct seefs_path_info *info,
                                   char **out, size_t *len_out)
{
	if (strcmp(info->data_file, "cmdline") == 0)
		return seefs_proc_read_cmdline(info->pid, out, len_out);
	
	if (strcmp(info->data_file, "status") == 0)
		return seefs_proc_read_status(info->pid, out, len_out);
	
	return -ENOENT;
}

/**
 * FUSE open callback. Enforces read-only access.
 */
int seefs_file_open(const char *path, struct fuse_file_info *fi)
{
	struct seefs_path_info info;
	
	if (!seefs_parse_path(path, &info))
		return -ENOENT;

	// Enforce read-only access
	if ((fi->flags & O_ACCMODE) != O_RDONLY)
		return -EACCES;

	switch (info.type) {
	case SEEFS_NODE_HELLO:
		return 0;
		
	case SEEFS_NODE_DATA_FILE: {
		// Verify the process still exists and cache PID in file handle
		int rc = seefs_pid_matches(&info, NULL);
		if (rc != 0)
			return rc;
		
		// Store validated PID in file handle for use in read()
		fi->fh = (uint64_t)info.pid;
		return 0;
	}
		
	default:
		return -EISDIR;
	}
}

/**
 * FUSE read callback. Reads file content from static or /proc sources.
 */
int seefs_file_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
	struct seefs_path_info info;
	
	if (!seefs_parse_path(path, &info))
		return -ENOENT;

	switch (info.type) {
	case SEEFS_NODE_HELLO:
		return seefs_read_static(SEEFS_HELLO_CONTENT, buf, size, offset);
		
	case SEEFS_NODE_DATA_FILE: {
		int rc;
		char *data = NULL;
		size_t len = 0;
		
		// Use cached PID from file handle (set during open)
		pid_t cached_pid = (pid_t)fi->fh;
		info.pid = cached_pid;

		rc = seefs_read_process_node(&info, &data, &len);
		if (rc != 0)
			return rc;

		int bytes_read = seefs_copy_slice(data, len, buf, size, offset);
		free(data);
		return bytes_read;
	}
	
	default:
		return -EISDIR;
	}
}
