// fs.c: 파일 시스템 초기화, 경로 해석 및 컨텍스트 관리 구현
#include "fs.h"
#include "bitmap.h"
#include "block.h"
#include "dir.h"
#include "img.h"
#include "inode.h"
#include "super.h"
#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief FUSE 콜백에서 사용될 전역 컨텍스트 반환
 */
struct sfuse_fs *get_fs_context(void) {
  return (struct sfuse_fs *)fuse_get_context()->private_data;
}

/**
 * @brief 파일 시스템 초기화
 * @param backing_fd 블록 디바이스 FD
 * @return 0 성공, 음수 오류코드
 */
int fs_initialize(int backing_fd) {
  struct sfuse_fs *fs = get_fs_context();
  fs->backing_fd = backing_fd;

  // 슈퍼블록 로드 또는 포맷
  if (sb_load(backing_fd, &fs->sb) < 0) {
    sb_format(&fs->sb);
    sb_sync(backing_fd, &fs->sb);
  }

  // 비트맵 버퍼 할당 및 로드
  size_t bmap_bytes = fs->sb.blocks_count / 8;
  size_t imap_bytes = fs->sb.inodes_count / 8;
  fs->block_map = malloc(bmap_bytes);
  fs->inode_map = malloc(imap_bytes);
  if (!fs->block_map || !fs->inode_map) {
    return -ENOMEM;
  }
  if (bitmap_load(backing_fd, fs->sb.block_bitmap_start, fs->block_map,
                  bmap_bytes) < 0)
    return -EIO;
  if (bitmap_load(backing_fd, fs->sb.inode_bitmap_start, fs->inode_map,
                  imap_bytes) < 0)
    return -EIO;

  // 루트 디렉터리(ino 1) 초기화
  uint32_t root = SFUSE_ROOT_INO;
  if (!(fs->inode_map[root / 8] & (1 << (root % 8)))) {
    // 루트 아이노드 할당 및 초기화
    fs_init_inode(&fs->sb, root, S_IFDIR | 0755, fuse_get_context()->uid,
                  fuse_get_context()->gid,
                  (struct sfuse_inode *)malloc(sizeof(struct sfuse_inode)));
    inode_sync(backing_fd, &fs->sb, root,
               (struct sfuse_inode *)malloc(sizeof(struct sfuse_inode)));
    // 비트맵 및 슈퍼블록 동기화
    bitmap_sync(backing_fd, fs->sb.block_bitmap_start, fs->block_map,
                bmap_bytes);
    bitmap_sync(backing_fd, fs->sb.inode_bitmap_start, fs->inode_map,
                imap_bytes);
    sb_sync(backing_fd, &fs->sb);
  }

  return 0;
}

/**
 * @brief 전체 경로를 부모 아이노드와 마지막 구성요소 이름으로 분할
 * @param fullpath    전체 경로 문자열
 * @param parent_ino  부모 디렉터리 아이노드 번호 출력
 * @return 마지막 구성요소 이름 (동적 할당 반환, free 필요)
 */
char *fs_split_path(const char *fullpath, uint32_t *parent_ino) {
  char *dup = strdup(fullpath);
  if (!dup)
    return NULL;
  char *slash = strrchr(dup, '/');
  if (!slash) {
    *parent_ino = SFUSE_ROOT_INO;
    return dup;
  }
  if (slash == dup) {
    // 루트 직하
    *(slash) = '\0';
    *parent_ino = SFUSE_ROOT_INO;
    return slash + 1;
  }
  *slash = '\0';
  // 부모 경로 해석
  fs_resolve_path(get_fs_context(), dup, parent_ino);
  return slash + 1;
}

/**
 * @brief 경로를 아이노드 번호로 해석
 * @param fs       파일 시스템 컨텍스트
 * @param path     경로 문자열
 * @param ino_out  결과 아이노드 번호 저장
 * @return 0 성공, 음수 오류코드
 */
int fs_resolve_path(struct sfuse_fs *fs, const char *path, uint32_t *ino_out) {
  // 루트
  uint32_t cur = SFUSE_ROOT_INO;
  if (!path || path[0] == '\0' || strcmp(path, "/") == 0) {
    *ino_out = cur;
    return 0;
  }
  char *dup = strdup(path);
  if (!dup)
    return -ENOMEM;
  char *token = strtok(dup, "/");
  while (token) {
    // 현재 디렉터리 블록 로드
    size_t buf_size = SFUSE_NDIR_BLOCKS * SFUSE_BLOCK_SIZE;
    void *buf = malloc(buf_size);
    if (!buf) {
      free(dup);
      return -ENOMEM;
    }
    if (dir_load(fs->backing_fd, &fs->sb, cur, buf) < 0) {
      free(buf);
      free(dup);
      return -ENOENT;
    }
    struct sfuse_dirent *ents = (struct sfuse_dirent *)buf;
    size_t nents = buf_size / sizeof(*ents);
    uint32_t next = 0;
    for (size_t i = 0; i < nents; i++) {
      if (ents[i].ino != 0 && strcmp(ents[i].name, token) == 0) {
        next = ents[i].ino;
        break;
      }
    }
    free(buf);
    if (next == 0) {
      free(dup);
      return -ENOENT;
    }
    cur = next;
    token = strtok(NULL, "/");
  }
  free(dup);
  *ino_out = cur;
  return 0;
}

/**
 * @brief 파일 시스템 언마운트 시 호출
 * @param private_data get_fs_context() 반환값
 */
void fs_destroy(void *private_data) {
  struct sfuse_fs *fs = private_data;
  size_t bmap_bytes = fs->sb.blocks_count / 8;
  size_t imap_bytes = fs->sb.inodes_count / 8;
  bitmap_sync(fs->backing_fd, fs->sb.block_bitmap_start, fs->block_map,
              bmap_bytes);
  bitmap_sync(fs->backing_fd, fs->sb.inode_bitmap_start, fs->inode_map,
              imap_bytes);
  sb_sync(fs->backing_fd, &fs->sb);
  free(fs->block_map);
  free(fs->inode_map);
}
