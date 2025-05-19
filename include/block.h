// File: include/block.h

#ifndef SFUSE_BLOCK_H
#define SFUSE_BLOCK_H

#include <stdint.h>
#include <sys/types.h>

/**
 * @brief 지정한 블록 번호의 데이터를 읽어 버퍼에 저장
 * @param fd 파일 디스크립터
 * @param blk 읽을 블록 번호
 * @param out_buf 데이터를 저장할 버퍼 포인터
 * @return 읽은 바이트 수 또는 오류 시 음수 값
 */
ssize_t read_block(int fd, uint32_t blk, void *out_buf);

/**
 * @brief 버퍼의 내용을 지정한 블록 번호에 기록
 * @param fd 파일 디스크립터
 * @param blk 기록할 블록 번호
 * @param buf 기록할 데이터가 담긴 버퍼 포인터
 * @return 기록한 바이트 수 또는 오류 시 음수 값
 */
ssize_t write_block(int fd, uint32_t blk, const void *buf);

#endif /* SFUSE_BLOCK_H */
