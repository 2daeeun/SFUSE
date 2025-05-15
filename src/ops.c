// === src/ops.c (Updated for FUSE 3.17.1) ===

#define FUSE_USE_VERSION 31
#include "fs.h"
#include <dirent.h>
#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// getattr: retrieve file attributes
int sfuse_getattr(const char *path, struct stat *stbuf,
                  struct fuse_file_info *fi) {
  (void)fi;
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_getattr(fs, path, stbuf);
}

// readdir: read directory entries
int sfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info *fi,
                  enum fuse_readdir_flags flags) {
  (void)fi;
  (void)flags;
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_readdir(fs, path, buf, filler, offset);
}

// open: open a file
int sfuse_open(const char *path, struct fuse_file_info *fi) {
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_open(fs, path, fi);
}

// read: read data from file
typedef ssize_t (*read_func_t)(struct sfuse_fs *, const char *, char *, size_t,
                               off_t);
int sfuse_read(const char *path, char *buf, size_t size, off_t offset,
               struct fuse_file_info *fi) {
  (void)fi;
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_read(fs, path, buf, size, offset);
}

// write: write data to file
int sfuse_write(const char *path, const char *buf, size_t size, off_t offset,
                struct fuse_file_info *fi) {
  (void)fi;
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_write(fs, path, buf, size, offset);
}

// create file
int sfuse_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_create(fs, path, mode, fi);
}

// mkdir: create directory
int sfuse_mkdir(const char *path, mode_t mode) {
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_mkdir(fs, path, mode);
}

// unlink: remove file
int sfuse_unlink(const char *path) {
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_unlink(fs, path);
}

// rmdir: remove directory
int sfuse_rmdir(const char *path) {
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_rmdir(fs, path);
}

// rename: rename file or directory
int sfuse_rename(const char *from, const char *to, unsigned int flags) {
  (void)flags; // handle flags if needed
  struct sfuse_fs *fs = (struct sfuse_fs *)fuse_get_context()->private_data;
  return fs_rename(fs, from, to);
}

// utimens: update file timestamps
int sfuse_utimens(const char *path, const struct timespec tv[2],
                  struct fuse_file_info *fi) {
  (void)fi;
  // delegate to system utimensat
  if (utimensat(AT_FDCWD, path, tv, 0) == -1)
    return -errno;
  return 0;
}

// Define and register operations
typedef struct fuse_operations fuse_ops_t;
struct fuse_operations *sfuse_get_operations(void) {
  static fuse_ops_t sfuse_oper = {
      .getattr = sfuse_getattr,
      .readdir = sfuse_readdir,
      .open = sfuse_open,
      .read = sfuse_read,
      .write = sfuse_write,
      .create = sfuse_create,
      .mkdir = sfuse_mkdir,
      .unlink = sfuse_unlink,
      .rmdir = sfuse_rmdir,
      .rename = sfuse_rename,
      .utimens = sfuse_utimens,
  };
  return &sfuse_oper;
}
