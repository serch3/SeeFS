// seefs_main.c - Entry point.

#include "include/seefs.h"

int main(int argc, char *argv[])
{
	const struct fuse_operations *ops = seefs_get_operations();
	return fuse_main(argc, argv, ops, NULL);
}