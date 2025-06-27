// File: src/block.c

#include "block.h"
#include "super.h"
#include <errno.h>
#include <unistd.h>

/**
 * @brief 지정한 블록 번호의 데이터를 읽어 버퍼에 저장
 * @param fd 파일 디스크립터
 * @param blk 읽어올 블록 번호
 * @param out_buf 데이터를 저장할 버퍼 포인터
 * @return 읽은 바이트 수, 실패 시 음수 오류 코드
 */
ssize_t read_block(int fd, uint32_t blk, void *out_buf) {
  off_t offset = (off_t)blk * SFUSE_BLOCK_SIZE;
  ssize_t n = pread(fd, out_buf, SFUSE_BLOCK_SIZE, offset);
  if (n < 0) {
    return -errno;
  }
  return n;
}

/**
 * @brief 버퍼의 내용을 지정한 블록 번호에 기록
 * @param fd 파일 디스크립터
 * @param blk 기록할 블록 번호
 * @param buf 기록할 데이터가 담긴 버퍼 포인터
 * @return 기록한 바이트 수, 실패 시 음수 오류 코드
 */
ssize_t write_block(int fd, uint32_t blk, const void *buf) {
  off_t offset = (off_t)blk * SFUSE_BLOCK_SIZE;
  ssize_t n = pwrite(fd, buf, SFUSE_BLOCK_SIZE, offset);
  if (n < 0) {
    return -errno;
  }
  return n;
}
