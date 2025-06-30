// dir.c: 디렉터리 엔트리 관리 구현
#include "dir.h"
#include "bitmap.h"
#include "block.h"
#include "img.h"
#include "inode.h"
#include "super.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * dir_load: 디렉터리 데이터 블록들을 모두 읽어서 버퍼에 로드
 */
int dir_load(int fd, const struct sfuse_super *sb, uint32_t ino, void *buf) {
  struct sfuse_inode inode;
  int res = inode_load(fd, sb, ino, &inode);
  if (res < 0)
    return res;
  for (uint32_t i = 0; i < SFUSE_NDIR_BLOCKS; i++) {
    if (inode.direct[i] == 0)
      continue;
    res = read_block(fd, inode.direct[i], (char *)buf + i * SFUSE_BLOCK_SIZE);
    if (res < 0)
      return res;
  }
  return 0;
}

/**
 * dir_add_entry: 디렉터리에 새 엔트리 추가
 */
int dir_add_entry(int fd, const struct sfuse_super *sb, uint32_t ino,
                  const char *name, uint32_t child_ino, uint8_t *block_map,
                  uint8_t *inode_map, struct sfuse_super *supermap) {
  // 디렉터리 아이노드 로드
  struct sfuse_inode inode;
  int res = inode_load(fd, sb, ino, &inode);
  if (res < 0)
    return res;

  // 디렉터리 블록 전체 로드
  size_t buf_size = SFUSE_NDIR_BLOCKS * SFUSE_BLOCK_SIZE;
  void *buf = malloc(buf_size);
  if (!buf)
    return -ENOMEM;
  res = dir_load(fd, sb, ino, buf);
  if (res < 0) {
    free(buf);
    return res;
  }

  struct sfuse_dirent *ents = (struct sfuse_dirent *)buf;
  uint32_t max_entries = buf_size / sizeof(*ents);

  // 빈 슬롯 찾기
  for (uint32_t i = 0; i < max_entries; i++) {
    if (ents[i].ino == 0) {
      ents[i].ino = child_ino;
      strncpy(ents[i].name, name, sizeof(ents[i].name) - 1);
      ents[i].name[sizeof(ents[i].name) - 1] = '\0';

      // 모든 블록에 다시 기록
      for (uint32_t b = 0; b < SFUSE_NDIR_BLOCKS; b++) {
        if (inode.direct[b] == 0)
          continue;
        res = write_block(fd, inode.direct[b],
                          (char *)buf + b * SFUSE_BLOCK_SIZE);
        if (res < 0) {
          free(buf);
          return res;
        }
      }

      // 슈퍼블록 및 비트맵 동기화
      sb_sync(fd, supermap);
      bitmap_sync(fd, sb->block_bitmap_start, block_map, sb->blocks_count / 8);
      bitmap_sync(fd, sb->inode_bitmap_start, inode_map, sb->inodes_count / 8);
      free(buf);
      return 0;
    }
  }
  free(buf);
  return -ENOSPC;
}

/**
 * dir_remove_entry: 디렉터리에서 엔트리 삭제
 */
int dir_remove_entry(int fd, const struct sfuse_super *sb, uint32_t ino,
                     const char *name) {
  struct sfuse_inode inode;
  int res = inode_load(fd, sb, ino, &inode);
  if (res < 0)
    return res;

  size_t buf_size = SFUSE_NDIR_BLOCKS * SFUSE_BLOCK_SIZE;
  void *buf = malloc(buf_size);
  if (!buf)
    return -ENOMEM;
  res = dir_load(fd, sb, ino, buf);
  if (res < 0) {
    free(buf);
    return res;
  }

  struct sfuse_dirent *ents = (struct sfuse_dirent *)buf;
  uint32_t max_entries = buf_size / sizeof(*ents);

  for (uint32_t i = 0; i < max_entries; i++) {
    if (ents[i].ino != 0 && strcmp(ents[i].name, name) == 0) {
      ents[i].ino = 0;
      ents[i].name[0] = '\0';

      // 해당 블록들에 기록
      for (uint32_t b = 0; b < SFUSE_NDIR_BLOCKS; b++) {
        if (inode.direct[b] == 0)
          continue;
        res = write_block(fd, inode.direct[b],
                          (char *)buf + b * SFUSE_BLOCK_SIZE);
        if (res < 0) {
          free(buf);
          return res;
        }
      }
      free(buf);
      return 0;
    }
  }
  free(buf);
  return -ENOENT;
}
