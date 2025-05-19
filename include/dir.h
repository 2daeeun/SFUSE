// File: include/dir.h

#ifndef SFUSE_DIR_H
#define SFUSE_DIR_H

#include "fs.h"
#include <fuse3/fuse.h>
#include <stdint.h>

#define SFUSE_NAME_MAX 256 /**< 파일/디렉터리 이름의 최대 길이 */
/**
 * @brief 한 블록당 디렉터리 엔트리 수
 */
#define DENTS_PER_BLOCK (SFUSE_BLOCK_SIZE / sizeof(struct sfuse_dirent))

/**
 * @brief 디스크에 저장되는 디렉터리 엔트리 구조체
 */
struct sfuse_dirent {
  uint32_t inode;            /**< 파일/디렉터리의 inode 번호 */
  char name[SFUSE_NAME_MAX]; /**< 파일/디렉터리 이름 (null-terminated) */
};

/**
 * @brief 디렉터리에서 이름으로 inode 번호를 검색
 * @param fs SFUSE 파일시스템 컨텍스트
 * @param dir_ino 디렉터리의 inode 번호
 * @param name 찾을 파일/디렉터리 이름 (null-terminated)
 * @param out_ino 검색된 inode 번호 저장 위치
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int dir_lookup(const struct sfuse_fs *fs, uint32_t dir_ino, const char *name,
               uint32_t *out_ino);

/**
 * @brief FUSE용 디렉터리 목록 채우기 콜백 호출
 * @param fs SFUSE 파일시스템 컨텍스트
 * @param dir_ino 디렉터리의 inode 번호
 * @param buf FUSE가 전달하는 버퍼 포인터
 * @param filler FUSE의 디렉터리 엔트리 추가 함수
 * @param offset 디렉터리 읽기 오프셋
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int dir_list(const struct sfuse_fs *fs, uint32_t dir_ino, void *buf,
             fuse_fill_dir_t filler, off_t offset);

#endif /* SFUSE_DIR_H */
