// File: include/inode.h
#ifndef SFUSE_INODE_H
#define SFUSE_INODE_H

#include "super.h"
#include <stdint.h>

/* 한 블록에 들어갈 수 있는 inode 수 */
#define SFUSE_INODES_PER_BLOCK (SFUSE_BLOCK_SIZE / sizeof(struct sfuse_inode))

/* 한 블록에 들어갈 수 있는 포인터 수 (간접 블록용) */
#define SFUSE_PTRS_PER_BLOCK (SFUSE_BLOCK_SIZE / sizeof(uint32_t)) // 추가됨

/* on-disk inode 구조체 */
struct sfuse_inode {
  uint32_t mode;            /* 파일 모드 (파일 타입 및 권한) */
  uint32_t uid;             /* 소유자 사용자 ID */
  uint32_t gid;             /* 소유자 그룹 ID */
  uint32_t size;            /* 파일 크기 (바이트 단위) */
  uint32_t atime;           /* 마지막 접근 시간 */
  uint32_t mtime;           /* 마지막 수정 시간 */
  uint32_t ctime;           /* inode 변경 시간 */
  uint32_t direct[12];      /* 직접 블록 포인터 */
  uint32_t indirect;        /* 단일 간접 블록 포인터 */
  uint32_t double_indirect; /* 이중 간접 블록 포인터 */
};

/* inode 블록: 한 블록 안에 여러 개의 inode가 들어감 */
struct sfuse_inode_block {
  struct sfuse_inode inodes[SFUSE_INODES_PER_BLOCK];
};

/* 지정한 inode 번호의 inode를 디스크에서 읽어옴 */
int inode_load(int fd, const struct sfuse_superblock *sb, uint32_t ino,
               struct sfuse_inode *out);

/* 지정한 inode 번호의 inode를 디스크에 저장함 */
int inode_sync(int fd, const struct sfuse_superblock *sb, uint32_t ino,
               const struct sfuse_inode *in);

#endif /* SFUSE_INODE_H */
