// file.c - FUSE file operations (open, read) for SeeFS.
// Handles read-only access for static and /proc-based files.

#include "include/seefs.h"

/**
 * Read from a static string buffer.
 */
static int seefs_read_static(const char *content, char *buf, size_t size,
                             off_t offset)
{
	size_t len = strlen(content);
	
	if ((size_t) offset >= len)
		return 0;

	if (offset + size > len)
		size = len - offset;

	memcpy(buf, content + offset, size);
	return (int) size;
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

		int bytes_read = 0;
		if ((size_t) offset < len) {
			size_t slice = size;
			if (offset + slice > len)
				slice = len - offset;
			memcpy(buf, data + offset, slice);
			bytes_read = (int) slice;
		}

		free(data);
		return bytes_read;
	}
	
	default:
		return -EISDIR;
	}
}
