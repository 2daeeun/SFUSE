// File: src/super.c
#include "super.h"
#include <errno.h>
#include <sys/types.h> // ssize_t 선언
#include <unistd.h>

int sb_load(int fd, struct sfuse_superblock *sb_out) {
  // ssize_t n = pread(fd, sb_out, SFUSE_BLOCK_SIZE, 0);
  ssize_t n = pread(fd, sb_out, sizeof(*sb_out), 0);
  if (n < 0) {
    return -errno;
  }
  /* Validate superblock */
  if (sb_out->magic != SFUSE_MAGIC) {
    return -EINVAL;
  }
  return 0;
}

int sb_sync(int fd, const struct sfuse_superblock *sb) {
  // ssize_t n = pwrite(fd, sb, SFUSE_BLOCK_SIZE, 0);
  ssize_t n = pwrite(fd, sb, sizeof(*sb), 0);
  if (n < 0) {
    return -errno;
  }
  // return (n == SFUSE_BLOCK_SIZE ? 0 : -EIO);
  return (n == sizeof(*sb)     ? 0 : -EIO);
}
