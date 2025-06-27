// File: include/bitmap.h

#ifndef SFUSE_BITMAP_H
#define SFUSE_BITMAP_H

#include "super.h"
#include <stdint.h>

/**
 * @brief SFUSE용 비트맵 구조체 (아이노드/블록 할당 상태 추적)
 */

/**
 * @brief 아이노드 할당 비트맵 (1블록)
 */
struct sfuse_inode_bitmap {
  uint8_t map[SFUSE_BLOCK_SIZE];
};

/**
 * @brief 데이터 블록 할당 비트맵 (2블록)
 */
struct sfuse_block_bitmap {
  uint8_t map[SFUSE_BLOCK_SIZE * 2];
};

/**
 * @brief 아이노드 비트맵과 블록 비트맵을 함께 담는 구조체
 */
struct sfuse_bitmaps {
  struct sfuse_inode_bitmap inode; /**< 아이노드 비트맵 */
  struct sfuse_block_bitmap block; /**< 데이터 블록 비트맵 */
};

/**
 * @brief 비트맵을 디스크에서 읽어 메모리에 로드
 * @param fd 파일 디스크립터
 * @param start_blk 시작 블록 번호
 * @param bmaps 비트맵 구조체 포인터
 * @param count 읽을 블록 수
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int bitmap_load(int fd, uint32_t start_blk, struct sfuse_bitmaps *bmaps,
                uint32_t count);

/**
 * @brief 메모리 비트맵을 디스크에 동기화
 * @param fd 파일 디스크립터
 * @param start_blk 시작 블록 번호
 * @param bmaps 비트맵 구조체 포인터 (읽기 전용)
 * @param count 쓸 블록 수
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int bitmap_sync(int fd, uint32_t start_blk, const struct sfuse_bitmaps *bmaps,
                uint32_t count);

/**
 * @brief 비트맵에서 비트를 할당
 * @param map 비트맵 버퍼
 * @param total_bits 전체 비트 수
 * @return 할당된 비트 인덱스 또는 음수 오류 코드
 */
int alloc_bit(uint8_t *map, uint32_t total_bits);

/**
 * @brief 비트맵에서 비트를 해제
 * @param map 비트맵 버퍼
 * @param idx 해제할 비트 인덱스
 */
void free_bit(uint8_t *map, uint32_t idx);

/**
 * @brief 슈퍼블록 기반으로 아이노드 할당
 * @param sb 슈퍼블록 포인터
 * @param imap 아이노드 비트맵 포인터
 * @return 할당된 아이노드 번호 또는 음수 오류 코드
 */
int alloc_inode(struct sfuse_superblock *sb, struct sfuse_inode_bitmap *imap);

/**
 * @brief 슈퍼블록 기반으로 아이노드 해제
 * @param sb 슈퍼블록 포인터
 * @param imap 아이노드 비트맵 포인터
 * @param ino 해제할 아이노드 번호
 */
void free_inode(struct sfuse_superblock *sb, struct sfuse_inode_bitmap *imap,
                uint32_t ino);

/**
 * @brief 슈퍼블록 기반으로 데이터 블록 할당
 * @param sb 슈퍼블록 포인터
 * @param bmap 블록 비트맵 포인터
 * @return 할당된 블록 번호 또는 음수 오류 코드
 */
int alloc_block(struct sfuse_superblock *sb, struct sfuse_block_bitmap *bmap);

/**
 * @brief 슈퍼블록 기반으로 데이터 블록 해제
 * @param sb 슈퍼블록 포인터
 * @param bmap 블록 비트맵 포인터
 * @param blk 해제할 블록 번호
 */
void free_block(struct sfuse_superblock *sb, struct sfuse_block_bitmap *bmap,
                uint32_t blk);

#endif /* SFUSE_BITMAP_H */
