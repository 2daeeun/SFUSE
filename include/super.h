#ifndef SFUSE_SUPER_H
#define SFUSE_SUPER_H

#include <stdint.h>

// FS 설정 상수
#define SFUSE_MAGIC 0xEF53
#define SFUSE_BLOCK_SIZE 4096
#define SFUSE_SUPERBLOCK_OFFSET 1024

// VSFS 레이아웃: 총 아이노드 개수와 총 블록 개수 정의
#define SFUSE_INODES_COUNT 1024
#define SFUSE_BLOCKS_COUNT 8192

// 비트맵 및 inode table 시작 블록
#define SFUSE_INODE_BITMAP_BLOCK 2 // 블록 번호 2에 아이노드 비트맵
#define SFUSE_BLOCK_BITMAP_BLOCK 3 // 블록 번호 3에 블록 비트맵
#define SFUSE_INODE_TABLE_BLOCK 4  // 블록 번호 4부터 inode table
#define SFUSE_DATA_BLOCK_START 100 // 데이터 블록 시작

// 슈퍼블록 구조체
struct sfuse_super {
  uint32_t magic;              /* 매직 넘버 */
  uint32_t inodes_count;       /* 전체 아이노드 수 */
  uint32_t blocks_count;       /* 전체 블록 수 */
  uint32_t free_inodes;        /* 남은 아이노드 수 */
  uint32_t free_blocks;        /* 남은 데이터 블록 수 */
  uint32_t inode_bitmap_start; /* 아이노드 비트맵 시작 블록 */
  uint32_t block_bitmap_start; /* 블록 비트맵 시작 블록 */
  uint32_t inode_table_start;  /* 아이노드 테이블 시작 블록 */
  uint32_t data_block_start;   /* 데이터 블록 시작 블록 */
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 디바이스에서 슈퍼블록을 읽어 로드
 * @param fd 디바이스 파일 디스크립터
 * @param sb 슈퍼블록 구조체
 * @return 0 성공, 음수 오류코드
 */
int sb_load(int fd, struct sfuse_super *sb);

/**
 * @brief 슈퍼블록 구조체를 디스크에 기록
 * @param fd 디바이스 파일 디스크립터
 * @param sb 기록할 슈퍼블록 구조체
 * @return 0 성공, 음수 오류코드
 */
int sb_sync(int fd, const struct sfuse_super *sb);

/**
 * @brief 슈퍼블록 구조체 초기화 (포맷)
 * @param sb 초기화할 슈퍼블록 구조체
 */
void sb_format(struct sfuse_super *sb);

#ifdef __cplusplus
}
#endif

#endif // SFUSE_SUPER_H
