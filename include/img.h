/* SPDX-License-Identifier: GPL-2.0 */
#ifndef IMG_H
#define IMG_H

#include <stddef.h>
#include <sys/types.h>

/* 외부에서 fd 주입 */
void img_set_fd(int fd);

/* 저수준 I/O 래퍼 */
ssize_t img_read(void *buf, size_t nbytes, off_t offset);
ssize_t img_write(const void *buf, size_t nbytes, off_t offset);

#endif /* IMG_H */
