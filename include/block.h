// File: include/block.h
#ifndef SFUSE_BLOCK_H
#define SFUSE_BLOCK_H

// #include "super.h"
#include <stdint.h>
#include <sys/types.h>

/* 디스크에서 지정한 블록을 읽어 버퍼에 저장 */
ssize_t read_block(int fd, uint32_t blk, void *out_buf);

/* 버퍼의 내용을 지정한 디스크 블록에 기록 */
ssize_t write_block(int fd, uint32_t blk, const void *buf);

#endif /* SFUSE_BLOCK_H */
