// File: src/inode.c

#include "inode.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>

// 정적 dirty 배열 추가 (inode 번호를 인덱스로 사용)
static struct {
  int dirty;
  struct sfuse_inode inode;
} inode_dirty_cache[SFUSE_MAX_INODES];

// inode를 dirty 상태로 마킹하는 함수 구현
void inode_mark_dirty(uint32_t ino, struct sfuse_inode *inode) {
  if (ino == 0 || ino >= SFUSE_MAX_INODES)
    return;

  inode_dirty_cache[ino].dirty = 1;
  memcpy(&inode_dirty_cache[ino].inode, inode, sizeof(struct sfuse_inode));
}

// inode가 dirty 상태인지 확인하고 디스크에 기록하는 함수 구현
int inode_sync_if_dirty(int fd, struct sfuse_superblock *sb, uint32_t ino) {
  if (ino == 0 || ino >= SFUSE_MAX_INODES)
    return -EINVAL;

  if (inode_dirty_cache[ino].dirty) {
    if (inode_sync(fd, sb, ino, &inode_dirty_cache[ino].inode) < 0)
      return -EIO;

    inode_dirty_cache[ino].dirty = 0; // 기록 후 dirty 해제
  }

  return 0;
}

/**
 * @brief 아이노드 테이블 내 특정 아이노드의 디스크 내 바이트 오프셋을 계산
 *
 * @param sb  슈퍼블록 포인터
 * @param ino 오프셋을 계산할 아이노드 번호
 * @return 디스크 이미지 내 바이트 단위 오프셋 값
 */
static off_t inode_offset(const struct sfuse_superblock *sb, uint32_t ino) {
  uint32_t inode_index = ino;
  off_t block_index =
      sb->inode_table_start + inode_index / SFUSE_INODES_PER_BLOCK;
  off_t offset_within_block =
      (inode_index % SFUSE_INODES_PER_BLOCK) * sizeof(struct sfuse_inode);
  return block_index * SFUSE_BLOCK_SIZE + offset_within_block;
}

/**
 * @brief 디스크에서 지정한 아이노드 번호의 아이노드 구조체를 읽어 옴
 *
 * @param fd  디스크 이미지 파일 디스크립터
 * @param sb  슈퍼블록 포인터
 * @param ino 읽을 아이노드 번호
 * @param out 읽어들인 아이노드 데이터를 저장할 구조체 포인터
 * @return 성공 시 0, 실패 시 음수(errno 또는 -EIO)
 */
int inode_load(int fd, const struct sfuse_superblock *sb, uint32_t ino,
               struct sfuse_inode *out) {
  off_t offs = inode_offset(sb, ino);
  ssize_t n = pread(fd, out, sizeof(struct sfuse_inode), offs);
  if (n < 0) {
    return -errno;
  }
  return (n == sizeof(struct sfuse_inode) ? 0 : -EIO);
}

/**
 * @brief 아이노드 구조체를 디스크에 동기화(쓰기)함
 *
 * @param fd  디스크 이미지 파일 디스크립터
 * @param sb  슈퍼블록 포인터
 * @param ino 저장할 아이노드 번호
 * @param in  디스크에 기록할 아이노드 데이터 구조체 포인터
 * @return 성공 시 0, 실패 시 음수(errno 또는 -EIO)
 */
int inode_sync(int fd, const struct sfuse_superblock *sb, uint32_t ino,
               const struct sfuse_inode *in) {
  off_t offs = inode_offset(sb, ino);
  ssize_t n = pwrite(fd, in, sizeof(struct sfuse_inode), offs);
  if (n < 0) {
    return -errno;
  }
  return (n == sizeof(struct sfuse_inode) ? 0 : -EIO);
}
