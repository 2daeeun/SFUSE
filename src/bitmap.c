// File: src/bitmap.c

#include "bitmap.h"
#include "block.h"
#include <errno.h>
#include <unistd.h>

/**
 * @brief 한 블록에 담길 수 있는 비트 수
 */
static const uint32_t BITS_PER_BLOCK = SFUSE_BLOCK_SIZE * 8;

/**
 * @brief 디스크에서 연속된 비트맵 블록들을 읽어 메모리에 로드
 * @param fd 파일 디스크립터
 * @param start_blk 시작 블록 번호
 * @param bmaps 비트맵 구조체 포인터
 * @param count 읽을 블록 수
 * @return 성공 시 0, 실패 시 -EIO
 */
int bitmap_load(int fd, uint32_t start_blk, struct sfuse_bitmaps *bmaps,
                uint32_t count) {
  uint8_t *buffer = (uint8_t *)bmaps;
  for (uint32_t i = 0; i < count; ++i) {
    if (read_block(fd, start_blk + i, buffer + i * SFUSE_BLOCK_SIZE) < 0) {
      return -EIO;
    }
  }
  return 0;
}

/**
 * @brief 메모리에 있는 비트맵을 디스크에 저장
 * @param fd 파일 디스크립터
 * @param start_blk 시작 블록 번호
 * @param bmaps 비트맵 구조체 포인터 (읽기 전용)
 * @param count 기록할 블록 수
 * @return 성공 시 0, 실패 시 -EIO
 */
int bitmap_sync(int fd, uint32_t start_blk, const struct sfuse_bitmaps *bmaps,
                uint32_t count) {
  const uint8_t *buffer = (const uint8_t *)bmaps;
  for (uint32_t i = 0; i < count; ++i) {
    if (write_block(fd, start_blk + i, buffer + i * SFUSE_BLOCK_SIZE) < 0) {
      return -EIO;
    }
  }
  return 0;
}

/**
 * @brief 비트맵에서 0인 비트를 찾아 1로 설정하고 인덱스를 반환
 * @param map 비트맵 버퍼
 * @param total_bits 전체 비트 수
 * @return 할당된 비트 인덱스, 실패 시 -ENOSPC
 */
int alloc_bit(uint8_t *map, uint32_t total_bits) {
  uint32_t blocks = (total_bits + BITS_PER_BLOCK - 1) / BITS_PER_BLOCK;
  for (uint32_t b = 0; b < blocks; ++b) {
    uint8_t *block_ptr = map + b * SFUSE_BLOCK_SIZE;
    for (uint32_t byte = 0; byte < SFUSE_BLOCK_SIZE; ++byte) {
      if (block_ptr[byte] == 0xFF)
        continue;
      for (uint32_t bit = 0; bit < 8; ++bit) {
        if (!(block_ptr[byte] & (1u << bit))) {
          uint32_t index = b * BITS_PER_BLOCK + byte * 8 + bit;
          if (index < total_bits) {
            block_ptr[byte] |= (uint8_t)(1u << bit);
            return index;
          }
        }
      }
    }
  }
  return -ENOSPC;
}

/**
 * @brief 비트맵에서 지정한 인덱스 비트를 0으로 설정
 * @param map 비트맵 버퍼
 * @param idx 해제할 비트 인덱스
 */
void free_bit(uint8_t *map, uint32_t idx) {
  uint32_t byte_index = idx / 8;
  uint32_t bit_offset = idx % 8;
  map[byte_index] &= (uint8_t)~(1u << bit_offset);
}

/**
 * @brief 새로운 아이노드 할당
 * @param sb 슈퍼블록 포인터
 * @param imap 아이노드 비트맵 포인터
 * @return 할당된 아이노드 번호, 실패 시 음수 오류 코드
 */
int alloc_inode(struct sfuse_superblock *sb, struct sfuse_inode_bitmap *imap) {
  int ino = alloc_bit(imap->map, sb->total_inodes);
  if (ino <= 0) {
    return -ENOSPC; // 반드시 inode 번호 1부터 시작 (0번 금지)
  }
  sb->free_inodes -= 1;
  return ino;
}

/**
 * @brief 아이노드 해제
 * @param sb 슈퍼블록 포인터
 * @param imap 아이노드 비트맵 포인터
 * @param ino 해제할 아이노드 번호
 */
void free_inode(struct sfuse_superblock *sb, struct sfuse_inode_bitmap *imap,
                uint32_t ino) {
  free_bit(imap->map, ino);
  sb->free_inodes += 1;
}

/**
 * @brief 새로운 데이터 블록 할당
 * @param sb 슈퍼블록 포인터
 * @param bmap 블록 비트맵 포인터
 * @return 할당된 블록 번호, 실패 시 음수 오류 코드
 */
int alloc_block(struct sfuse_superblock *sb, struct sfuse_block_bitmap *bmap) {
  int blk = alloc_bit(bmap->map, sb->total_blocks);
  if (blk < 0) {
    return -ENOSPC; // 음수로 명확히 실패 표시
  }
  sb->free_blocks -= 1;
  return blk;
}

/**
 * @brief 데이터 블록 해제
 * @param sb 슈퍼블록 포인터
 * @param bmap 블록 비트맵 포인터
 * @param blk 해제할 블록 번호
 */
void free_block(struct sfuse_superblock *sb, struct sfuse_block_bitmap *bmap,
                uint32_t blk) {
  free_bit(bmap->map, blk);
  sb->free_blocks += 1;
}
