// File: include/super.h

#ifndef SFUSE_SUPER_H
#define SFUSE_SUPER_H

#include <stdint.h>

/**
 * @brief SFUSE 매직 넘버
 *
 * 파일 시스템을 식별하기 위한 고유 매직 넘버입니다.
 */
#define SFUSE_MAGIC 0xEF53

/**
 * @brief 블록 크기 (바이트 단위)
 *
 * 파일 시스템에서 사용하는 블록의 크기를 바이트 단위로 정의합니다.
 */
#define SFUSE_BLOCK_SIZE 4096

/**
 * @brief 최대 아이노드 수
 *
 * 파일 시스템에서 허용하는 최대 아이노드 개수입니다.
 */
#define SFUSE_MAX_INODES 1024

/**
 * @brief 최대 데이터 블록 수
 *
 * 파일 시스템에서 허용하는 최대 데이터 블록 개수입니다.
 */
#define SFUSE_MAX_BLOCKS 65536

/**
 * @brief 슈퍼블록 구조체
 *
 * 파일 시스템 전체의 상태를 나타내는 핵심 메타데이터를 저장합니다.
 *
 * - magic               : 파일 시스템 식별자 (매직 넘버)
 * - total_inodes        : 전체 아이노드 개수
 * - total_blocks        : 전체 데이터 블록 개수
 * - free_inodes         : 사용 가능한 아이노드 개수
 * - free_blocks         : 사용 가능한 데이터 블록 개수
 * - inode_bitmap_start  : 아이노드 비트맵 시작 블록 번호
 * - block_bitmap_start  : 블록 비트맵 시작 블록 번호
 * - inode_table_start   : 아이노드 테이블 시작 블록 번호
 * - data_block_start    : 데이터 블록 시작 블록 번호
 */
struct sfuse_superblock {
    uint32_t magic;              /**< 파일 시스템 식별자 (매직 넘버) */
    uint32_t total_inodes;       /**< 전체 아이노드 수 */
    uint32_t total_blocks;       /**< 전체 데이터 블록 수 */
    uint32_t free_inodes;        /**< 사용 가능한 아이노드 수 */
    uint32_t free_blocks;        /**< 사용 가능한 데이터 블록 수 */
    uint32_t inode_bitmap_start; /**< 아이노드 비트맵 시작 블록 번호 */
    uint32_t block_bitmap_start; /**< 블록 비트맵 시작 블록 번호 */
    uint32_t inode_table_start;  /**< 아이노드 테이블 시작 블록 번호 */
    uint32_t data_block_start;   /**< 데이터 블록 시작 블록 번호 */
};

/**
 * @brief 슈퍼블록을 디스크에서 읽어 로드
 *
 * @param fd      파일 디스크립터
 * @param sb_out  읽어들인 슈퍼블록 정보를 저장할 포인터
 * @return        0이면 성공, 음수면 오류 코드
 */
int sb_load(int fd, struct sfuse_superblock *sb_out);

/**
 * @brief 슈퍼블록을 디스크에 동기화
 *
 * @param fd  파일 디스크립터
 * @param sb  동기화할 슈퍼블록 구조체 포인터
 * @return    0이면 성공, 음수면 오류 코드
 */
int sb_sync(int fd, const struct sfuse_superblock *sb);

#endif /* SFUSE_SUPER_H */
