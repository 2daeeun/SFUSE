#include "fs.h"
#include "bitmap.h"
#include "block.h"
#include "dir.h"
#include "inode.h"
#include "super.h"
#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct sfuse_fs *get_fs_context(void) {
  return (struct sfuse_fs *)fuse_get_context()->private_data;
}

int fs_initialize(int backing_fd) {
  struct sfuse_fs *fs = get_fs_context();
  fs->backing_fd = backing_fd;

  // 디바이스 크기와 총 블록 수 계산
  off_t device_size = lseek(backing_fd, 0, SEEK_END);
  uint32_t total_blocks = device_size / SFUSE_BLOCK_SIZE;

  // 슈퍼블록 로드 시도
  if (sb_load(backing_fd, &fs->sb) == 0 && fs->sb.magic == SFUSE_MAGIC) {
    // 기존 FS 로드 성공
    // 메모리상 비트맵 할당 및 디스크에서 불러오기
    size_t bmap_bytes = fs->sb.blocks_count / 8;
    size_t imap_bytes = fs->sb.inodes_count / 8;
    fs->block_map = calloc(1, bmap_bytes);
    fs->inode_map = calloc(1, imap_bytes);
    if (!fs->block_map || !fs->inode_map)
      return -ENOMEM;
    bitmap_load(backing_fd, fs->sb.block_bitmap_start, fs->block_map,
                bmap_bytes);
    bitmap_load(backing_fd, fs->sb.inode_bitmap_start, fs->inode_map,
                imap_bytes);

    // 루트 아이노드 존재 확인
    uint32_t root = SFUSE_ROOT_INO; // 보통 1번
    if (!(fs->inode_map[root / 8] & (1 << (root % 8)))) {
      // 루트 비트맵 비어있으면 루트 아이노드 생성
      fs->inode_map[root / 8] |= (1 << (root % 8));
      fs->sb.free_inodes--;
      // 아이노드 구조체 초기화
      struct sfuse_inode root_inode;
      fs_init_inode(&fs->sb, root, S_IFDIR | 0755, fuse_get_context()->uid,
                    fuse_get_context()->gid, &root_inode);
      // 데이터 블록 할당
      int blk_index = alloc_block(&fs->sb, fs->block_map);
      if (blk_index < 0) {
        // 공간 부족 - 루트 생성 실패
        return -ENOSPC;
      }
      uint32_t phys_block = fs->sb.data_block_start + blk_index;
      root_inode.direct[0] = phys_block;
      root_inode.size = SFUSE_BLOCK_SIZE;
      // 루트 디렉터리 블록 초기화 ("." 와 "..")
      uint8_t block[SFUSE_BLOCK_SIZE] = {0};
      struct sfuse_dirent *entries = (struct sfuse_dirent *)block;
      entries[0].ino = root;
      strcpy(entries[0].name, ".");
      entries[1].ino = root;
      strcpy(entries[1].name, "..");
      if (write_block(backing_fd, phys_block, block) < 0)
        return -EIO;
      if (inode_sync(backing_fd, &fs->sb, root, &root_inode) < 0)
        return -EIO;
      // 변경된 비트맵과 슈퍼블록 동기화
      bitmap_sync(backing_fd, fs->sb.block_bitmap_start, fs->block_map,
                  bmap_bytes);
      bitmap_sync(backing_fd, fs->sb.inode_bitmap_start, fs->inode_map,
                  imap_bytes);
      sb_sync(backing_fd, &fs->sb);
    }
  } else {
    // 슈퍼블록이 없거나 매직넘버 불일치 -> 새로운 FS 포맷
    sb_format(&fs->sb); // 매직넘버, 기본 값 초기화
    fs->sb.blocks_count = total_blocks;
    // 총 아이노드 수는 이미 SFUSE_INODES_COUNT(1024)로 설정됨
    // 비트맵 및 아이노드 테이블 크기에 따라 동적으로 레이아웃 조정
    uint32_t block_bitmap_blocks =
        (total_blocks + 8 * SFUSE_BLOCK_SIZE - 1) / (8 * SFUSE_BLOCK_SIZE);
    uint32_t inode_bitmap_blocks =
        (fs->sb.inodes_count + 8 * SFUSE_BLOCK_SIZE - 1) /
        (8 * SFUSE_BLOCK_SIZE);
    fs->sb.block_bitmap_start = 2;
    fs->sb.inode_bitmap_start = fs->sb.block_bitmap_start + block_bitmap_blocks;
    fs->sb.inode_table_start = fs->sb.inode_bitmap_start + inode_bitmap_blocks;
    // 아이노드 테이블 블록 수 계산
    uint32_t inode_table_blocks =
        (fs->sb.inodes_count * sizeof(struct sfuse_inode) + SFUSE_BLOCK_SIZE -
         1) /
        SFUSE_BLOCK_SIZE;
    fs->sb.data_block_start = fs->sb.inode_table_start + inode_table_blocks;
    // 남은 공간 업데이트
    fs->sb.free_blocks = fs->sb.blocks_count - fs->sb.data_block_start;
    fs->sb.free_inodes = fs->sb.inodes_count - 1; // 루트 제외

    // 슈퍼블록 기록
    if (sb_sync(backing_fd, &fs->sb) < 0)
      return -EIO;
    // 비트맵 메모리 할당 및 0으로 초기화 후 디스크에 기록
    size_t bmap_bytes = fs->sb.blocks_count / 8;
    size_t imap_bytes = fs->sb.inodes_count / 8;
    fs->block_map = calloc(1, bmap_bytes);
    fs->inode_map = calloc(1, imap_bytes);
    if (!fs->block_map || !fs->inode_map)
      return -ENOMEM;
    bitmap_sync(backing_fd, fs->sb.block_bitmap_start, fs->block_map,
                bmap_bytes);
    bitmap_sync(backing_fd, fs->sb.inode_bitmap_start, fs->inode_map,
                imap_bytes);

    // 루트 아이노드 생성 (신규 FS)
    uint32_t root = SFUSE_ROOT_INO;
    fs->inode_map[root / 8] |= (1 << (root % 8));
    fs->sb.free_inodes--;
    struct sfuse_inode root_inode;
    fs_init_inode(&fs->sb, root, S_IFDIR | 0755, fuse_get_context()->uid,
                  fuse_get_context()->gid, &root_inode);
    int blk_index = alloc_block(&fs->sb, fs->block_map);
    if (blk_index < 0)
      return -ENOSPC;
    uint32_t phys_block = fs->sb.data_block_start + blk_index;
    root_inode.direct[0] = phys_block;
    root_inode.size = SFUSE_BLOCK_SIZE;
    // "." 및 ".." 엔트리 포함한 디렉터리 블록 작성
    uint8_t block[SFUSE_BLOCK_SIZE] = {0};
    struct sfuse_dirent *entries = (struct sfuse_dirent *)block;
    entries[0].ino = root;
    strcpy(entries[0].name, ".");
    entries[1].ino = root;
    strcpy(entries[1].name, "..");
    if (write_block(backing_fd, phys_block, block) < 0)
      return -EIO;
    if (inode_sync(backing_fd, &fs->sb, root, &root_inode) < 0)
      return -EIO;
    // 슈퍼블록/비트맵 최종 동기화
    bitmap_sync(backing_fd, fs->sb.block_bitmap_start, fs->block_map,
                bmap_bytes);
    bitmap_sync(backing_fd, fs->sb.inode_bitmap_start, fs->inode_map,
                imap_bytes);
    sb_sync(backing_fd, &fs->sb);
  }

  return 0;
}

