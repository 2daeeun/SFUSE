// bitmap.c: 아이노드/블록 비트맵 로드, 동기화 및 할당 함수 구현
#include "bitmap.h"
#include "img.h"
#include "super.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

/**
 * bitmap_load: 비트맵 블록에서 맵 데이터를 읽어들임
 * @fd: 디바이스 파일 디스크립터
 * @block_no: 비트맵이 저장된 블록 번호
 * @map: 읽어들인 비트맵을 저장할 버퍼
 * @map_size: 버퍼 크기(바이트)
 * @return: 0 성공, 음수 오류
 */
int bitmap_load(int fd, uint32_t block_no, uint8_t *map, size_t map_size) {
  ssize_t ret = img_read(fd, map, map_size, (off_t)block_no * SFUSE_BLOCK_SIZE);
  if (ret < 0)
    return (int)ret;
  if ((size_t)ret != map_size)
    return -EIO;
  return 0;
}

/**
 * bitmap_sync: 비트맵 버퍼를 디스크에 기록
 * @fd: 디바이스 파일 디스크립터
 * @block_no: 비트맵 블록 번호
 * @map: 기록할 비트맵 버퍼
 * @map_size: 버퍼 크기(바이트)
 * @return: 0 성공, 음수 오류
 */
int bitmap_sync(int fd, uint32_t block_no, uint8_t *map, size_t map_size) {
  ssize_t ret =
      img_write(fd, map, map_size, (off_t)block_no * SFUSE_BLOCK_SIZE);
  if (ret < 0)
    return (int)ret;
  if ((size_t)ret != map_size)
    return -EIO;
  return 0;
}

/**
 * alloc_block: 새로운 데이터 블록을 할당
 * @sb: 슈퍼블록 구조체
 * @block_map: 블록 비트맵 버퍼
 * @return: 할당된 블록 오프셋(0부터), 실패 시 -ENOSPC
 */
int alloc_block(struct sfuse_super *sb, uint8_t *block_map) {
  uint32_t total = sb->blocks_count - sb->data_block_start;
  for (uint32_t i = 0; i < total; i++) {
    uint32_t byte_idx = i / 8;
    uint32_t bit_idx = i % 8;
    if (!(block_map[byte_idx] & (1 << bit_idx))) {
      block_map[byte_idx] |= (1 << bit_idx);
      sb->free_blocks--;
      return (int)i;
    }
  }
  return -ENOSPC;
}

/**
 * free_block: 데이터 블록을 해제
 * @sb: 슈퍼블록 구조체
 * @block_map: 블록 비트맵 버퍼
 * @offset: 해제할 블록 오프셋(0부터)
 */
void free_block(struct sfuse_super *sb, uint8_t *block_map, uint32_t offset) {
  uint32_t byte_idx = offset / 8;
  uint32_t bit_idx = offset % 8;
  block_map[byte_idx] &= ~(1 << bit_idx);
  sb->free_blocks++;
}

/**
 * alloc_inode: 새로운 아이노드를 할당
 * @sb: 슈퍼블록 구조체
 * @inode_map: 아이노드 비트맵 버퍼
 * @return: 할당된 아이노드 번호, 실패 시 -ENOSPC
 */
int alloc_inode(struct sfuse_super *sb, uint8_t *inode_map) {
  // 아이노드 0은 예약, 1부터 시작
  for (uint32_t i = 1; i < sb->inodes_count; i++) {
    uint32_t byte_idx = i / 8;
    uint32_t bit_idx = i % 8;
    if (!(inode_map[byte_idx] & (1 << bit_idx))) {
      inode_map[byte_idx] |= (1 << bit_idx);
      sb->free_inodes--;
      return (int)i;
    }
  }
  return -ENOSPC;
}

/**
 * free_inode: 아이노드를 해제
 * @sb: 슈퍼블록 구조체
 * @inode_map: 아이노드 비트맵 버퍼
 * @ino: 해제할 아이노드 번호
 */
void free_inode(struct sfuse_super *sb, uint8_t *inode_map, uint32_t ino) {
  if (ino == 0 || ino >= sb->inodes_count)
    return;
  uint32_t byte_idx = ino / 8;
  uint32_t bit_idx = ino % 8;
  inode_map[byte_idx] &= ~(1 << bit_idx);
  sb->free_inodes++;
}
