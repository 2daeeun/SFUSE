// block.c: 블록 단위 I/O 헬퍼 함수 구현
#include "block.h"
#include "img.h"
#include "super.h" // SFUSE_BLOCK_SIZE
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

/**
 * @brief 디바이스에서 블록 단위(4KB)로 읽기
 * @param fd        디바이스 파일 디스크립터
 * @param block_no  읽을 블록 번호 (0부터 시작)
 * @param buf       SFUSE_BLOCK_SIZE 바이트 버퍼
 * @return 0 성공, 음수 오류코드
 */
int read_block(int fd, uint32_t block_no, void *buf) {
  off_t offset = (off_t)block_no * SFUSE_BLOCK_SIZE;
  ssize_t ret = img_read(fd, buf, SFUSE_BLOCK_SIZE, offset);
  if (ret < 0)
    return (int)ret;
  if ((size_t)ret != SFUSE_BLOCK_SIZE)
    return -EIO;
  return 0;
}

/**
 * @brief 디바이스에 블록 단위(4KB)로 쓰기
 * @param fd        디바이스 파일 디스크립터
 * @param block_no  쓸 블록 번호 (0부터 시작)
 * @param buf       SFUSE_BLOCK_SIZE 바이트 데이터 버퍼
 * @return 0 성공, 음수 오류코드
 */
int write_block(int fd, uint32_t block_no, const void *buf) {
  off_t offset = (off_t)block_no * SFUSE_BLOCK_SIZE;
  ssize_t ret = img_write(fd, buf, SFUSE_BLOCK_SIZE, offset);
  if (ret < 0)
    return (int)ret;
  if ((size_t)ret != SFUSE_BLOCK_SIZE)
    return -EIO;
  return 0;
}
