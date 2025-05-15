#ifndef SFUSE_BITMAP_H
#define SFUSE_BITMAP_H

// #include "inode.h"
#include "super.h"
#include <stdint.h>

/* 아이노드 비트맵 (아이노드 할당 상태 저장) */
struct sfuse_inode_bitmap {
  uint8_t map[SFUSE_BLOCK_SIZE]; /* 아이노드 할당 비트맵 (1블록 크기) */
};

/* 데이터 블록 비트맵 (데이터 블록 할당 상태 저장) */
struct sfuse_block_bitmap {
  uint8_t map[SFUSE_BLOCK_SIZE *
              2]; /* 데이터 블록 할당 비트맵 (2블록에 걸쳐 있음) */
};

/* 비트맵 통합 구조체 */
struct sfuse_bitmaps {
  struct sfuse_inode_bitmap inode; /* 아이노드 비트맵 */
  struct sfuse_block_bitmap block; /* 데이터 블록 비트맵 */
};

/* 비트맵 로드 및 동기화 함수 */
int bitmap_load(int fd, uint32_t start_blk, struct sfuse_bitmaps *bmaps,
                uint32_t count); /* 디스크에서 비트맵을 읽어옴 */
int bitmap_sync(int fd, uint32_t start_blk, const struct sfuse_bitmaps *bmaps,
                uint32_t count); /* 메모리 상의 비트맵을 디스크에 저장 */

/* 비트 할당/해제 유틸리티 */
int alloc_bit(uint8_t *map,
              uint32_t total_bits);        /* 비어 있는 비트를 찾아 할당 */
void free_bit(uint8_t *map, uint32_t idx); /* 지정한 비트를 해제 */

int alloc_inode(struct sfuse_superblock *sb,
                struct sfuse_inode_bitmap *imap); /* 아이노드 할당 */
void free_inode(struct sfuse_superblock *sb, struct sfuse_inode_bitmap *imap,
                uint32_t ino); /* 아이노드 해제 */

int alloc_block(struct sfuse_superblock *sb,
                struct sfuse_block_bitmap *bmap); /* 데이터 블록 할당 */
void free_block(struct sfuse_superblock *sb, struct sfuse_block_bitmap *bmap,
                uint32_t blk); /* 데이터 블록 해제 */

#endif /* SFUSE_BITMAP_H */