char *fs_split_path(const char *fullpath, uint32_t *parent_ino) {
  char *dup = strdup(fullpath);
  if (!dup)
    return NULL;

  char *slash = strrchr(dup, '/');
  char *name;

  if (!slash) {
    *parent_ino = SFUSE_ROOT_INO;
    name = dup;
  } else if (slash == dup) {
    *parent_ino = SFUSE_ROOT_INO;
    name = strdup(slash + 1);
    free(dup);
  } else {
    *slash = '\0';
    if (fs_resolve_path(get_fs_context(), dup, parent_ino) < 0) {
      free(dup);
      return NULL;
    }
    name = strdup(slash + 1);
    free(dup);
  }

  return name; // 호출한 곳에서 반드시 free(name)
}

// 누락된 필수 함수: fs_resolve_path 추가
int fs_resolve_path(struct sfuse_fs *fs, const char *path, uint32_t *ino_out) {
  uint32_t cur = SFUSE_ROOT_INO;
  if (!path || path[0] == '\0' || strcmp(path, "/") == 0) {
    *ino_out = cur;
    return 0;
  }
  // 경로를 '/' 구분자로 토큰 분리
  char *dup = strdup(path);
  if (!dup)
    return -ENOMEM;
  char *token = strtok(dup, "/");
  while (token) {
    // 현재 디렉터리의 모든 엔트리 로드
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
    size_t max_entries = buf_size / sizeof(*ents);
    uint32_t next_ino = 0;
    for (size_t i = 0; i < max_entries; i++) {
      if (ents[i].ino != 0 && strcmp(ents[i].name, token) == 0) {
        next_ino = ents[i].ino;
        break;
      }
    }
    free(buf);
    if (next_ino == 0) {
      free(dup);
      return -ENOENT;
    }
    cur = next_ino;
    token = strtok(NULL, "/");
  }
  free(dup);
  *ino_out = cur;
  return 0;
}

// 누락된 필수 함수: fs_destroy 추가
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
