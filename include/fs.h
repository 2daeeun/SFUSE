#ifndef SFUSE_FS_H
#define SFUSE_FS_H

#include "super.h"
#include <stdint.h>

// 루트 아이노드 번호 (일반적으로 1)
#define SFUSE_ROOT_INO 1

/** 파일 시스템 전역 컨텍스트 */
struct sfuse_fs {
  int backing_fd;        /* 블록 디바이스 FD */
  struct sfuse_super sb; /* 슈퍼블록 */
  uint8_t *block_map;    /* 블록 비트맵 버퍼 */
  uint8_t *inode_map;    /* 아이노드 비트맵 버퍼 */
};

/**
 * @brief 파일 시스템 초기화
 * @param backing_fd 블록 디바이스 FD
 * @return 0 성공, 음수 오류코드
 */
int fs_initialize(int backing_fd);

/**
 * @brief 파일 시스템 종료 (언마운트)
 * @param private_data get_fs_context()가 반환한 포인터
 */
void fs_destroy(void *private_data);

/**
 * @brief 경로를 아이노드 번호로 해석
 * @param fs       파일 시스템 컨텍스트
 * @param path     경로 문자열
 * @param ino_out  결과 아이노드 번호 저장
 * @return 0 성공, 음수 오류코드
 */
int fs_resolve_path(struct sfuse_fs *fs, const char *path, uint32_t *ino_out);

/**
 * @brief 전체 경로를 부모 아이노드와 이름으로 분할
 * @param fullpath    전체 경로 ("/foo/bar")
 * @param parent_ino  부모 디렉터리 아이노드 번호 출력
 * @return 마지막 구성요소 이름 (동적 할당 반환, free 필요)
 */
char *fs_split_path(const char *fullpath, uint32_t *parent_ino);

/**
 * @brief FUSE 콜백에서 전역 컨텍스트 참조
 * @return struct sfuse_fs* (fuse_get_context()->private_data)
 */
struct sfuse_fs *get_fs_context(void);

#endif // SFUSE_FS_H
