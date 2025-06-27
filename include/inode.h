// File: include/inode.h

#ifndef SFUSE_INODE_H
#define SFUSE_INODE_H

#include "super.h"
#include <stdint.h>

/**
 * @brief 한 블록에 담을 수 있는 아이노드 수
 */
#define SFUSE_INODES_PER_BLOCK (SFUSE_BLOCK_SIZE / sizeof(struct sfuse_inode))

/**
 * @brief 한 블록에 담을 수 있는 포인터 수 (32-bit)
 */
#define SFUSE_PTRS_PER_BLOCK (SFUSE_BLOCK_SIZE / sizeof(uint32_t))

/**
 * @brief 디스크에 저장되는 아이노드 구조체
 */
struct sfuse_inode {
  uint32_t mode;            /**< 파일 타입 및 권한 */
  uint32_t uid;             /**< 소유자 사용자 ID */
  uint32_t gid;             /**< 소유자 그룹 ID */
  uint32_t size;            /**< 파일 크기 (바이트 단위) */
  uint32_t atime;           /**< 마지막 접근 시간 (epoch) */
  uint32_t mtime;           /**< 마지막 수정 시간 (epoch) */
  uint32_t ctime;           /**< 메타데이터 변경 시간 (epoch) */
  uint32_t direct[12];      /**< 직접 데이터 블록 포인터 배열 */
  uint32_t indirect;        /**< 단일 간접 블록 포인터 */
  uint32_t double_indirect; /**< 이중 간접 블록 포인터 */
};

/**
 * @brief 아이노드 블록: 여러 아이노드를 담는 블록
 */
struct sfuse_inode_block {
  struct sfuse_inode inodes[SFUSE_INODES_PER_BLOCK]; /**< 아이노드 배열 */
};

/**
 * @brief 디스크 이미지에서 아이노드를 읽어 구조체에 로드
 * @param fd 파일 디스크립터
 * @param sb 슈퍼블록 포인터
 * @param ino 읽을 아이노드 번호
 * @param out 출력할 아이노드 구조체 포인터
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int inode_load(int fd, const struct sfuse_superblock *sb, uint32_t ino,
               struct sfuse_inode *out);

/**
 * @brief 아이노드 구조체 내용을 디스크 이미지에 동기화
 * @param fd 파일 디스크립터
 * @param sb 슈퍼블록 포인터
 * @param ino 동기화할 아이노드 번호
 * @param in 입력할 아이노드 구조체 포인터
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int inode_sync(int fd, const struct sfuse_superblock *sb, uint32_t ino,
               const struct sfuse_inode *in);

#endif /* SFUSE_INODE_H */
