// img.c: Block image I/O (low-level read/write)
#include "img.h"
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * @brief Read raw data from the backing device/file
 * @param fd      File descriptor of backing store
 * @param buf     Buffer to fill
 * @param count   Number of bytes to read
 * @param off     Offset in bytes from start of device
 * @return Number of bytes read on success, or negative errno on error
 */
ssize_t img_read(int fd, void *buf, size_t count, off_t off) {
  if (lseek(fd, off, SEEK_SET) < 0)
    return -errno;
  ssize_t ret = read(fd, buf, count);
  if (ret < 0)
    return -errno;
  return ret;
}

/**
 * @brief Write raw data to the backing device/file
 * @param fd      File descriptor of backing store
 * @param buf     Buffer containing data to write
 * @param count   Number of bytes to write
 * @param off     Offset in bytes from start of device
 * @return Number of bytes written on success, or negative errno on error
 */
ssize_t img_write(int fd, const void *buf, size_t count, off_t off) {
  if (lseek(fd, off, SEEK_SET) < 0)
    return -errno;
  ssize_t ret = write(fd, buf, count);
  if (ret < 0)
    return -errno;
  return ret;
}

// Note: read_block() and write_block() are implemented in block.c to avoid
// duplication.
