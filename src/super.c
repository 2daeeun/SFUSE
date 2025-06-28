// File: src/super.c

#include "super.h"
#include "img.h"
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * @brief 디스크에서 슈퍼블록을 읽어 로드
 *
 * 파일 디스크립터가 가리키는 디스크 이미지의 시작에서
 * 슈퍼블록 구조체 크기만큼 읽어 들이고, 매직 넘버를 확인한다.
 *
 * @param fd     디스크 이미지 파일 디스크립터
 * @param sb_out 읽어들인 슈퍼블록을 저장할 구조체 포인터
 * @return 성공 시 0, 매직 넘버 불일치 시 -EINVAL, 읽기 오류 시 -errno
 */
int sb_load(int fd, struct sfuse_superblock *sb_out) {
  /* 슈퍼블록 오프셋에서 읽기 */
  ssize_t n = img_read(sb_out, sizeof(*sb_out), SFUSE_SUPERBLOCK_OFFSET);
  if (n < 0) {
    return -errno;
  }
  /* 매직 넘버 확인 */
  if (sb_out->magic != SFUSE_MAGIC) {
    return -EINVAL;
  }
  return 0;
}

/**
 * @brief 메모리의 슈퍼블록을 디스크에 기록
 *
 * 슈퍼블록 구조체 크기만큼 디스크 이미지 시작 위치에 쓰기 수행.
 *
 * @param fd 파일 디스크립터
 * @param sb 기록할 슈퍼블록 구조체 포인터
 * @return 성공 시 0, 쓰기 오류 시 -errno 또는 -EIO
 */
int sb_sync(int fd, const struct sfuse_superblock *sb) {
  /* 슈퍼블록 오프셋에 기록 */
  ssize_t n = img_write(sb, sizeof(*sb), SFUSE_SUPERBLOCK_OFFSET);
  if (n < 0) {
    return -errno;
  }
  return (n == sizeof(*sb) ? 0 : -EIO);
}
