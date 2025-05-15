#include "block.h"
#include <errno.h>
#include <unistd.h>

ssize_t read_block(int fd, uint32_t blk, void *out_buf) {
    off_t offset = (off_t)blk * SFUSE_BLOCK_SIZE;
    ssize_t n = pread(fd, out_buf, SFUSE_BLOCK_SIZE, offset);
    if (n < 0) {
        return -errno;
    }
    return n;
}

ssize_t write_block(int fd, uint32_t blk, const void *buf) {
    off_t offset = (off_t)blk * SFUSE_BLOCK_SIZE;
    ssize_t n = pwrite(fd, buf, SFUSE_BLOCK_SIZE, offset);
    if (n < 0) {
        return -errno;
    }
    return n;
}
