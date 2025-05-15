// File: include/super.h
#ifndef SFUSE_SUPER_H
#define SFUSE_SUPER_H

#include <stdint.h>

#define SFUSE_MAGIC 0xEF53     /* SFUSE 파일 시스템을 식별하는 매직 넘버 */
#define SFUSE_BLOCK_SIZE 4096  /* 블록 크기 (바이트 단위) */
#define SFUSE_MAX_INODES 1024  /* 최대 아이노드 수 */
#define SFUSE_MAX_BLOCKS 65536 /* 최대 데이터 블록 수 */

/* 슈퍼블록 구조체 */
struct sfuse_superblock {
  uint32_t magic;              /* 파일 시스템 매직 넘버 */
  uint32_t total_inodes;       /* 전체 아이노드 수 */
  uint32_t total_blocks;       /* 전체 데이터 블록 수 */
  uint32_t free_inodes;        /* 사용 가능한 아이노드 수 */
  uint32_t free_blocks;        /* 사용 가능한 데이터 블록 수 */
  uint32_t inode_bitmap_start; /* 아이노드 비트맵의 시작 블록 인덱스 */
  uint32_t block_bitmap_start; /* 블록 비트맵의 시작 블록 인덱스 */
  uint32_t inode_table_start;  /* 아이노드 테이블의 시작 블록 인덱스 */
  uint32_t data_block_start;   /* 데이터 블록의 시작 블록 인덱스 */
};

/* 슈퍼블록 관련 함수 */
int sb_load(
    int fd,
    struct sfuse_superblock *sb_out); /* 슈퍼블록을 디스크에서 읽어오기 */
int sb_sync(
    int fd,
    const struct sfuse_superblock *sb); /* 슈퍼블록을 디스크에 동기화하기 */

#endif /* SFUSE_SUPER_H */
