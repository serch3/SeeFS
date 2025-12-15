// seefs_main.c - Entry point.

#include "include/seefs.h"

int main(int argc, char *argv[])
{
	seefs_history_init();
	const struct fuse_operations *ops = seefs_get_operations();
	int ret = fuse_main(argc, argv, ops, NULL);
	seefs_history_shutdown();
	return ret;
}