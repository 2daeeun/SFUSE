// File: include/dir.h
#ifndef SFUSE_DIR_H
#define SFUSE_DIR_H

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 31
#endif
#include "fs.h"
#include <fuse.h>
#include <stdint.h>

#define SFUSE_NAME_MAX 256 /* 파일 또는 디렉터리 이름의 최대 길이 */
#define DENTS_PER_BLOCK                                                        \
  (SFUSE_BLOCK_SIZE /                                                          \
   sizeof(                                                                     \
       struct sfuse_dirent)) /* 한 블록에 들어갈 수 있는 디렉터리 엔트리 수 */

/* 디렉터리 엔트리 구조체 */
struct sfuse_dirent {
  uint32_t inode;            /* 해당 이름이 가리키는 inode 번호 */
  char name[SFUSE_NAME_MAX]; /* 파일 또는 디렉터리 이름 */
};

/* 디렉터리 내에서 주어진 이름을 검색하고 inode 번호를 반환 */
int dir_lookup(const struct sfuse_fs *fs, uint32_t dir_ino, const char *name,
               uint32_t *out_ino);

/* 디렉터리의 엔트리 목록을 읽어서 FUSE에 전달 */
int dir_list(const struct sfuse_fs *fs, uint32_t dir_ino, void *buf,
             fuse_fill_dir_t filler, off_t offset);

#endif /* SFUSE_DIR_H */
