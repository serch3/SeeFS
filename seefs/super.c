// super.c - FUSE operations registration.
//
// Defines the FUSE operations structure that maps callbacks to functions in inode.c and file.c.

#include "include/seefs.h"

static struct fuse_operations seefs_oper = {
	.getattr = seefs_inode_getattr,
	.readdir = seefs_inode_readdir,
	.open = seefs_file_open,
	.read = seefs_file_read,
};

/**
 * Get the FUSE operations structure.
 */
const struct fuse_operations *seefs_get_operations(void)
{
	return &seefs_oper;
}
