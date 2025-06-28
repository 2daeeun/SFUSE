/* SPDX-License-Identifier: GPL-2.0 */
/*
 * fd 기반 저수준 블록-I/O 유틸리티
 */

#include "img.h"
#include <unistd.h>

static int img_fd = -1;

void img_set_fd(int fd) { img_fd = fd; }

ssize_t img_read(void *buf, size_t nbytes, off_t offset) {
  return pread(img_fd, buf, nbytes, offset);
}

ssize_t img_write(const void *buf, size_t nbytes, off_t offset) {
  return pwrite(img_fd, buf, nbytes, offset);
}
