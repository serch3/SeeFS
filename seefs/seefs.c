// seefs.c - module entry/exit + filesystem registration

#define FUSE_USE_VERSION 31
#include "include/seefs.h"

// The string to be displayed in the 'hello' file.
static const char *hello_str = "Hello, World!\n";
// The path to the 'hello' file.
static const char *hello_path = "/hello";

/**
 * @brief Get file attributes.
 *
 * @param path The path to the file.
 * @param stbuf The stat structure to fill.
 * @return 0 on success, or a negative error code.
 */
static int seefs_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (strcmp(path, hello_path) == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen(hello_str);
    } else {
        res = -ENOENT;
    }

    return res;
}

/**
 * @brief Read directory entries.
 *
 * @param path The path to the directory.
 * @param buf The buffer to fill with directory entries.
 * @param filler The function to add entries to the buffer.
 * @param offset The offset to start reading from.
 * @param fi The fuse file info structure.
 * @return 0 on success, or a negative error code.
 */
static int seefs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, hello_path + 1, NULL, 0);

    return 0;
}

/**
 * @brief Open a file.
 *
 * @param path The path to the file.
 * @param fi The fuse file info structure.
 * @return 0 on success, or a negative error code.
 */
static int seefs_open(const char *path, struct fuse_file_info *fi)
{
    if (strcmp(path, hello_path) != 0)
        return -ENOENT;

    if ((fi->flags & O_ACCMODE) != O_RDONLY)
        return -EACCES;

    return 0;
}

/**
 * @brief Read data from a file.
 *
 * @param path The path to the file.
 * @param buf The buffer to read data into.
 * @param size The number of bytes to read.
 * @param offset The offset to start reading from.
 * @param fi The fuse file info structure.
 * @return The number of bytes read, or a negative error code.
 */
static int seefs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
    size_t len;
    (void) fi;
    if(strcmp(path, hello_path) != 0)
        return -ENOENT;

    len = strlen(hello_str);
    if (offset < len) {
        if (offset + size > len)
            size = len - offset;
        memcpy(buf, hello_str + offset, size);
    } else {
        size = 0;
    }

    return size;
}

// The FUSE operations structure.
static struct fuse_operations seefs_oper = {
    .getattr    = seefs_getattr,
    .readdir    = seefs_readdir,
    .open       = seefs_open,
    .read       = seefs_read,
};

/**
 * @brief The main function.
 *
 * @param argc The number of command-line arguments.
 * @param argv The command-line arguments.
 * @return The exit code of the FUSE main loop.
 */
int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &seefs_oper, NULL);
}
