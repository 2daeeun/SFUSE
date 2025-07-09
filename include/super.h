/**
 * @file include/super.h
 * @brief SFUSE 파일 시스템의 슈퍼블록 관련 구조 및 함수 정의
 *
 * 이 헤더 파일은 SFUSE 파일 시스템에서 슈퍼블록을 관리하기 위한 구조체 및 함수
 * 원형을 정의한다.
 */

#ifndef SFUSE_SUPER_H
#define SFUSE_SUPER_H

#include <stdint.h>

/** @brief 슈퍼블록의 매직 넘버 */
#define SFUSE_MAGIC 0xEF53

/** @brief 블록 크기 (바이트) */
#define SFUSE_BLOCK_SIZE 4096

/** @brief 슈퍼블록이 저장되는 디스크 내의 오프셋 (바이트 단위) */
#define SFUSE_SUPERBLOCK_OFFSET 1024

/** @brief 파일 시스템의 총 아이노드 개수 */
#define SFUSE_INODES_COUNT 1024

/** @brief 파일 시스템의 총 블록 개수 */
#define SFUSE_BLOCKS_COUNT 8192

/** @brief 아이노드 비트맵이 시작되는 블록 번호 */
#define SFUSE_INODE_BITMAP_BLOCK 2

/** @brief 블록 비트맵이 시작되는 블록 번호 */
#define SFUSE_BLOCK_BITMAP_BLOCK 3

/** @brief 아이노드 테이블이 시작되는 블록 번호 */
#define SFUSE_INODE_TABLE_BLOCK 4

/** @brief 실제 데이터 블록이 시작되는 블록 번호 */
#define SFUSE_DATA_BLOCK_START 100

/**
 * @struct sfuse_super
 * @brief 파일 시스템 메타데이터를 저장하는 슈퍼블록 구조체
 */
struct sfuse_super {
  uint32_t magic;              /**< 슈퍼블록 유효성 확인을 위한 매직 넘버 */
  uint32_t inodes_count;       /**< 파일 시스템에 존재하는 전체 아이노드 수 */
  uint32_t blocks_count;       /**< 파일 시스템의 전체 블록 수 */
  uint32_t free_inodes;        /**< 사용할 수 있는 아이노드의 수 */
  uint32_t free_blocks;        /**< 사용할 수 있는 데이터 블록의 수 */
  uint32_t inode_bitmap_start; /**< 아이노드 비트맵이 시작되는 블록 번호 */
  uint32_t block_bitmap_start; /**< 블록 비트맵이 시작되는 블록 번호 */
  uint32_t inode_table_start;  /**< 아이노드 테이블이 시작되는 블록 번호 */
  uint32_t data_block_start;   /**< 실제 데이터 블록이 시작되는 블록 번호 */
};

/**
 * @brief 디스크에서 슈퍼블록을 읽어오는 함수
 *
 * 이 함수는 디스크의 특정 위치에서 슈퍼블록을 읽어 유효성을 검사한다.
 *
 * @param fd 디바이스 파일 디스크립터
 * @param sb 읽어온 슈퍼블록을 저장할 구조체의 포인터
 * @return 성공 시 0, 실패 시 음수 오류 코드 반환
 */
int sb_load(int fd, struct sfuse_super *sb);

/**
 * @brief 슈퍼블록을 디스크에 동기화(쓰기)하는 함수
 *
 *  * 이 함수는 메모리에 있는 슈퍼블록의 내용을 디스크에 기록한다.
 *
 * @param fd 디바이스 파일 디스크립터
 * @param sb 디스크에 기록할 슈퍼블록 구조체의 포인터
 * @return 성공 시 0, 실패 시 음수 오류 코드 반환
 */
int sb_sync(int fd, const struct sfuse_super *sb);

/**
 * @brief 슈퍼블록 구조체를 기본 값으로 초기화하는 함수
 *
 * 이 함수는 새로운 파일 시스템을 만들 때 사용된다.
 *
 * @param sb 초기화할 슈퍼블록 구조체의 포인터
 */
void sb_format(struct sfuse_super *sb);

#endif // SFUSE_SUPER_H
