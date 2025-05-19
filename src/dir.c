// File: src/dir.c

#include "dir.h"
#include "block.h"
#include "inode.h"
#include <errno.h>
#include <string.h>
#include <sys/types.h>

/**
 * @brief 디렉터리 내에서 지정한 이름의 엔트리를 검색하여 inode 번호 반환
 * @param fs SFUSE 파일 시스템 컨텍스트
 * @param dir_ino 검색 대상 디렉터리의 inode 번호
 * @param name 찾을 파일/디렉터리 이름 (null-terminated)
 * @param out_ino 검색된 inode 번호를 저장할 포인터
 * @return 성공 시 0, 실패 시 -ENOENT 또는 -EIO
 */
int dir_lookup(const struct sfuse_fs *fs, uint32_t dir_ino, const char *name,
               uint32_t *out_ino) {
  struct sfuse_inode dir_inode;
  if (inode_load(fs->backing_fd, &fs->sb, dir_ino, &dir_inode) < 0) {
    return -ENOENT;
  }

  /* 모든 direct 블록 순회 */
  for (uint32_t i = 0; i < 12; i++) {
    uint32_t blk = dir_inode.direct[i];
    if (blk == 0)
      continue; /**< 할당된 블록이 없으면 건너뜀 */

    uint8_t block[SFUSE_BLOCK_SIZE]; /**< 블록 데이터를 읽어올 버퍼 */
    if (read_block(fs->backing_fd, blk, block) < 0) {
      return -EIO;
    }

    struct sfuse_dirent *entries = (struct sfuse_dirent *)block;
    for (uint32_t j = 0; j < DENTS_PER_BLOCK; ++j) {
      if (entries[j].inode == 0)
        continue; /**< 빈 엔트리 건너뜀 */
      if (strncmp(entries[j].name, name, SFUSE_NAME_MAX) == 0) {
        *out_ino = entries[j].inode;
        return 0;
      }
    }
  }
  return -ENOENT;
}

/**
 * @brief 디렉터리 엔트리 목록을 FUSE에 전달하여 나열
 * @param fs SFUSE 파일 시스템 컨텍스트
 * @param dir_ino 목록을 읽을 디렉터리의 inode 번호
 * @param buf FUSE가 제공하는 버퍼 포인터
 * @param filler FUSE의 디렉터리 엔트리 추가 콜백
 * @param offset 읽기 시작 오프셋 (사용되지 않음)
 * @return 성공 시 0, 실패 시 -ENOENT 또는 -EIO
 */
int dir_list(const struct sfuse_fs *fs, uint32_t dir_ino, void *buf,
             fuse_fill_dir_t filler, off_t offset) {
  struct sfuse_inode dir_inode;
  if (inode_load(fs->backing_fd, &fs->sb, dir_ino, &dir_inode) < 0) {
    return -ENOENT;
  }

  /* "." 및 ".." 기본 엔트리 추가 */
  filler(buf, ".", NULL, 0, 0);
  filler(buf, "..", NULL, 0, 0);

  /* 저장된 디렉터리 엔트리 나열 */
  for (uint32_t i = 0; i < 12; i++) {
    uint32_t blk = dir_inode.direct[i];
    if (blk == 0)
      break; /**< 더 이상 블록이 없으면 종료 */

    uint8_t block[SFUSE_BLOCK_SIZE]; /**< 블록 데이터를 읽어올 버퍼 */
    if (read_block(fs->backing_fd, blk, block) < 0) {
      return -EIO;
    }

    struct sfuse_dirent *entries = (struct sfuse_dirent *)block;
    for (uint32_t j = 0; j < DENTS_PER_BLOCK; ++j) {
      if (entries[j].inode == 0)
        continue; /**< 빈 엔트리 건너뜀 */
      if (entries[j].name[0] == '\0')
        continue; /**< 이름이 비어있으면 건너뜀 */
      filler(buf, entries[j].name, NULL, 0, 0);
    }
  }
  return 0;
}
