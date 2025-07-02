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

  off_t device_size = lseek(backing_fd, 0, SEEK_END);
  uint32_t total_blocks = device_size / SFUSE_BLOCK_SIZE;

  if (sb_load(backing_fd, &fs->sb) < 0) {
    sb_format(&fs->sb);
    fs->sb.blocks_count = total_blocks;
    fs->sb.inodes_count = SFUSE_INODES_COUNT;

    uint32_t block_bitmap_blocks =
        (fs->sb.blocks_count / 8 + SFUSE_BLOCK_SIZE - 1) / SFUSE_BLOCK_SIZE;
    uint32_t inode_bitmap_blocks =
        (fs->sb.inodes_count / 8 + SFUSE_BLOCK_SIZE - 1) / SFUSE_BLOCK_SIZE;
    uint32_t inode_table_blocks =
        (fs->sb.inodes_count * sizeof(struct sfuse_inode) + SFUSE_BLOCK_SIZE -
         1) /
        SFUSE_BLOCK_SIZE;

    fs->sb.block_bitmap_start = 2;
    fs->sb.inode_bitmap_start = fs->sb.block_bitmap_start + block_bitmap_blocks;
    fs->sb.inode_table_start = fs->sb.inode_bitmap_start + inode_bitmap_blocks;
    fs->sb.data_block_start = fs->sb.inode_table_start + inode_table_blocks;

    fs->sb.free_blocks = total_blocks - fs->sb.data_block_start;
    fs->sb.free_inodes = fs->sb.inodes_count - 1;

    if (sb_sync(backing_fd, &fs->sb) < 0)
      return -EIO;

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

    uint32_t root = SFUSE_ROOT_INO;
    fs->inode_map[root / 8] |= (1 << (root % 8));
    fs->sb.free_inodes--;

    struct sfuse_inode root_inode;
    memset(&root_inode, 0, sizeof(root_inode));
    fs_init_inode(&fs->sb, root, S_IFDIR | 0755, fuse_get_context()->uid,
                  fuse_get_context()->gid, &root_inode);

    int blk_index = alloc_block(&fs->sb, fs->block_map);
    if (blk_index < 0)
      return -ENOSPC;

    uint32_t phys_block = fs->sb.data_block_start + blk_index;
    root_inode.direct[0] = phys_block;
    root_inode.size = SFUSE_BLOCK_SIZE;

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

    bitmap_sync(backing_fd, fs->sb.block_bitmap_start, fs->block_map,
                bmap_bytes);
    bitmap_sync(backing_fd, fs->sb.inode_bitmap_start, fs->inode_map,
                imap_bytes);
    sb_sync(backing_fd, &fs->sb);
  } else {
    size_t bmap_bytes = fs->sb.blocks_count / 8;
    size_t imap_bytes = fs->sb.inodes_count / 8;

    fs->block_map = calloc(1, bmap_bytes);
    fs->inode_map = calloc(1, imap_bytes);
    bitmap_load(backing_fd, fs->sb.block_bitmap_start, fs->block_map,
                bmap_bytes);
    bitmap_load(backing_fd, fs->sb.inode_bitmap_start, fs->inode_map,
                imap_bytes);
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
